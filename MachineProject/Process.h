#ifndef PROCESS_H
#define PROCESS_H


#include <string>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <vector>
#include <map>
#include <random>
#include <cstdint>
#include <stdexcept>
#include "MemoryManager.h"
#include <ctime>

struct ProcessInfo {
    int id;
    std::string name;
    std::string startTime;
    int coreID;
    int progress;
    int total;
    bool finished;
    // For memory access violation reporting
    bool accessViolation = false;
    uint32_t violationAddress = 0;
    std::string violationTime;
    // Default constructor
    ProcessInfo() : id(-1), name(""), coreID(-1), progress(0), total(0), finished(false) {}
    // Main constructor
    ProcessInfo(int pid, const std::string& pname)
        : id(pid), name(pname), coreID(-1), progress(0), total(100), finished(false) {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::tm tm_time;
        localtime_s(&tm_time, &time);
        std::stringstream ss;
        ss << std::put_time(&tm_time, "%m/%d/%Y %I:%M:%S%p");
        startTime = ss.str();
    }
};

class Process {
public:
    std::string name;
    int totalCommands;
    int executedCommands;
    std::ofstream logFile;
    MemoryManager* memoryManager = nullptr; // Pointer to memory manager
    int memorySize = 0; // Allocated memory size for this process
    bool accessViolation = false; // Set to true if process is killed by memory violation
    uint32_t violationAddress = 0; // Offending address

    enum class InstrType {
        PRINT,
        DECLARE,
        ADD,
        SUBTRACT,
        SLEEP,
        FOR,
        READ,
        WRITE
    };

    struct Instruction {
        InstrType type = InstrType::PRINT;
        std::vector<std::string> args;
        std::vector<Instruction> body; // For FOR loops only
        int repeats = 0;
    };

    // Core data
    std::vector<Instruction> instructions;
    std::map<std::string, uint16_t> variables;
    int currentInstr = 0;
    int sleepTicks = 0;
    bool isFinished = false;

    // Constructor
    Process(const std::string& processName) : name(processName), totalCommands(0), executedCommands(0) {
        // Create logs directory if it doesn't exist
        system("mkdir process_logs 2>nul"); // Windows command, silent error
        
        std::string logPath = "process_logs/" + name + ".txt";
        logFile.open(logPath, std::ios::out);
        
        if (!logFile.is_open()) {
            throw std::runtime_error("Failed to open log file: " + logPath);
        }

        // random instructions will be generated here later with specific parameters
    }

    ~Process() {
        if (logFile.is_open())
            logFile.close();
    }

    // Main execution method - returns true if process should continue
    bool executeNextInstruction(int coreId);

    // Generate random instructions for the process
    void generateRandomInstructions(int minIns, int maxIns);

    // Check if process is complete
    bool isComplete() const {
        return currentInstr >= instructions.size() && sleepTicks == 0;
    }

    // Get progress percentage
    double getProgress() const {
        if (instructions.empty()) return 0.0;
        return (double)executedCommands / instructions.size() * 100.0;
    }

private:
    void setVariable(const std::string& var, int value);
    void logMessage(const std::string& message, int coreId);
};

inline void Process::setVariable(const std::string& var, int value) {
    if (value < 0) value = 0;
    if (value > 65535) value = 65535;
    variables[var] = static_cast<uint16_t>(value);
}

inline void Process::logMessage(const std::string& message, int coreId) {
    if (!logFile.is_open()) {
        return;
    }
    
    try {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::tm tm_time;
        localtime_s(&tm_time, &time);
        std::stringstream ss;
        ss << "(" << std::put_time(&tm_time, "%m/%d/%Y %I:%M:%S%p")
            << ") Core:" << coreId << " \"" << message << "\"";
        logFile << ss.str() << std::endl;
        logFile.flush();
    } catch (const std::exception&) {
    }
}

