// Console.cpp
#include "Console.h"
#include <iostream>
#include <ctime>
#include <sstream>


Console::Console(std::string screenName) {
    name = screenName;
    currentLine = 1;
    totalLines = 0;
    timestamp = getCurrentTimestamp();
}

// Default constructor
Console::Console() : name("Default"), currentLine(0), totalLines(0), timestamp(getCurrentTimestamp()) {}

void Console::displayScreen() {
    std::cout << "\n=== Screen for Process: " << name << " ===" << std::endl;
    std::cout << "Instruction: " << currentLine << " / " << totalLines << std::endl;
    std::cout << "Started at: " << timestamp << std::endl;
    std::cout << "(Type 'exit' to return to the main menu)\n";
}

void Console::updateProgress(int current, int total) {
        currentLine = current;
		totalLines = total;
}

// Helper function for current timestamp
std::string Console::getCurrentTimestamp() {
    time_t now = time(0);
    tm localTime;
    localtime_s(&localTime, &now);
    std::stringstream ss;
    ss << (localTime.tm_year + 1900) << "-"
       << (localTime.tm_mon + 1) << "-"
       << localTime.tm_mday << " "
       << localTime.tm_hour << ":"
       << localTime.tm_min << ":"
       << localTime.tm_sec;
    return ss.str();
}
