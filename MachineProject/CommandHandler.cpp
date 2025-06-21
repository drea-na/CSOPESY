#include "CommandHandler.h"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <map>
#include <mutex>
#include <vector>
#include <queue>

const std::string G = "\033[32m"; //G for green
const std::string Y = "\033[33m"; //Y for yellow
const std::string C = "\033[36m"; //C for cyan
const std::string Default = "\033[0m";

// External global variables
extern int global_core_count;
extern int global_quantum;
extern SchedulingAlgorithm global_algo;
extern std::vector<ProcessInfo> processList;
extern std::mutex processMutex;
extern std::queue<Console> readyQueue;
extern std::mutex queueMutex;
extern std::condition_variable cv;
extern bool schedulerRunning;

//external declarations
extern void readConfig();
extern void generateDummyProcess(const std::string& name);
extern void showProcessList();

bool CommandHandler::handleCommands(const std::string& command) {
    if (command == "clear") {
        system("cls");
        printHeader();
    }
    else if (command == "initialize") {
        initialize();
    }
    else if (command == "scheduler-start") {
        schedulerStart();
    }
    else if (command == "scheduler-stop") {
        schedulerStop();
    }
    else if (command == "screen -ls") {
        screenList();
    }
    else if (command == "report-util") {
        reportUtil();
    }
    else if (command.substr(0, 10) == "screen -s ") {
        std::string name = command.substr(10);
        screenS(name);
    }
    else if (command.substr(0, 10) == "screen -r ") {
        std::string name = command.substr(10);
        screenR(name);
    }
    else {
        std::cout << "Unknown command." << std::endl;
        printEnter();
        return false;
    }

    return true;
}

void CommandHandler::initialize() {
    readConfig();

    if (scheduler) {
        delete scheduler;
    }

    scheduler = new Scheduler(global_core_count, global_algo, global_quantum);
    std::cout << "Scheduler initialized: ";
    std::cout << (global_algo == SchedulingAlgorithm::FCFS ? "FCFS" : "RR");
    std::cout << ", cores: " << global_core_count << ", quantum: " << global_quantum << std::endl;
    printEnter();
}

void CommandHandler::schedulerStart() {
    if (!scheduler) {
        std::cout << "Run 'initialize' first!" << std::endl;
        printEnter();
    }

    for (int i = 1; i <= 10; ++i) {
        generateDummyProcess("process" + std::to_string(i));
    }

    std::cout << "Started scheduler with 10 dummy processes (see process_logs/)." << std::endl;
    printEnter();
}

void CommandHandler::schedulerStop() {
    if (!scheduler) {
        std::cout << "Run 'initialize' first!" << std::endl;
        printEnter();
        return;
    }

    schedulerRunning = false;
    cv.notify_all();

    delete scheduler;
    scheduler = nullptr;

    std::cout << "Scheduler stopped." << std::endl;
    printEnter();
}

void CommandHandler::screenList() {
    showProcessList();
    printEnter();
}

void CommandHandler::screenS(const std::string& name) {
    if (name.empty()) {
        std::cout << "Error: screen name cannot be empty." << std::endl;
    }
    else {
        if (screenMap.count(name)) {
            std::cout << "Screen '" << name << "' already exists." << std::endl;
        }
        else {
            screenMap[name] = Console(name);
            std::cout << "Created screen: " << name << std::endl;
        }
        bool inScreen = true;
        system("cls");
        screenMap[name].displayScreen();
        while (inScreen) {
			printEnter();
            std::string input;
            std::getline(std::cin, input);
            if (input == "exit") {
                inScreen = false;
                system("cls");
                printHeader();
            }
            else if (input == "process-smi") {
                inScreen = true;
                processSmi();
            }
            else {
				std::cout << "Unknown command in screen '" << name << "'. Type 'exit' to leave the screen." << std::endl;
            }
        }
    }
}

