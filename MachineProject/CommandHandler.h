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

public:
    CommandHandler(std::map<std::string, Console>& screens, Scheduler*& sched)
        : screenMap(screens), scheduler(sched) {
    }

    bool handleCommands(const std::string& command);

    void initialize();
    void schedulerStart();
    void schedulerStop();
    void screenList();
    void screenS(const std::string& name);
    void screenR(const std::string& name);
    void reportUtil();
    void processSmi();
    void printHeader();
    void printEnter();
};

#endif
