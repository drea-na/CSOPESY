#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <string>
#include <map>
#include "Scheduler.h"
#include "Screen.h"

class CommandHandler {
public:
    // Initialize static members
    static void initialize(std::map<std::string, Screen>& screenMapRef, Scheduler*& schedulerRef);

    // Main loop
    static void handle();

    // Console UI
    static void drawHeader();
    static void prompt();

    // Command Execution
    static void executeCommand(const std::string& command);
    static void doInitialize();
    static void startScheduler();
    static void showScreenList();
    static void showScreen(const std::string& name);
    static void resetScreen(const std::string& name);
    static void reportUtilization();
    static void showHelp();

private:
    static std::map<std::string, Screen>* screenMap;
    static Scheduler* scheduler;
    static bool initialized;
};

#endif // COMMAND_HANDLER_H
