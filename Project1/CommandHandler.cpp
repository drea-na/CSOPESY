#include "CommandHandler.h"
#include "ConfigHandler.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm> // for trim helpers

// === Static Members ===
std::map<std::string, Screen>* CommandHandler::screenMap = nullptr;
Scheduler* CommandHandler::scheduler = nullptr;
bool CommandHandler::initialized = false;
std::string CommandHandler::lastScreenedProcessName = "";

// Helper function to trim leading and trailing spaces
static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(' ');
    if (start == std::string::npos) return ""; // all spaces
    size_t end = s.find_last_not_of(' ');
    return s.substr(start, end - start + 1);
}

// === Initialization ===
void CommandHandler::initialize(std::map<std::string, Screen>& screenMapRef, Scheduler*& schedulerRef) {
    screenMap = &screenMapRef;
    scheduler = schedulerRef;
    drawHeader();
    prompt();
}

// === Input Loop ===
void CommandHandler::handle() {
    std::string input;
    while (true) {
        std::getline(std::cin, input);

        if (input == "exit") {
            std::cout << "Shutting Down...\n";
            break;
        }

        if (!initialized) {
            if (input == "initialize") {
                doInitialize();
            }
            else {
                std::cout << "[System not initialized. Type 'initialize' to start or 'exit' to quit.]\n";
                prompt();
            }
        }
        else {
            if (input == "clear") {
                system("cls");
                drawHeader();
            }
            else {
                executeCommand(input);
            }
            prompt();
        }
    }
}

// === Console UI ===
void CommandHandler::drawHeader() {
    using namespace std;
    cout << "\033[36m _______  _______  _______  _______  _______  _______  __   __ \033[0m\n";
    cout << "\033[36m|       ||       ||       ||       ||       ||       ||  | |  |\033[0m\n";
    cout << "\033[36m|       ||  _____||   _   ||    _  ||    ___||  _____||  |_|  |\033[0m\n";
    cout << "\033[36m|       || |_____ |  | |  ||   |_| ||   |___ | |_____ |       |\033[0m\n";
    cout << "\033[36m|      _||_____  ||  |_|  ||    ___||    ___||_____  ||_     _|\033[0m\n";
    cout << "\033[36m|     |_  _____| ||       ||   |    |   |___  _____| |  |   |  \033[0m\n";
    cout << "\033[36m|_______||_______||_______||___|    |_______||_______|  |___|  \033[0m\n";
    cout << "\033[32m\nHello, Welcome to CSOPESY commandline!\033[0m\n";
    cout << "\033[33mType 'exit' to quit, 'clear' to clear the screen\033[0m\n";
}

void CommandHandler::prompt() {
    std::cout << "\nroot:\\> ";
}

// === Command Execution ===
void CommandHandler::executeCommand(const std::string& command) {
    if (command == "initialize") {
        doInitialize();
    }
    else if (!initialized) {
        std::cout << "[System not initialized. Type 'initialize' first.]\n";
    }
    else if (command == "scheduler-start") {
        startScheduler();
    }
    else if (command == "screen -ls") {
        showScreenList();
    }
    else if (command.rfind("screen -s ", 0) == 0) {
        std::string procName = trim(command.substr(10));
        if (procName.empty()) {
            std::cout << "Please provide a process name for screen -s.\n";
        }
        else {
            newProcess(procName);
        }
    }
    else if (command.rfind("screen -r ", 0) == 0) {
        std::string procName = trim(command.substr(10));
        if (procName.empty()) {
            std::cout << "Please provide a process name for screen -r.\n";
        }
        else {
            showProcessScreen(procName);
        }
    }
    else if (command == "report-util") {
        showReportUtil();
    }
    else if (command == "process-smi") {
        showProcessSMI();
    }
    else {
        std::cout << "Unknown command. Type 'help' for available commands.\n";
    }
}

