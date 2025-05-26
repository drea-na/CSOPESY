#ifndef CONSOLE_H
#define CONSOLE_H

#include <string>
using namespace std;

class Console {
public:
    string name;
    int currentLine;
    int totalLines;
    string timestamp;

    Console(); // Default constructor
    Console(string screenName);

    void displayScreen();

private:
    string getCurrentTimestamp();
};

#endif
