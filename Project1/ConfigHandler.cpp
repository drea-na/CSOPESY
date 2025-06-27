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
        try {
            if (key == "num-cpu") config.numCPU = std::stoi(readQuotes(file));
            else if (key == "scheduler") config.scheduler = readQuotes(file);
            else if (key == "quantum-cycles") config.quantumCycles = std::stoi(readQuotes(file));
            else if (key == "batch-process-freq") config.batchProcessFreq = std::stoi(readQuotes(file));
            else if (key == "min-ins") config.minInstructions = std::stoi(readQuotes(file));
            else if (key == "max-ins") config.maxInstructions = std::stoi(readQuotes(file));
            else if (key == "delay-per-exec") config.delaysPerExec = std::stoi(readQuotes(file));
            else std::cerr << "Warning: unknown config key '" << key << "'" << std::endl;
        }
        catch (...) {
            std::cerr << "[config.txt] Invalid value for key '" << key << "'. Skipping...\n";
        }
    }

    file.close();

    // === Validation and Defaults ===
    if (config.numCPU < 1 || config.numCPU > 128) {
        std::cout << "[config.txt] num-cpu out of range (1-128). Using default (4)." << std::endl;
        config.numCPU = 4;
    }
    if (config.quantumCycles < 1) {
        std::cout << "[config.txt] quantum-cycles out of range (>=1). Using default (2)." << std::endl;
        config.quantumCycles = 2;
    }
    if (config.batchProcessFreq < 1) {
        std::cout << "[config.txt] batch-process-freq out of range (>=1). Using default (1)." << std::endl;
        config.batchProcessFreq = 1;
    }
    if (config.minInstructions < 1) {
        std::cout << "[config.txt] min-ins out of range (>=1). Using default (1000)." << std::endl;
        config.minInstructions = 1000;
    }
    if (config.maxInstructions < config.minInstructions) {
        std::cout << "[config.txt] max-ins less than min-ins. Setting max-ins = min-ins." << std::endl;
        config.maxInstructions = config.minInstructions;
    }
    if (config.delaysPerExec < 0) {
        std::cout << "[config.txt] delay-per-exec out of range (>=0). Using default (0)." << std::endl;
        config.delaysPerExec = 0;
    }

    return config;
}
