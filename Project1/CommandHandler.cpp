#include "CommandHandler.h"
#include "ConfigHandler.h"
#include <iostream>
#include <sstream>

// === Static Members ===
std::map<std::string, Screen>* CommandHandler::screenMap = nullptr;
Scheduler* CommandHandler::scheduler = nullptr;
bool CommandHandler::initialized = false;

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
            } else {
                std::cout << "[System not initialized. Type 'initialize' to start or 'exit' to quit.]\n";
                prompt();
            }
        } else {
            if (input == "clear") {
                system("cls");
                drawHeader();
            } else {
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
    else if (command.rfind("screen s ", 0) == 0) {
        showScreen(command.substr(9));
    }
    else if (command.rfind("screen r ", 0) == 0) {
        resetScreen(command.substr(9));
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

void CommandHandler::showScreen(const std::string& name) {
    auto it = screenMap->find(name);
    if (it != screenMap->end()) {
        std::cout << "Screen: " << name << "\n";
        it->second.displayScreen();
    }
    else {
        std::cout << "Screen not found.\n";
    }
}

void CommandHandler::resetScreen(const std::string& name) {
    auto it = screenMap->find(name);
    if (it != screenMap->end()) {
        it->second.setCurrentLine(0);
        std::cout << "Screen reset.\n";
    }
    else {
        std::cout << "Screen not found.\n";
    }
}

void CommandHandler::showReportUtil() {
    std::cout << "[CPU Utilization Report Placeholder]\n";
}

void CommandHandler::showProcessSMI() {
    std::cout << "Placeholder\n";
}