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
    MemoryManager* memoryManager = nullptr; // Reference to memory manager

    enum class InstrType {
        PRINT,
        DECLARE,
        ADD,
        SUBTRACT,
        SLEEP,
        FOR,
        READ,   // New
        WRITE   // New
    };

    struct Instruction {
        InstrType type;
        std::vector<std::string> args;
        std::vector<Instruction> body; // For FOR loops only
        int repeats = 0;
    };

    // Core data
    std::vector<Instruction> instructions;
    // Symbol table: 32 uint16_t variables (64 bytes)
    uint16_t symbolTable[32] = {0};
    std::map<std::string, int> symbolIndex; // variable name -> index in symbolTable
    int symbolCount = 0;
    int currentInstr = 0;
    int sleepTicks = 0;
    bool isFinished = false;

    // Constructor
    Process(const std::string& processName, MemoryManager* memMgr = nullptr) : name(processName), totalCommands(0), executedCommands(0), memoryManager(memMgr) {
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

    // New: symbol table helpers
    int getSymbolIndex(const std::string& var) {
        if (symbolIndex.count(var)) return symbolIndex[var];
        if (symbolCount >= 32) throw std::runtime_error("Symbol table full");
        symbolIndex[var] = symbolCount;
        return symbolCount++;
    }
    void setSymbol(const std::string& var, uint16_t value) {
        int idx = getSymbolIndex(var);
        symbolTable[idx] = value;
    }
    uint16_t getSymbol(const std::string& var) {
        int idx = getSymbolIndex(var);
        return symbolTable[idx];
    }

    // Parse user instructions from a semicolon-separated string
    void parseUserInstructions(const std::string& instrStr) {
        instructions.clear();
        size_t start = 0, end = 0;
        while ((end = instrStr.find(';', start)) != std::string::npos) {
            std::string instr = instrStr.substr(start, end - start);
            parseSingleInstruction(instr);
            start = end + 1;
        }
        if (start < instrStr.size()) {
            parseSingleInstruction(instrStr.substr(start));
        }
        totalCommands = instructions.size();
        executedCommands = 0;
        currentInstr = 0;
        sleepTicks = 0;
        isFinished = false;
    }

private:
    void setVariable(const std::string& var, int value);
    void logMessage(const std::string& message, int coreId);
    void parseSingleInstruction(const std::string& instr);
};

inline void Process::setVariable(const std::string& var, int value) {
    if (value < 0) value = 0;
    if (value > 65535) value = 65535;
    // This function is no longer used for symbol table, but kept for compatibility
    // if (variables.count(var)) {
    //     variables[var] = static_cast<uint16_t>(value);
    // } else {
    //     // This case should ideally not happen if setSymbol is used consistently
    //     // For now, we'll just set it to 0 or throw an error
    //     // throw std::runtime_error("Variable not found in symbol table: " + var);
    // }
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

inline bool Process::executeNextInstruction(int coreId) {
    // Handle sleep state
    if (sleepTicks > 0) {
        --sleepTicks;
        return true;
    }
    if (currentInstr >= instructions.size()) {
        isFinished = true;
        return false;
    }
    try {
        const Instruction& instr = instructions[currentInstr];
        switch (instr.type) {
        case InstrType::PRINT: {
            std::string msg = "Hello world from " + name + "!";
            if (!instr.args.empty() && symbolIndex.count(instr.args[0])) {
                msg += " " + instr.args[0] + "=" + std::to_string(getSymbol(instr.args[0]));
            }
            logMessage(msg, coreId);
            break;
        }
        case InstrType::DECLARE: {
            if (instr.args.size() >= 2) {
                int val = std::stoi(instr.args[1]);
                setSymbol(instr.args[0], val);
            }
            break;
        }
        case InstrType::ADD: {
            if (instr.args.size() >= 3) {
                int v2 = symbolIndex.count(instr.args[1]) ? getSymbol(instr.args[1]) : std::stoi(instr.args[1]);
                int v3 = symbolIndex.count(instr.args[2]) ? getSymbol(instr.args[2]) : std::stoi(instr.args[2]);
                setSymbol(instr.args[0], v2 + v3);
            }
            break;
        }
        case InstrType::SUBTRACT: {
            if (instr.args.size() >= 3) {
                int v2 = symbolIndex.count(instr.args[1]) ? getSymbol(instr.args[1]) : std::stoi(instr.args[1]);
                int v3 = symbolIndex.count(instr.args[2]) ? getSymbol(instr.args[2]) : std::stoi(instr.args[2]);
                setSymbol(instr.args[0], v2 - v3);
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
            // READ(var, address)
            if (instr.args.size() >= 2 && memoryManager) {
                const std::string& var = instr.args[0];
                int address = std::stoi(instr.args[1], nullptr, 16); // hex address
                uint16_t value = 0;
                bool ok = memoryManager->accessMemory(name, address, false, &value);
                if (!ok) {
                    logMessage("Memory access violation at address " + instr.args[1], coreId);
                    isFinished = true;
                    return false;
                }
                setSymbol(var, value);
            }
            break;
        }
        case InstrType::WRITE: {
            // WRITE(address, value)
            if (instr.args.size() >= 2 && memoryManager) {
                int address = std::stoi(instr.args[0], nullptr, 16); // hex address
                uint16_t value = 0;
                if (symbolIndex.count(instr.args[1])) {
                    value = getSymbol(instr.args[1]);
                } else {
                    value = static_cast<uint16_t>(std::stoi(instr.args[1]));
                }
                bool ok = memoryManager->accessMemory(name, address, true, &value);
                if (!ok) {
                    logMessage("Memory access violation at address " + instr.args[0], coreId);
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