#ifndef PROCESS_H
#define PROCESS_H

#include <string>
#include <fstream>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <iostream>

class Process {
public:
    std::string name;
    int totalCommands = 100;
    int executedCommands = 0;
    std::ofstream logFile;

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
        std::cout << logLine << std::endl;
    }
};

#endif
