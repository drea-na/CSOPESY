#ifndef COMMANDHANDLER_H
#define COMMANDHANDLER_H

#include <string>
#include <map>
#include "Console.h"
#include "Scheduler.h"

class CommandHandler {
private:
    std::map<std::string, Console>& screenMap;
    Scheduler*& scheduler;
    bool initialized = false; // Track if initialize has been called

    // Helper function to validate memory size
    bool isValidMemorySize(int size) const;
    bool isPowerOfTwo(int n) const;

public:
    CommandHandler(std::map<std::string, Console>& screens, Scheduler*& sched)
        : screenMap(screens), scheduler(sched), initialized(false) {
    }

    bool isInitialized() const { return initialized; }

    bool handleCommands(const std::string& command);

    void initialize();
    void schedulerStart();
    void schedulerStop();
    void screenList();
    void screenS(const std::string& name, int memorySize = -1);
    void screenR(const std::string& name);
    void reportUtil();
    void processSmi(const std::string& processName);
    void vmstat();
    void printHeader();
    void printEnter();
};

#endif