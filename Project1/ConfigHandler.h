#pragma once
#include <string>

struct Config {
    int numCPU = 1;
    std::string scheduler = "fcfs";
    int quantumCycles = 1;
    int batchProcessFreq = 0;
    int minInstructions = 1;
    int maxInstructions = 10;
    int delaysPerExec = 0;
};

class ConfigHandler {
public:
    static Config loadFromFile(const std::string& filename);
};