void CommandHandler::screenR(const std::string& name) {
    if (name.empty()) {
        std::cout << "Error: screen name cannot be empty." << std::endl;
    }
    else if (screenMap.count(name)) {
        bool inScreen = true;
        system("cls");
        screenMap[name].displayScreen();
        while (inScreen) {
			printEnter();
            std::string input;
            std::getline(std::cin, input);
            if (input == "exit") {
                inScreen = false;
                system("cls");
                printHeader();
            }
            else if (input == "process-smi") {
                inScreen = true;
                processSmi();
            }
            else {
                std::cout << "Unknown command in screen '" << name << "'. Type 'exit' to leave the screen." << std::endl;
            }
        }
    }
    else {
        std::cout << "No screen named '" << name << "' found." << std::endl;
        printEnter();
    }
}

void CommandHandler::reportUtil() {
    std::string filename = "csopesy-log.txt";
    std::ofstream reportFile(filename);

    if (!reportFile.is_open()) {
        std::cout << "Error: Could not create report file." << std::endl;
        printEnter();
        return;
    }

    auto now = Console().getCurrentTimestamp();

    reportFile << "CPU utilization: " << std::endl;
    reportFile << "Cores used: " << std::endl;
    reportFile << "Cores available: " << std::endl;

    {
        std::lock_guard<std::mutex> lock(processMutex);

        reportFile << std::string(50, '-') << std::endl;
        reportFile << "Running processes:\n";

        for (const auto& p : processList) {
            if (!p.finished) {
                reportFile << p.name << "\t(" << p.startTime << ")"
                    << "  Core: " << p.coreID
                    << "  " << p.progress << " / " << p.total << std::endl;
            }
        }

        reportFile << "\nFinished processes:\n";
        for (const auto& p : processList) {
            if (p.finished) {
                reportFile << p.name << "\t(" << p.startTime << ")"
                    << "  Finished  " << p.total << " / " << p.total << std::endl;
            }
        }

        reportFile << std::string(50, '-') << std::endl;
    }

    reportFile.close();
    std::cout << "Report generated at: " << filename << "!" << std::endl;
    printEnter();
}

void CommandHandler::processSmi() {
    std::cout << "\nProcess name: " << Console().getName() << std::endl;
    std::cout << "ID: " << std::endl;
    std::cout << "Logs:" << std::endl;
    std::cout << "Current instruction line: " << Console().getCurrentLine() << std::endl;
    std::cout << "Lines of code:" << Console().getTotalLines() << std::endl;
}

void CommandHandler::printHeader() {
    std::cout << C << " _______  _______  _______  _______  _______  _______  __   __ " << Default << std::endl;
    std::cout << C << "|       ||       ||       ||       ||       ||       ||  | |  |" << Default << std::endl;
    std::cout << C << "|       ||  _____||   _   ||    _  ||    ___||  _____||  |_|  |" << Default << std::endl;
    std::cout << C << "|       || |_____ |  | |  ||   |_| ||   |___ | |_____ |       |" << Default << std::endl;
    std::cout << C << "|      _||_____  ||  |_|  ||    ___||    ___||_____  ||_     _|" << Default << std::endl;
    std::cout << C << "|     |_  _____| ||       ||   |    |   |___  _____| |  |   |  " << Default << std::endl;
    std::cout << C << "|_______||_______||_______||___|    |_______||_______|  |___|  " << Default << std::endl;

    std::cout << Default << "\n" << std::string(50, '-') << Default << std::endl;
    std::cout << G << "\nHello, Welcome to CSOPESY Emulator!" << Default << std::endl;
    
    std::cout << Default << "Developers:" << Default << std::endl;
    std::cout << Default << "Chan, Kendrick Martin" << Default << std::endl;
    std::cout << Default << "Dionela, Valiant Lance" << Default << std::endl;
    std::cout << Default << "Dy, Fatima Kriselle" << Default << std::endl;
    std::cout << Default << "Loria, Andrea Euceli" << Default << std::endl;

    std::cout << Default << "\nLast Updated: " << Y << "06-21-2025" << Default << std::endl;
    
    std::cout << Default << "\n" << std::string(50, '-') << Default << std::endl;
    std::cout << Y << "Type 'exit' to quit, 'clear' to clear the screen" << Default << std::endl;
    std::cout << "Enter a command: ";
}

void CommandHandler::printEnter() {
    std::cout << "\nEnter a command: ";
}