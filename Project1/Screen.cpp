#include "Screen.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

Screen::Screen() : name("Unnamed"), currentLine(0), totalLines(0) {
    timestamp = getCurrentTimestamp();
}

Screen::Screen(const std::string& screenName)
    : name(screenName), currentLine(0), totalLines(0) {
    timestamp = getCurrentTimestamp();
}

void Screen::displayScreen() const {
    std::cout << "[Screen: " << name << "]\n";
    std::cout << "Current Line: " << currentLine << "\n";
    std::cout << "Total Lines: " << totalLines << "\n";
    std::cout << "Timestamp: " << timestamp << "\n";
}

std::string Screen::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto timeT = std::chrono::system_clock::to_time_t(now);
    std::tm tmTime{};
#ifdef _WIN32
    localtime_s(&tmTime, &timeT);
#else
    localtime_r(&timeT, &tmTime);
#endif
    std::stringstream ss;
    ss << std::put_time(&tmTime, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::string Screen::getName() const {
    return name;
}

int Screen::getCurrentLine() const {
    return currentLine;
}

int Screen::getTotalLines() const {
    return totalLines;
}

std::string Screen::getTimestamp() const {
    return timestamp;
}

void Screen::setCurrentLine(int line) {
    currentLine = line;
}

void Screen::setTotalLines(int total) {
    totalLines = total;
}
