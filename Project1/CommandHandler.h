#ifndef COMMANDHANDLER_H
#define COMMANDHANDLER_H

#include <thread>
#include <string>
#include <map>
#include "Scheduler.h"
#include "Screen.h"

class CommandHandler {
public:
    static void initialize(std::map<std::string, Screen>& screenMapRef, Scheduler*& schedulerRef);
    static void handle();

private:
    static void executeCommand(const std::string& command);
    static void doInitialize();
    static void startScheduler();
    static void stopScheduler();
    static void showScreenList();
    static void showReportUtil();
    static void showProcessSMI();
    static void newProcess(const std::string& name);
    static void showProcessScreen(const std::string& name);
    static void drawHeader();
    static void prompt();

    static void printProcessLine(const ProcessInfo& p);
    static void logProcessLine(std::ofstream& log, const ProcessInfo& p);

    static std::map<std::string, Screen>* screenMap;
    static Scheduler* scheduler;
    static bool initialized;
    static std::string lastScreenedProcessName;
    static std::thread batcherThread;
    static std::atomic<bool> batchingEnabled;
};

#endif // COMMANDHANDLER_H