inline bool Process::executeNextInstruction(int coreId) {
    // Handle sleep state
    if (sleepTicks > 0) {
        --sleepTicks;
        return true; // Still running, just sleeping
    }
    // Check if finished
    if (currentInstr >= instructions.size()) {
        isFinished = true;
        return false;
    }
    try {
        const Instruction& instr = instructions[currentInstr];
        switch (instr.type) {
        case InstrType::PRINT: {
            std::string msg = "Hello world from " + name + "!";
            if (!instr.args.empty() && variables.count(instr.args[0])) {
                msg += " " + instr.args[0] + "=" + std::to_string(variables[instr.args[0]]);
            }
            logMessage(msg, coreId);
            break;
        }
        case InstrType::DECLARE: {
            if (instr.args.size() >= 2 && variables.size() < 32) { // Enforce symbol table size
                int val = std::stoi(instr.args[1]);
                setVariable(instr.args[0], val);
            }
            break;
        }
        case InstrType::ADD: {
            if (instr.args.size() >= 3) {
                int v2 = variables.count(instr.args[1]) ? variables[instr.args[1]] : std::stoi(instr.args[1]);
                int v3 = variables.count(instr.args[2]) ? variables[instr.args[2]] : std::stoi(instr.args[2]);
                setVariable(instr.args[0], v2 + v3);
            }
            break;
        }
        case InstrType::SUBTRACT: {
            if (instr.args.size() >= 3) {
                int v2 = variables.count(instr.args[1]) ? variables[instr.args[1]] : std::stoi(instr.args[1]);
                int v3 = variables.count(instr.args[2]) ? variables[instr.args[2]] : std::stoi(instr.args[2]);
                setVariable(instr.args[0], v2 - v3);
            }
            break;
        }
        case InstrType::SLEEP: {
            if (!instr.args.empty()) {
                sleepTicks = std::stoi(instr.args[0]);
            }
            break;
        }
        case InstrType::FOR: {
            for (int i = 0; i < instr.repeats; ++i) {
                for (const auto& bodyInstr : instr.body) {
                    logMessage("Executing FOR loop iteration " + std::to_string(i + 1), coreId);
                }
            }
            break;
        }
        case InstrType::READ: {
            // READ var memory_address
            if (instr.args.size() >= 2 && memoryManager) {
                std::string var = instr.args[0];
                uint32_t addr = std::stoul(instr.args[1], nullptr, 0);
                uint16_t value = 0;
                if (!memoryManager->readMemory(name, memorySize, addr, value)) {
                    accessViolation = true;
                    violationAddress = addr;
                    isFinished = true;
                    logMessage("Memory access violation at address 0x" + std::to_string(addr), coreId);
                    return false;
                }
                setVariable(var, value);
            }
            break;
        }
        case InstrType::WRITE: {
            // WRITE memory_address value
            if (instr.args.size() >= 2 && memoryManager) {
                uint32_t addr = std::stoul(instr.args[0], nullptr, 0);
                int val = 0;
                if (variables.count(instr.args[1])) val = variables[instr.args[1]];
                else val = std::stoi(instr.args[1]);
                if (!memoryManager->writeMemory(name, memorySize, addr, (uint16_t)val)) {
                    accessViolation = true;
                    violationAddress = addr;
                    isFinished = true;
                    logMessage("Memory access violation at address 0x" + std::to_string(addr), coreId);
                    return false;
                }
            }
            break;
        }
        }
        ++currentInstr;
        ++executedCommands;
        return currentInstr < instructions.size();
    } catch (const std::exception&) {
        logMessage("Error executing instruction", coreId);
        ++currentInstr;
        ++executedCommands;
        return currentInstr < instructions.size();
    }
}

inline void Process::generateRandomInstructions(int minIns, int maxIns) {
    static const std::vector<std::string> varNames = { "a", "b", "c", "d", "e" };
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> insDist(minIns, maxIns);
    std::uniform_int_distribution<> typeDist(0, 4);
    std::uniform_int_distribution<> varDist(0, varNames.size() - 1);
    std::uniform_int_distribution<> valDist(1, 100);
    std::uniform_int_distribution<> sleepDist(1, 5);

    int numInstructions = insDist(gen);
    instructions.clear();
    instructions.reserve(numInstructions);

    for (int i = 0; i < numInstructions; ++i) {
        Instruction instr;
        int type = typeDist(gen);

        switch (type) {
        case 0: // PRINT
            instr.type = InstrType::PRINT;
            if (gen() % 3 == 0) { // 33% chance to print a variable
                instr.args.push_back(varNames[varDist(gen)]);
            }
            break;

        case 1: // DECLARE
            instr.type = InstrType::DECLARE;
            instr.args.push_back(varNames[varDist(gen)]);
            instr.args.push_back(std::to_string(valDist(gen)));
            break;

        case 2: // ADD
            instr.type = InstrType::ADD;
            instr.args.push_back(varNames[varDist(gen)]); // result variable
            instr.args.push_back(std::to_string(valDist(gen))); // first operand
            instr.args.push_back(std::to_string(valDist(gen))); // second operand
            break;

        case 3: // SUBTRACT
            instr.type = InstrType::SUBTRACT;
            instr.args.push_back(varNames[varDist(gen)]);
            instr.args.push_back(std::to_string(valDist(gen)));
            instr.args.push_back(std::to_string(valDist(gen)));
            break;

        case 4: // SLEEP
            instr.type = InstrType::SLEEP;
            instr.args.push_back(std::to_string(sleepDist(gen)));
            break;
        }

        instructions.push_back(instr);
    }

    totalCommands = numInstructions;
    executedCommands = 0;
    currentInstr = 0;
    sleepTicks = 0;
    isFinished = false;
};

#endif