// === Command Implementations ===
void CommandHandler::doInitialize() {
    if (initialized) {
        std::cout << "[System already initialized]\n";
        return;
    }

    try {
        Config config = ConfigHandler::loadFromFile("config.txt");

        SchedulingAlgorithm algorithm = SchedulingAlgorithm::FCFS;
        if (config.scheduler == "RR") algorithm = SchedulingAlgorithm::RR;

        scheduler = new Scheduler(config.numCPU, algorithm, config.quantumCycles);
        initialized = true;

        std::cout << "[System initialized successfully]\n";
        std::cout << "Loaded configuration:\n";
        std::cout << " - CPU Cores: " << config.numCPU << "\n";
        std::cout << " - Scheduler: " << config.scheduler << "\n";
        std::cout << " - Quantum Cycles: " << config.quantumCycles << "\n";
        std::cout << " - Batch Freq: " << config.batchProcessFreq << "\n";
        std::cout << " - Min Instructions: " << config.minInstructions << "\n";
        std::cout << " - Max Instructions: " << config.maxInstructions << "\n";
        std::cout << " - Delay per Exec: " << config.delaysPerExec << "\n";
        prompt();
    }
    catch (const std::exception& ex) {
        std::cerr << "Initialization failed: " << ex.what() << "\n";
    }
}

void CommandHandler::startScheduler() {
    if (scheduler) {
        scheduler->start();
        std::cout << "[Scheduler started]\n";
    }
    else {
        std::cout << "[No scheduler found]\n";
    }
}

void CommandHandler::showScreenList() {
    std::cout << "Available screens:\n";
    for (const auto& [name, screen] : *screenMap) {
        std::cout << " - " << name << "\n";
    }
}

void CommandHandler::newProcess(const std::string& name) {
    if (!scheduler) {
        std::cout << "[Scheduler not initialized]\n";
        return;
    }

    // Check if process already exists
    ProcessInfo* info = scheduler->getProcessInfoByName(name);
    if (info && !info->finished) {
        std::cout << "Process " << name << " is already running.\n";
        return;
    }

    // Create new process, add to scheduler, and start interaction
    Process* proc = new Process(name);
    scheduler->addProcess(proc);

    system("cls");
    lastScreenedProcessName = name;

    std::string cmd;
    while (true) {
        std::cout << "[screen: " << name << "]> ";
        std::getline(std::cin, cmd);

        if (cmd == "process-smi") {
            showProcessSMI();
        }
        else if (cmd == "exit") {
            lastScreenedProcessName = "";
            system("cls");
            drawHeader();
            break;
        }
        else {
            std::cout << "Unknown command. Available: process-smi, exit\n";
        }
    }
}

void CommandHandler::showProcessScreen(const std::string& name) {
    if (!scheduler) return;

    ProcessInfo* info = scheduler->getProcessInfoByName(name);
    if (!info || info->finished) {
        std::cout << "Process " << name << " not found.\n";
        return;
    }

    system("cls");
    lastScreenedProcessName = name;

    std::string cmd;
    while (true) {
        std::cout << "[screen: " << name << "]> ";
        std::getline(std::cin, cmd);

        if (cmd == "process-smi") {
            showProcessSMI();
        }
        else if (cmd == "exit") {
            lastScreenedProcessName = "";
            system("cls");
            drawHeader();
            break;
        }
        else {
            std::cout << "Unknown command. Available: process-smi, exit\n";
        }
    }
}

void CommandHandler::showReportUtil() {
    if (!scheduler) {
        std::cout << "No scheduler found.\n";
        return;
    }

    std::vector<ProcessInfo> processes = scheduler->getProcessList();
    std::cout << "Process Report:\n";
    for (const auto& p : processes) {
        std::cout << "ID: " << p.id << " | Name: " << p.name
            << " | Core: " << p.coreID
            << " | Progress: " << p.progress << "/" << p.total
            << " | " << (p.finished ? "Finished" : "Running") << "\n";
    }
}

void CommandHandler::showProcessSMI() {
    if (!lastScreenedProcessName.empty() && scheduler) {
        ProcessInfo* info = scheduler->getProcessInfoByName(lastScreenedProcessName);
        if (!info) {
            std::cout << "Process not found.\n";
            return;
        }

        std::cout << "Process name: " << info->name << "\n";
        std::cout << "ID: " << info->id << "\n";

        // Try to read from log file
        std::ifstream logFile(info->name + ".txt");
        std::string line;
        std::cout << "Logs:\n";
        if (logFile.is_open()) {
            while (std::getline(logFile, line)) {
                std::cout << line << "\n";
            }
        }
        else {
            std::cout << "No print logs yet.\n";
        }

        std::cout << "Instruction line: " << info->progress << "\n";
        std::cout << "Lines of code: " << info->total << "\n";

        if (info->finished)
            std::cout << "Finished!\n";
    }
    else {
        std::cout << "No active screen process.\n";
    }
}
