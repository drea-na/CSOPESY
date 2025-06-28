// Process.h
#ifndef PROCESS_H
#define PROCESS_H

#include <string>
#include <fstream>
#include <vector>
#include <map>
#include <cstdint>

struct ProcessInfo {
    int id;
    std::string name;
    std::string startTime;
    int coreID;
    int progress;
    int total;
    bool finished;
};

class Process {
public:
    enum class InstrType { PRINT, DECLARE, ADD, SUBTRACT, SLEEP, FOR };

    struct Instruction {
        InstrType type;
        std::vector<std::string> args;
        std::vector<Instruction> body;
        int repeats = 0;
    };

    std::string name;
    int totalCommands = 100;
    int executedCommands = 0;
    std::ofstream logFile;

    std::vector<Instruction> instructions;
    std::map<std::string, uint16_t> variables;
    int currentInstr = 0;
    int sleepTicks = 0;
    int forDepth = 0;

    Process(const std::string& processName);
    Process(const std::string& logPath, bool isPath);
    ~Process();

    bool executeNextInstruction(int coreId, int& cpuTick);
    void generateRandomInstructions(int minIns, int maxIns);

private:
    void setVariable(const std::string& var, int value);
    int getValueFromArg(const std::string& arg);

};

#endif
