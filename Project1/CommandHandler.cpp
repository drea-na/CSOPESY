#include "CommandHandler.h"
#include "ConfigHandler.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <thread>
#include <chrono>

std::map<std::string, Screen>* CommandHandler::screenMap = nullptr;
Scheduler* CommandHandler::scheduler = nullptr;
bool CommandHandler::initialized = false;
std::string CommandHandler::lastScreenedProcessName = "";
std::thread CommandHandler::batcherThread;
std::atomic<bool> CommandHandler::batchingEnabled = false;

static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(' ');
    if (start == std::string::npos) return "";
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
            batchingEnabled = false;
            if (batcherThread.joinable()) batcherThread.join();
            std::cout << "Shutting Down...\n";
            break;
        }


        if (!initialized) {
            if (input == "initialize") {
                doInitialize();
                prompt();
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
    else if (command == "scheduler-stop") {
        stopScheduler();
    }
    else if (command == "screen -ls") {
        showScreenList();
    }
    else if (command.rfind("screen -s ", 0) == 0) {
        std::string procName = trim(command.substr(10));
        if (!procName.empty()) newProcess(procName);
    }
    else if (command.rfind("screen -r ", 0) == 0) {
        std::string procName = trim(command.substr(10));
        if (!procName.empty()) showProcessScreen(procName);
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
        SchedulingAlgorithm algorithm = config.scheduler == "RR" ? SchedulingAlgorithm::RR : SchedulingAlgorithm::FCFS;
        scheduler = new Scheduler(config.numCPU, algorithm, config.quantumCycles);
        initialized = true;

        std::cout << "[Config Loaded]\n";
        std::cout << "  Cores: " << config.numCPU << "\n";
        std::cout << "  Scheduler: " << config.scheduler << "\n";
        std::cout << "  Quantum: " << config.quantumCycles << "\n";
        std::cout << "  Batch Freq (s): " << config.batchProcessFreq << "\n";
        std::cout << "  Instruction Range: " << config.minInstructions << " - " << config.maxInstructions << "\n";

        std::cout << "[System initialized successfully]\n";
    }
    catch (const std::exception& ex) {
        std::cerr << "Initialization failed: " << ex.what() << "\n";
    }

}

void CommandHandler::startScheduler() {
    if (!scheduler) {
        std::cout << "[No scheduler found]\n";
        return;
    }

    if (batchingEnabled) {
        std::cout << "[Scheduler already running]\n";
        return;
    }

    Config config = ConfigHandler::loadFromFile("config.txt");
    batchingEnabled = true;

    batcherThread = std::thread([config]() {
        int counter = 1;
        //std::cout << "[Batcher thread started]\n";

        while (batchingEnabled) {
            std::this_thread::sleep_for(std::chrono::milliseconds(config.batchProcessFreq * 1000));

            if (!batchingEnabled) break;

            try {
                std::ostringstream oss;
                oss << "process" << std::setw(2) << std::setfill('0') << counter++;
                std::string procName = oss.str();

                Process* proc = new Process(procName);
                proc->generateRandomInstructions(config.minInstructions, config.maxInstructions);
                scheduler->addProcess(proc);

                Screen screen(procName);
                screen.setTotalLines(proc->totalCommands);
                (*screenMap)[procName] = screen;

            }
            catch (const std::exception& e) {
                std::cerr << "[Batcher error] " << e.what() << "\n";
                break;
            }
        }

        //std::cout << "[Batcher thread exited]\n";
        });

    std::cout << "Scheduler started. Generating new process every " << config.batchProcessFreq << " CPU tick.\n";
}


void CommandHandler::stopScheduler() {
    if (!scheduler) {
        std::cout << "[No scheduler found]\n";
        return;
    }

    if (!batchingEnabled) {
        std::cout << "[Scheduler is not running]\n";
        return;
    }

    batchingEnabled = false;

    if (batcherThread.joinable()) {
        batcherThread.join(); 
    }

    scheduler->stop(); 
    std::cout << "[Scheduler stopped]\n";
    prompt();
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

    ProcessInfo* info = scheduler->getProcessInfoByName(name);
    if (info && !info->finished) {
        std::cout << "Process " << name << " is already running.\n";
        return;
    }

    // Create new process and add to scheduler
    Process* proc = new Process(name);
    Config config = ConfigHandler::loadFromFile("config.txt");
    proc->generateRandomInstructions(config.minInstructions, config.maxInstructions);
    scheduler->addProcess(proc);

    // Create screen
    Screen screen(name);
    screen.setTotalLines(proc->totalCommands);
    screen.setCurrentLine(0);  // New process, line is 0
    (*screenMap)[name] = screen;

    system("cls");
    screen.displayScreen();

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

    // Update and show screen
    Screen screen(name);
    screen.setCurrentLine(info->progress);
    screen.setTotalLines(info->total);
    (*screenMap)[name] = screen;

    screen.displayScreen();
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
