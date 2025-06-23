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
#include <functional>

struct ProcessInfo {
    std::string name;
    std::string startTime;
    int coreID;
    int progress;
    int total;
    bool finished;
};

class Process {
public:
    std::string name;
    int totalCommands = 100;
    int executedCommands = 0;
    std::ofstream logFile;

    // Instruction types
    enum class InstrType {
        PRINT,
        DECLARE,
        ADD,
        SUBTRACT,
        SLEEP,
        FOR
    };

    struct Instruction {
        InstrType type;
        std::vector<std::string> args; // Arguments (variable names, values, etc.)
        std::vector<Instruction> body; // For FOR loops
        int repeats = 0; // For FOR loops
    };

    std::vector<Instruction> instructions; // List of instructions
    std::map<std::string, uint16_t> variables; // Variable storage
    int currentInstr = 0; // Current instruction index
    int sleepTicks = 0; // Remaining sleep ticks
    int forDepth = 0; // Track FOR nesting

    Process(const std::string& processName) : name(processName) {
        logFile.open(name + ".txt", std::ios::out);
    }

    // New constructor for custom log path
    Process(const std::string& logPath, bool isPath) : name(logPath) {
        logFile.open(logPath + ".txt", std::ios::out);
    }

    ~Process() {
        if (logFile.is_open())
            logFile.close();
    }

    void logPrint(int coreId) {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::tm tm_time;
        localtime_s(&tm_time, &time);
        std::stringstream ss;
        ss << "(" << std::put_time(&tm_time, "%m/%d/%Y %I:%M:%S%p")
            << ") Core:" << coreId << " \"Hello world from " << name << "!\"";
        std::string logLine = ss.str();
        logFile << logLine << std::endl;
        //std::cout << logLine << std::endl;
    }

    // New methods for instruction execution
    bool executeNextInstruction(int coreId, int& cpuTick);
    void generateRandomInstructions(int minIns, int maxIns);

    // Helper for variable clamping
    void setVariable(const std::string& var, int value);
};

// Implementation of Process methods
inline void Process::setVariable(const std::string& var, int value) {
    if (value < 0) value = 0;
    if (value > 65535) value = 65535;
    variables[var] = static_cast<uint16_t>(value);
}

inline bool Process::executeNextInstruction(int coreId, int& cpuTick) {
    // If sleeping, decrement sleepTicks and return
    if (sleepTicks > 0) {
        --sleepTicks;
        return true;
    }
    if (currentInstr >= instructions.size()) {
        return false; // Finished
    }
    Instruction& instr = instructions[currentInstr];
    switch (instr.type) {
        case InstrType::PRINT: {
            std::string msg = "Hello world from " + name + "!";
            if (!instr.args.empty()) {
                // If variable is specified, print its value
                std::string var = instr.args[0];
                uint16_t val = 0;
                if (variables.count(var)) val = variables[var];
                msg = "Hello world from " + name + "! " + var + "=" + std::to_string(val);
            }
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            std::tm tm_time;
            localtime_s(&tm_time, &time);
            std::stringstream ss;
            ss << "(" << std::put_time(&tm_time, "%m/%d/%Y %I:%M:%S%p")
                << ") Core:" << coreId << " " << msg;
            logFile << ss.str() << std::endl;
            break;
        }
        case InstrType::DECLARE: {
            if (instr.args.size() >= 2) {
                std::string var = instr.args[0];
                int val = std::stoi(instr.args[1]);
                setVariable(var, val);
            }
            break;
        }
        case InstrType::ADD: {
            if (instr.args.size() >= 3) {
                std::string var1 = instr.args[0];
                int v2 = 0, v3 = 0;
                // var2/value
                if (variables.count(instr.args[1])) v2 = variables[instr.args[1]];
                else {
                    try { v2 = std::stoi(instr.args[1]); } catch (...) { v2 = 0; }
                }
                // var3/value
                if (variables.count(instr.args[2])) v3 = variables[instr.args[2]];
                else {
                    try { v3 = std::stoi(instr.args[2]); } catch (...) { v3 = 0; }
                }
                setVariable(var1, v2 + v3);
            }
            break;
        }
        case InstrType::SUBTRACT: {
            if (instr.args.size() >= 3) {
                std::string var1 = instr.args[0];
                int v2 = 0, v3 = 0;
                if (variables.count(instr.args[1])) v2 = variables[instr.args[1]];
                else {
                    try { v2 = std::stoi(instr.args[1]); } catch (...) { v2 = 0; }
                }
                if (variables.count(instr.args[2])) v3 = variables[instr.args[2]];
                else {
                    try { v3 = std::stoi(instr.args[2]); } catch (...) { v3 = 0; }
                }
                setVariable(var1, v2 - v3);
            }
            break;
        }
        case InstrType::SLEEP: {
            if (!instr.args.empty()) {
                int ticks = std::stoi(instr.args[0]);
                if (ticks > 0) {
                    sleepTicks = ticks - 1; // Already spent 1 tick
                }
            }
            break;
        }
        case InstrType::FOR: {
            if (forDepth >= 3) break; // Max 3-level nesting
            int repeats = instr.repeats;
            for (int r = 0; r < repeats; ++r) {
                ++forDepth;
                for (auto& subInstr : instr.body) {
                    // Recursively execute sub-instructions
                    Instruction temp = subInstr;
                    instructions.insert(instructions.begin() + currentInstr + 1, temp);
                }
                --forDepth;
            }
            break;
        }
    }
    ++currentInstr;
    ++executedCommands;
    return currentInstr < instructions.size();
}

