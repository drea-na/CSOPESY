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
extern std::atomic<bool> generatorRunning;
extern std::thread processGeneratorThread;
extern int global_batch_process_freq;
extern std::atomic<int> coresUsed;
extern std::atomic<int> totalCpuCycles;
extern std::atomic<int> activeCpuCycles;

//external declarations
extern void readConfig();
extern void generateDummyProcess(const std::string& name);
extern void showProcessList();

bool CommandHandler::handleCommands(const std::string& command) {
    // Only allow 'initialize' and 'exit' before initialization
    if (!initialized && command != "initialize" && command != "exit" && command != "clear") {
        std::cout << Y << "You must run 'initialize' before any other command!" << Default << std::endl;
        printEnter();
        return false;
    }
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
    initialized = true;
    printEnter();
}

void CommandHandler::schedulerStart() {
    if (!scheduler) {
        std::cout << "Run 'initialize' first!" << std::endl;
        printEnter();
    }
    schedulerRunning = true;
    for (int i = 1; i <= 10; ++i) {
        char buf[16];
        snprintf(buf, sizeof(buf), "process%02d", i);
        generateDummyProcess(buf);
    }
    if (!generatorRunning) {
        generatorRunning = true;
        processGeneratorThread = std::thread ([this]() {
            int processCount = 11;
            while (generatorRunning && schedulerRunning) {
                char buf[16];
                snprintf(buf, sizeof(buf), "process%02d", processCount);
                generateDummyProcess(buf);
                processCount++;
                std::this_thread::sleep_for(std::chrono::seconds(global_batch_process_freq));
            }
        });
    }
    std::cout << "Scheduler started. Processes will be generated every " << global_batch_process_freq << " seconds." << std::endl;
    printEnter();
}

void CommandHandler::schedulerStop() {
    if (!scheduler) {
        std::cout << "Run 'initialize' first!" << std::endl;
        printEnter();
        return;
    }

    schedulerRunning = false;
	generatorRunning = false;
    cv.notify_all();

    if (processGeneratorThread.joinable()) {
        processGeneratorThread.join();
    }

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
    } else {
        if (screenMap.count(name)) {
            std::cout << "Screen '" << name << "' already exists." << std::endl;
        } else {
            screenMap[name] = Console(name);
            std::cout << "Created screen: " << name << std::endl;
        }
        bool inScreen = true;
        system("cls");
        // Display process info and logs
        auto it = std::find_if(processList.begin(), processList.end(), [&](const ProcessInfo& p){ return p.name == name; });
        if (it != processList.end()) {
            std::cout << "\n=== Screen for Process: " << it->name << " ===" << std::endl;
            std::cout << "Process ID: " << (it - processList.begin()) << std::endl;
            std::cout << "Instruction: " << it->progress << " / " << it->total << std::endl;
            std::cout << "Started at: " << it->startTime << std::endl;
            // Show last 10 lines of log
            std::ifstream logFile("process_logs/" + name + ".txt");
            std::vector<std::string> lines;
            std::string line;
            while (std::getline(logFile, line)) lines.push_back(line);
            std::cout << "--- Last 10 PRINT logs ---" << std::endl;
            for (int i = std::max(0, (int)lines.size()-10); i < (int)lines.size(); ++i) {
                std::cout << lines[i] << std::endl;
            }
        } else {
            std::cout << "Process " << name << " not found" << std::endl;
        }
        while (inScreen) {
            printEnter();
            std::string input;
            std::getline(std::cin, input);
            if (input == "exit") {
                inScreen = false;
                system("cls");
                printHeader();
            } else if (input == "process-smi") {
                inScreen = true;
                auto it = std::find_if(processList.begin(), processList.end(), [&](const ProcessInfo& p){ return p.name == name; });
                if (it != processList.end()) {
                    std::cout << "\nProcess name: " << it->name << std::endl;
                    std::cout << "ID: " << (it - processList.begin()) << std::endl;
                    std::cout << "Current instruction line: " << it->progress << std::endl;
                    std::cout << "Lines of code: " << it->total << std::endl;
                    if (it->finished) std::cout << "Finished!" << std::endl;
                } else {
                    std::cout << "Process " << name << " not found" << std::endl;
                }
            } else {
                std::cout << "Unknown command in screen '" << name << "'. Type 'exit' to leave the screen." << std::endl;
            }
        }
    }
}

void CommandHandler::screenR(const std::string& name) {
    if (name.empty()) {
        std::cout << "Error: screen name cannot be empty." << std::endl;
    } else {
        auto it = std::find_if(processList.begin(), processList.end(), [&](const ProcessInfo& p){ return p.name == name && !p.finished; });
        if (it == processList.end()) {
            std::cout << "Process " << name << " not found" << std::endl;
            printEnter();
            return;
        }
        bool inScreen = true;
        system("cls");
        std::cout << "\n=== Screen for Process: " << it->name << " ===" << std::endl;
        std::cout << "Process ID: " << (it - processList.begin()) << std::endl;
        std::cout << "Instruction: " << it->progress << " / " << it->total << std::endl;
        std::cout << "Started at: " << it->startTime << std::endl;
        // Show last 10 lines of log
        std::ifstream logFile("process_logs/" + name + ".txt");
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(logFile, line)) lines.push_back(line);
        std::cout << "--- Last 10 PRINT logs ---" << std::endl;
        for (int i = std::max(0, (int)lines.size()-10); i < (int)lines.size(); ++i) {
            std::cout << lines[i] << std::endl;
        }
        while (inScreen) {
            printEnter();
            std::string input;
            std::getline(std::cin, input);
            if (input == "exit") {
                inScreen = false;
                system("cls");
                printHeader();
            } else if (input == "process-smi") {
                inScreen = true;
                if (it != processList.end()) {
                    std::cout << "\nProcess name: " << it->name << std::endl;
                    std::cout << "ID: " << (it - processList.begin()) << std::endl;
                    std::cout << "Current instruction line: " << it->progress << std::endl;
                    std::cout << "Lines of code: " << it->total << std::endl;
                    if (it->finished) std::cout << "Finished!" << std::endl;
                } else {
                    std::cout << "Process " << name << " not found" << std::endl;
                }
            } else {
                std::cout << "Unknown command in screen '" << name << "'. Type 'exit' to leave the screen." << std::endl;
            }
        }
    }
}

void CommandHandler::reportUtil() {
    double utilization = 0.0;
    if (totalCpuCycles > 0) {
        utilization = (double)activeCpuCycles / totalCpuCycles * 100.0;
    }

    int used = coresUsed.load();
    int available = global_core_count - used;

    std::string filename = "csopesy-log.txt";
    std::ofstream reportFile(filename);

    if (!reportFile.is_open()) {
        std::cout << "Error: Could not create report file." << std::endl;
        printEnter();
        return;
    }

    auto now = Console().getCurrentTimestamp();

    reportFile << "CPU utilization: " << std::fixed << std::setprecision(2) << utilization << "%" << std::endl;
    reportFile << "Cores used: " << used << std::endl;
    reportFile << "Cores available: " << available << std::endl;

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
    std::cout << Default << "\nLast Updated: " << Y << "06-27-2025" << Default << std::endl;
    std::cout << Default << "\n" << std::string(50, '-') << Default << std::endl;
    std::cout << Y << "Type 'exit' to quit, 'clear' to clear the screen" << Default << std::endl;
    std::cout << "root:\> ";
}

void CommandHandler::printEnter() {
    std::cout << "\nroot:\> ";
}