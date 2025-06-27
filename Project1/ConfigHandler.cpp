#include "ConfigHandler.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>

std::string readQuotes(std::istream& is) {
    std::string val;
    is >> std::ws; 

    char c = is.peek();
    if (c == '"') {
        is.get(); 
        std::getline(is, val, '"');
    }
    else {
        is >> val;
    }

    return val;
}

Config ConfigHandler::loadFromFile(const std::string& filename) {
    Config config;
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open config file: " + filename);
    }

    std::string key;
    while (file >> key) {
        if (key == "num-cpu") config.numCPU = std::stoi(readQuotes(file));
        else if (key == "scheduler") config.scheduler = readQuotes(file);
        else if (key == "quantum-cycles") config.quantumCycles = std::stoi(readQuotes(file));
        else if (key == "batch-process-freq") config.batchProcessFreq = std::stoi(readQuotes(file));
        else if (key == "min-ins") config.minInstructions = std::stoi(readQuotes(file));
        else if (key == "max-ins") config.maxInstructions = std::stoi(readQuotes(file));
        else if (key == "delay-per-exec") config.delaysPerExec = std::stoi(readQuotes(file));
        else std::cerr << "Warning: unknown config key '" << key << "'" << std::endl;
    }

    file.close();
    return config;
}