inline void Process::generateRandomInstructions(int minIns, int maxIns) {
    static const std::vector<std::string> varNames = {"a", "b", "c", "d", "e"};
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> insDist(minIns, maxIns);
    std::uniform_int_distribution<> typeDist(0, 5); // 6 types
    std::uniform_int_distribution<> varDist(0, varNames.size() - 1);
    std::uniform_int_distribution<> valDist(0, 1000);
    std::uniform_int_distribution<> sleepDist(1, 10);
    std::uniform_int_distribution<> forRepeatDist(1, 5);
    std::uniform_int_distribution<> forBodyDist(1, 3);

    int numInstructions = insDist(gen);
    instructions.clear();

    std::function<Instruction(int)> makeInstruction;
    makeInstruction = [&](int depth) -> Instruction {
        int t = typeDist(gen);
        Instruction instr;
        if (t == 0) { // PRINT
            instr.type = InstrType::PRINT;
            if (varDist(gen) % 2 == 0) // 50% chance to print a variable
                instr.args.push_back(varNames[varDist(gen)]);
        } else if (t == 1) { // DECLARE
            instr.type = InstrType::DECLARE;
            instr.args.push_back(varNames[varDist(gen)]);
            instr.args.push_back(std::to_string(valDist(gen)));
        } else if (t == 2) { // ADD
            instr.type = InstrType::ADD;
            instr.args.push_back(varNames[varDist(gen)]); // result
            // arg2
            if (varDist(gen) % 2 == 0)
                instr.args.push_back(varNames[varDist(gen)]);
            else
                instr.args.push_back(std::to_string(valDist(gen)));
            // arg3
            if (varDist(gen) % 2 == 0)
                instr.args.push_back(varNames[varDist(gen)]);
            else
                instr.args.push_back(std::to_string(valDist(gen)));
        } else if (t == 3) { // SUBTRACT
            instr.type = InstrType::SUBTRACT;
            instr.args.push_back(varNames[varDist(gen)]); // result
            // arg2
            if (varDist(gen) % 2 == 0)
                instr.args.push_back(varNames[varDist(gen)]);
            else
                instr.args.push_back(std::to_string(valDist(gen)));
            // arg3
            if (varDist(gen) % 2 == 0)
                instr.args.push_back(varNames[varDist(gen)]);
            else
                instr.args.push_back(std::to_string(valDist(gen)));
        } else if (t == 4) { // SLEEP
            instr.type = InstrType::SLEEP;
            instr.args.push_back(std::to_string(sleepDist(gen)));
        } else if (t == 5 && depth < 3) { // FOR
            instr.type = InstrType::FOR;
            instr.repeats = forRepeatDist(gen);
            int bodyLen = forBodyDist(gen);
            for (int i = 0; i < bodyLen; ++i) {
                instr.body.push_back(makeInstruction(depth + 1));
            }
        } else { // fallback to PRINT
            instr.type = InstrType::PRINT;
        }
        return instr;
    };

    for (int i = 0; i < numInstructions; ++i) {
        instructions.push_back(makeInstruction(0));
    }
    totalCommands = numInstructions;
    executedCommands = 0;
    currentInstr = 0;
    sleepTicks = 0;
}

#endif
