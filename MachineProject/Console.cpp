// Console.cpp
#include "Console.h"
#include <iostream>
#include <ctime>
#include <iomanip>
#include <sstream>


Console::Console(string screenName) {
    name = screenName;
    currentLine = 1;
    totalLines = 100;
    timestamp = getCurrentTimestamp();
}

// Default constructor implementation
Console::Console() : name("Default"), currentLine(0), totalLines(0), timestamp(getCurrentTimestamp()) {}

void Console::displayScreen() {
    cout << "\n=== Screen for Process: " << name << " ===" << endl;
    cout << "Instruction: " << currentLine << " / " << totalLines << endl;
    cout << "Started at: " << timestamp << endl;
    cout << "(Type 'exit' to return to the main menu)\n";
}

// Helper function to get the current timestamp
string Console::getCurrentTimestamp() {
    time_t now = time(0);
    tm localTime;
    localtime_s(&localTime, &now); // Use localtime_s for safer local time conversion
    stringstream ss;
    ss << (localTime.tm_year + 1900) << "-"
       << (localTime.tm_mon + 1) << "-"
       << localTime.tm_mday << " "
       << localTime.tm_hour << ":"
       << localTime.tm_min << ":"
       << localTime.tm_sec;
    return ss.str();
}
