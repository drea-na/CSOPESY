#pragma once
#ifndef SCREEN_H
#define SCREEN_H

#include <string>

class Screen {
private:
    std::string name;
    int currentLine;
    int totalLines;
    std::string timestamp;

public:
    Screen(); // Default constructor
    Screen(const std::string& screenName);

    void displayScreen() const;
    std::string getCurrentTimestamp() const;

    // Getters
    std::string getName() const;
    int getCurrentLine() const;
    int getTotalLines() const;
    std::string getTimestamp() const;

    // Setters
    void setCurrentLine(int line);
    void setTotalLines(int total);
};

#endif // SCREEN_H
