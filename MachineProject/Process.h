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
#include <algorithm>
#include "PageTable.h"

struct ProcessInfo {
    int id;
    std::string name;
    std::string startTime;
    int coreID;
    int progress;
    int total;
    bool finished;
    bool memoryAccessViolation;
    std::string violationAddress;
    std::string violationTime;

    // Constructor for initialization
    ProcessInfo(int pid, const std::string& pname)
        : id(pid), name(pname), coreID(-1), progress(0), total(100), finished(false), 
          memoryAccessViolation(false) {
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
    std::map<int, uint16_t> memory; // Memory map: address -> value
    int currentInstr = 0;
    int sleepTicks = 0;
    bool isFinished = false;
    bool memoryAccessViolation = false;
    std::string violationAddress;
    std::string violationTime;
    
    // Page table for demand paging
    PageTable* pageTable;

    // Constructor
    Process(const std::string& processName) : name(processName), totalCommands(0), executedCommands(0), pageTable(nullptr) {
        // Create logs directory if it doesn't exist
        system("mkdir process_logs 2>nul"); // Windows command, silent error
        
        std::string logPath = "process_logs/" + name + ".txt";
        logFile.open(logPath, std::ios::out);
        
        if (!logFile.is_open()) {
            throw std::runtime_error("Failed to open log file: " + logPath);
        }

        // Initialize page table for demand paging
        pageTable = new PageTable(processName);

        // random instructions will be generated here later with specific parameters
    }

    ~Process() {
        if (logFile.is_open())
            logFile.close();
        if (pageTable) {
            delete pageTable;
        }
    }

    // Main execution method - returns true if process should continue
    bool executeNextInstruction(int coreId);

    // Generate random instructions for the process
    void generateRandomInstructions(int minIns, int maxIns);
    
    // Parse custom instructions
    void parseCustomInstructions(const std::string& instructionString);

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
    
    // Memory access helper functions
    int parseHexAddress(const std::string& hexStr) const;
    bool isValidMemoryAddress(int address) const;
    uint16_t readMemory(int address);
    void writeMemory(int address, uint16_t value);
    std::string getCurrentTimestamp() const;
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

inline std::string Process::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm_time;
    localtime_s(&tm_time, &time);
    std::stringstream ss;
    ss << std::put_time(&tm_time, "%H:%M:%S");
    return ss.str();
}

inline int Process::parseHexAddress(const std::string& hexStr) const {
    try {
        // Remove "0x" prefix if present
        std::string cleanHex = hexStr;
        if (cleanHex.substr(0, 2) == "0x" || cleanHex.substr(0, 2) == "0X") {
            cleanHex = cleanHex.substr(2);
        }
        
        // Convert hex string to integer
        return std::stoi(cleanHex, nullptr, 16);
    } catch (const std::exception& e) {
        throw std::runtime_error("Invalid hexadecimal address: " + hexStr);
    }
}

inline bool Process::isValidMemoryAddress(int address) const {
    // Memory addresses must be non-negative and within reasonable bounds
    // For this implementation, we'll use a simple range check
    return address >= 0 && address <= 0xFFFF; // 16-bit address space
}

inline uint16_t Process::readMemory(int address) {
    // Use page table for address translation if available
    if (pageTable) {
        uint32_t physicalAddr;
        if (pageTable->translateAddress(address, physicalAddr)) {
            // Address translation successful, check if we have a value
            auto it = memory.find(physicalAddr);
            if (it != memory.end()) {
                return it->second;
            }
            return 0; // Valid address but no value stored
        } else {
            // Page fault - try to handle it
            if (pageTable->handlePageFault(address, physicalAddr)) {
                // Page fault handled successfully
                auto it = memory.find(physicalAddr);
                if (it != memory.end()) {
                    return it->second;
                }
                return 0;
            } else {
                // Page fault handling failed
                memoryAccessViolation = true;
                violationAddress = "0x" + std::to_string(address);
                violationTime = getCurrentTimestamp();
                throw std::runtime_error("Memory access violation: Page fault handling failed for address " + violationAddress);
            }
        }
    }
    
    // Fallback to direct memory access if no page table
    if (!isValidMemoryAddress(address)) {
        memoryAccessViolation = true;
        violationAddress = "0x" + std::to_string(address);
        violationTime = getCurrentTimestamp();
        throw std::runtime_error("Memory access violation: Invalid read address 0x" + std::to_string(address));
    }
    
    // If address is not initialized, return 0 (as per specification)
    auto it = memory.find(address);
    if (it == memory.end()) {
        return 0;
    }
    return it->second;
}

inline void Process::writeMemory(int address, uint16_t value) {
    // Use page table for address translation if available
    if (pageTable) {
        uint32_t physicalAddr;
        if (pageTable->translateAddress(address, physicalAddr)) {
            // Address translation successful, write to physical address
            memory[physicalAddr] = value;
            pageTable->setPageDirty(pageTable->virtualToPageNumber(address));
            return;
        } else {
            // Page fault - try to handle it
            if (pageTable->handlePageFault(address, physicalAddr)) {
                // Page fault handled successfully, write to physical address
                memory[physicalAddr] = value;
                pageTable->setPageDirty(pageTable->virtualToPageNumber(address));
                return;
            } else {
                // Page fault handling failed
                memoryAccessViolation = true;
                violationAddress = "0x" + std::to_string(address);
                violationTime = getCurrentTimestamp();
                throw std::runtime_error("Memory access violation: Page fault handling failed for address " + violationAddress);
            }
        }
    }
    
    // Fallback to direct memory access if no page table
    if (!isValidMemoryAddress(address)) {
        memoryAccessViolation = true;
        violationAddress = "0x" + std::to_string(address);
        violationTime = getCurrentTimestamp();
        throw std::runtime_error("Memory access violation: Invalid write address 0x" + std::to_string(address));
    }
    
    memory[address] = value;
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
            if (instr.args.size() >= 2) {
                try {
                    int address = parseHexAddress(instr.args[1]);
                    uint16_t value = readMemory(address);
                    setVariable(instr.args[0], value);
                    logMessage("READ: " + instr.args[0] + " = " + std::to_string(value) + " from address " + instr.args[1], coreId);
                } catch (const std::exception& e) {
                    logMessage("Memory access violation: " + std::string(e.what()), coreId);
                    throw; // Re-throw to trigger process shutdown
                }
            }
            break;
        }
        case InstrType::WRITE: {
            if (instr.args.size() >= 2) {
                try {
                    int address = parseHexAddress(instr.args[0]);
                    uint16_t value = static_cast<uint16_t>(std::stoi(instr.args[1]));
                    writeMemory(address, value);
                    logMessage("WRITE: " + std::to_string(value) + " to address " + instr.args[0], coreId);
                } catch (const std::exception& e) {
                    logMessage("Memory access violation: " + std::string(e.what()), coreId);
                    throw; // Re-throw to trigger process shutdown
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
    std::uniform_int_distribution<> typeDist(0, 6); // Updated to include READ and WRITE
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
            instr.args.push_back(varNames[varDist(gen)]); // variable to store result
            // Generate a random hex address (sometimes invalid to test violations)
            {
                std::uniform_int_distribution<> addrDist(0, 0x1FFFF); // Extended range to include invalid addresses
                int addr = addrDist(gen);
                std::stringstream ss;
                ss << "0x" << std::hex << addr;
                instr.args.push_back(ss.str());
            }
            break;
        case 6: // WRITE
            instr.type = InstrType::WRITE;
            // Generate a random hex address (sometimes invalid to test violations)
            {
                std::uniform_int_distribution<> addrDist(0, 0x1FFFF); // Extended range to include invalid addresses
                int addr = addrDist(gen);
                std::stringstream ss;
                ss << "0x" << std::hex << addr;
                instr.args.push_back(ss.str());
            }
            instr.args.push_back(std::to_string(valDist(gen))); // value to write
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

inline void Process::parseCustomInstructions(const std::string& instructionString) {
    instructions.clear();
    
    // Split by semicolon
    std::stringstream ss(instructionString);
    std::string instruction;
    std::vector<std::string> instructionList;
    
    while (std::getline(ss, instruction, ';')) {
        // Trim whitespace
        instruction.erase(0, instruction.find_first_not_of(" \t"));
        instruction.erase(instruction.find_last_not_of(" \t") + 1);
        
        if (!instruction.empty()) {
            instructionList.push_back(instruction);
        }
    }
    
    // Validate instruction count (1-50)
    if (instructionList.size() < 1 || instructionList.size() > 50) {
        throw std::runtime_error("Invalid instruction count. Must be between 1 and 50 instructions.");
    }
    
    // Parse each instruction
    for (const auto& instrStr : instructionList) {
        Instruction instr;
        std::stringstream instrStream(instrStr);
        std::string command;
        instrStream >> command;
        
        // Convert to uppercase for comparison
        std::transform(command.begin(), command.end(), command.begin(), ::toupper);
        
        if (command == "PRINT") {
            instr.type = InstrType::PRINT;
            std::string arg;
            if (instrStream >> arg) {
                instr.args.push_back(arg);
            }
        }
        else if (command == "DECLARE") {
            instr.type = InstrType::DECLARE;
            std::string var, val;
            if (instrStream >> var >> val) {
                instr.args.push_back(var);
                instr.args.push_back(val);
            }
        }
        else if (command == "ADD") {
            instr.type = InstrType::ADD;
            std::string result, op1, op2;
            if (instrStream >> result >> op1 >> op2) {
                instr.args.push_back(result);
                instr.args.push_back(op1);
                instr.args.push_back(op2);
            }
        }
        else if (command == "SUBTRACT") {
            instr.type = InstrType::SUBTRACT;
            std::string result, op1, op2;
            if (instrStream >> result >> op1 >> op2) {
                instr.args.push_back(result);
                instr.args.push_back(op1);
                instr.args.push_back(op2);
            }
        }
        else if (command == "SLEEP") {
            instr.type = InstrType::SLEEP;
            std::string ticks;
            if (instrStream >> ticks) {
                instr.args.push_back(ticks);
            }
        }
        else if (command == "READ") {
            instr.type = InstrType::READ;
            std::string var, addr;
            if (instrStream >> var >> addr) {
                instr.args.push_back(var);
                instr.args.push_back(addr);
            }
        }
        else if (command == "WRITE") {
            instr.type = InstrType::WRITE;
            std::string addr, val;
            if (instrStream >> addr >> val) {
                instr.args.push_back(addr);
                instr.args.push_back(val);
            }
        }
        else {
            throw std::runtime_error("Unknown instruction: " + command);
        }
        
        instructions.push_back(instr);
    }
    
    totalCommands = instructions.size();
    executedCommands = 0;
    currentInstr = 0;
    sleepTicks = 0;
    isFinished = false;
}

#endif