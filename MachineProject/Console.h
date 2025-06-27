#ifndef CONSOLE_H
#define CONSOLE_H

#include <string>

class Console {
private:
    std::string name;
    int currentLine;
    int totalLines;
    std::string timestamp;

public:
    Console(); // Default constructor
    Console(std::string screenName);

    void displayScreen();
	void updateProgress(int current, int total);
    std::string getCurrentTimestamp();

    //Getter functions
    std::string getName() const {
		return name;
    }

    int getCurrentLine() const {
		return currentLine;
    }

	int getTotalLines() const {
        return totalLines;
    }
    std::string getTimestamp() const {
        return timestamp;
    }

    //Setter functions
    void setCurrentLine(int line) {
        currentLine = line;
    }

    void setTotalLines(int total) {
        totalLines = total;
    }

};

#endif
