#include "Process.h"
#include <chrono>
#include <sstream>
#include <iomanip>
#include <random>
#include <thread>
#include <functional>

Process::Process(const std::string& processName) : name(processName) {
    logFile.open(name + ".txt", std::ios::out);
}

Process::Process(const std::string& logPath, bool isPath) : name(logPath) {
    logFile.open(logPath + ".txt", std::ios::out);
}

Process::~Process() {
    if (logFile.is_open())
        logFile.close();
}

void Process::setVariable(const std::string& var, int value) {
    if (value < 0) value = 0;
    if (value > 65535) value = 65535;
    variables[var] = static_cast<uint16_t>(value);
}

int Process::getValueFromArg(const std::string& arg) {
    if (variables.count(arg)) return variables[arg];
    try {
        return std::stoi(arg);
    }
    catch (...) {
        return 0; // fallback if not a number
    }
}

bool Process::executeNextInstruction(int coreId, int& cpuTick) {
    if (sleepTicks > 0) {
        --sleepTicks;
        return true;
    }

    if (currentInstr >= instructions.size()) return false;
    Instruction& instr = instructions[currentInstr];

    switch (instr.type) {
    case InstrType::PRINT: {
        std::string msg = "Hello world from " + name + "!";
        if (!instr.args.empty()) {
            std::string var = instr.args[0];
            uint16_t val = variables.count(var) ? variables[var] : 0;
            msg += " " + var + "=" + std::to_string(val);
        }

        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::tm tm_time;
        localtime_s(&tm_time, &time);
        std::stringstream ss;
        ss << "(" << std::put_time(&tm_time, "%m/%d/%Y %I:%M:%S%p")
            << ") Core:" << coreId << " \"" << msg << "\"";
        logFile << ss.str() << std::endl;
        break;
    }
    case InstrType::DECLARE: {
        if (instr.args.size() >= 2) {
            setVariable(instr.args[0], getValueFromArg(instr.args[1]));
        }
        break;
    }
    case InstrType::ADD:
    case InstrType::SUBTRACT: {
        if (instr.args.size() >= 3) {
            std::string var1 = instr.args[0];
            int v2 = getValueFromArg(instr.args[1]);
            int v3 = getValueFromArg(instr.args[2]);
            int result = (instr.type == InstrType::ADD) ? v2 + v3 : v2 - v3;
            setVariable(var1, result);
        }
        break;
    }
    case InstrType::SLEEP: {
        if (!instr.args.empty()) {
            int ticks = getValueFromArg(instr.args[0]);
            if (ticks > 0) sleepTicks = ticks - 1;
        }
        break;
    }
    case InstrType::FOR: {
        if (forDepth >= 3) break;
        int repeats = instr.repeats;
        for (int r = 0; r < repeats; ++r) {
            ++forDepth;
            for (auto& subInstr : instr.body) {
                instructions.insert(instructions.begin() + currentInstr + 1, subInstr);
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

void Process::generateRandomInstructions(int minIns, int maxIns) {
    static const std::vector<std::string> varNames = { "a", "b", "c", "d", "e" };
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> insDist(minIns, maxIns);
    std::uniform_int_distribution<> typeDist(0, 5);
    std::uniform_int_distribution<> varDist(0, varNames.size() - 1);
    std::uniform_int_distribution<> valDist(0, 1000);
    std::uniform_int_distribution<> sleepDist(1, 10);
    std::uniform_int_distribution<> forRepeatDist(1, 5);
    std::uniform_int_distribution<> forBodyDist(1, 3);

    instructions.clear();
    int numInstructions = insDist(gen);

    std::function<Instruction(int)> makeInstruction = [&](int depth) -> Instruction {
        Instruction instr;
        int t = typeDist(gen);

        if (t == 0) {
            instr.type = InstrType::PRINT;
            if (varDist(gen) % 2 == 0)
                instr.args.push_back(varNames[varDist(gen)]);
        }
        else if (t == 1) {
            instr.type = InstrType::DECLARE;
            instr.args = { varNames[varDist(gen)], std::to_string(valDist(gen)) };
        }
        else if (t == 2 || t == 3) {
            instr.type = (t == 2) ? InstrType::ADD : InstrType::SUBTRACT;
            instr.args.push_back(varNames[varDist(gen)]);
            for (int i = 0; i < 2; ++i) {
                if (varDist(gen) % 2 == 0)
                    instr.args.push_back(varNames[varDist(gen)]);
                else
                    instr.args.push_back(std::to_string(valDist(gen)));
            }
        }
        else if (t == 4) {
            instr.type = InstrType::SLEEP;
            instr.args.push_back(std::to_string(sleepDist(gen)));
        }
        else if (t == 5 && depth < 3) {
            instr.type = InstrType::FOR;
            instr.repeats = forRepeatDist(gen);
            int bodyLen = forBodyDist(gen);
            for (int i = 0; i < bodyLen; ++i) {
                instr.body.push_back(makeInstruction(depth + 1));
            }
        }
        else {
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

    logFile.open(name + ".txt", std::ios::out | std::ios::trunc);
}
