#ifndef COMMANDHANDLER_H
#define COMMANDHANDLER_H

#include <string>
#include <map>
#include "Scheduler.h"
#include "Screen.h"

class CommandHandler {
public:
    // Initialize references to screen map and scheduler
    static void initialize(std::map<std::string, Screen>& screenMapRef, Scheduler*& schedulerRef);

    // Main input loop
    static void handle();

private:
    // Command execution
    static void executeCommand(const std::string& command);

    // Command handlers
    static void doInitialize();
    static void startScheduler();
    static void showScreenList();

    // NEW: Create a new process screen
    static void newProcess(const std::string& name);

    // NEW: Show existing process screen
    static void showProcessScreen(const std::string& name);

    static void showReportUtil();
    static void showProcessSMI();

    // Console UI helpers
    static void drawHeader();
    static void prompt();

private:
    static std::map<std::string, Screen>* screenMap;
    static Scheduler* scheduler;
    static bool initialized;
    static std::string lastScreenedProcessName;
};

#endif // COMMANDHANDLER_H
