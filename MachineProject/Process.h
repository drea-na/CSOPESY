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

// Include MemoryManager header
#include "MemoryManager.h"

struct ProcessInfo {
    int id;
    std::string name;
    std::string startTime;
    int coreID;
    int progress;
    int total;
    bool finished;

    // Constructor for initialization
    ProcessInfo(int pid, const std::string& pname)
        : id(pid), name(pname), coreID(-1), progress(0), total(100), finished(false) {
        // Get current timestamp
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
        InstrType type;
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
    
    // Memory management
    int processId;
    int allocatedMemory;
    MemoryManager* memoryManager;

    // Constructor
    Process(const std::string& processName, int pid = -1, MemoryManager* mm = nullptr, int memSize = 0) 
        : name(processName), totalCommands(0), executedCommands(0), 
          processId(pid), allocatedMemory(memSize), memoryManager(mm) {
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
    
    // Add custom instruction
    void addInstruction(InstrType type, const std::vector<std::string>& args);

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
    } catch (const std::exception& e) {
    }
}

inline void Process::addInstruction(InstrType type, const std::vector<std::string>& args) {
    Instruction instr;
    instr.type = type;
    instr.args = args;
    instructions.push_back(instr);
    totalCommands++;
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
            if (instr.args.size() >= 2) {
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
            if (instr.args.size() >= 2 && memoryManager) {
                try {
                    std::string varName = instr.args[0];
                    uint32_t address = std::stoul(instr.args[1], nullptr, 16); // Parse hex address
                    uint16_t value = memoryManager->readMemory(processId, address);
                    setVariable(varName, value);
                    logMessage("READ " + varName + " = " + std::to_string(value) + " from 0x" + instr.args[1], coreId);
                } catch (const std::exception& e) {
                    logMessage("Memory access violation: " + std::string(e.what()), coreId);
                    isFinished = true;
                    return false;
                }
            }
            break;
        }
        case InstrType::WRITE: {
            if (instr.args.size() >= 2 && memoryManager) {
                try {
                    uint32_t address = std::stoul(instr.args[0], nullptr, 16); // Parse hex address
                    uint16_t value = std::stoul(instr.args[1]);
                    if (memoryManager->writeMemory(processId, address, value)) {
                        logMessage("WRITE " + std::to_string(value) + " to 0x" + instr.args[0], coreId);
                    } else {
                        logMessage("Memory access violation: failed to write to 0x" + instr.args[0], coreId);
                        isFinished = true;
                        return false;
                    }
                } catch (const std::exception& e) {
                    logMessage("Memory access violation: " + std::string(e.what()), coreId);
                    isFinished = true;
                    return false;
                }
            }
            break;
        }
        }

        ++currentInstr;
        ++executedCommands;
        return currentInstr < instructions.size();
    } catch (const std::exception& e) {
        logMessage("Error executing instruction: " + std::string(e.what()), coreId);
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
    std::uniform_int_distribution<> typeDist(0, 6); // Now includes READ and WRITE
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

        case 5: // READ
            instr.type = InstrType::READ;
            instr.args.push_back(varNames[varDist(gen)]); // variable name
            instr.args.push_back("0x" + std::to_string(valDist(gen) * 16)); // hex address
            break;

        case 6: // WRITE
            instr.type = InstrType::WRITE;
            instr.args.push_back("0x" + std::to_string(valDist(gen) * 16)); // hex address
            instr.args.push_back(std::to_string(valDist(gen))); // value
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