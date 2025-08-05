#include "CommandHandler.h"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <map>
#include <mutex>
#include <vector>
#include <queue>
#include <algorithm>
#include <condition_variable>

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
extern MemoryManager* memoryManager;
extern int global_mem_per_proc;

//external declarations
extern void readConfig();
extern void generateDummyProcess(const std::string& name);
extern void generateDummyProcessWithMemory(const std::string& name, int memorySize);
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
    else if (command == "vmstat") {
        vmstat();
    }
    else if (command == "screen -ls") {
        screenList();
    }
    else if (command == "report-util") {
        reportUtil();
    }
    else if (command.substr(0, 10) == "screen -s ") {
        std::string remaining = command.substr(10);
        size_t spacePos = remaining.find(' ');
        if (spacePos != std::string::npos) {
            // Format: screen -s <process_name> <memory_size>
            std::string name = remaining.substr(0, spacePos);
            std::string memorySizeStr = remaining.substr(spacePos + 1);
            
            try {
                int memorySize = std::stoi(memorySizeStr);
                screenS(name, memorySize);
            } catch (const std::exception& e) {
                std::cout << "Error: Invalid memory size format. Please provide a valid number." << std::endl;
                printEnter();
                return false;
            }
        } else {
            // Format: screen -s <process_name> (no memory size specified)
            std::string name = remaining;
            screenS(name);
        }
    }
    else if (command.substr(0, 10) == "screen -r ") {
        std::string name = command.substr(10);
        screenR(name);
    }
    else if (command.substr(0, 10) == "screen -c ") {
        // Format: screen -c <process_name> <memory_size> "<instructions>"
        std::string remaining = command.substr(10);
        size_t firstSpace = remaining.find(' ');
        if (firstSpace == std::string::npos) {
            std::cout << "Error: Missing process name or memory size." << std::endl;
            printEnter();
            return false;
        }
        std::string name = remaining.substr(0, firstSpace);
        std::string afterName = remaining.substr(firstSpace + 1);
        size_t secondSpace = afterName.find(' ');
        if (secondSpace == std::string::npos) {
            std::cout << "Error: Missing memory size or instructions." << std::endl;
            printEnter();
            return false;
        }
        std::string memorySizeStr = afterName.substr(0, secondSpace);
        std::string instructionsStr = afterName.substr(secondSpace + 1);
        // Remove quotes if present
        if (!instructionsStr.empty() && instructionsStr.front() == '"' && instructionsStr.back() == '"') {
            instructionsStr = instructionsStr.substr(1, instructionsStr.size() - 2);
        }
        int memorySize = -1;
        try {
            memorySize = std::stoi(memorySizeStr);
        } catch (...) {
            std::cout << "Error: Invalid memory size format. Please provide a valid number." << std::endl;
            printEnter();
            return false;
        }
        if (!isValidMemorySize(memorySize)) {
            std::cout << "Error: Invalid memory allocation. Memory size must be between 64 and 65536 bytes and be a power of 2." << std::endl;
            printEnter();
            return false;
        }
        // Create process with instructions
        try {
            // Create process and parse instructions
            Process* p = new Process(name, memoryManager);
            p->parseUserInstructions(instructionsStr);
            // Initialize page table for this process
            if (memoryManager) {
                int numPages = (memorySize + memoryManager->getFrameSize() - 1) / memoryManager->getFrameSize();
                memoryManager->processPageTables[name] = std::vector<MemoryManager::PageTableEntry>(numPages);
            }
            if (scheduler) {
                scheduler->addProcessWithMemory(p, memorySize);
                {
                    std::lock_guard<std::mutex> lock(processMutex);
                    ProcessInfo info(nextProcessId++, name);
                    info.coreID = -1;
                    info.progress = 0;
                    info.total = p->totalCommands;
                    info.finished = false;
                    processList.push_back(info);
                }
                std::cout << "Process '" << name << "' created successfully with instructions." << std::endl;
            } else {
                delete p;
            }
        } catch (const std::exception& e) {
            std::cout << "Error creating process '" << name << "': " << e.what() << std::endl;
            printEnter();
            return false;
        }
    }
    else if (command == "process-smi") {
        // Show a summary of memory and process usage
        if (!memoryManager) {
            std::cout << "Memory manager not initialized!" << std::endl;
            printEnter();
            return false;
        }
        // Example output format (customize as needed)
        int totalMem = 0, usedMem = 0;
        int procCount = 0;
        auto layout = memoryManager->getMemoryLayout();
        for (const auto& block : layout) {
            totalMem += block.size;
            if (block.isAllocated) usedMem += block.size;
        }
        double memUtil = totalMem ? (usedMem * 100.0 / totalMem) : 0.0;
        std::cout << "| PROCESS-SMI V01.00 Driver Version: 01.00 |" << std::endl;
        std::cout << "CPU-Util: 100%" << std::endl;
        std::cout << "Memory Usage: " << usedMem / 1024 << "MiB / " << totalMem / 1024 << "MiB" << std::endl;
        std::cout << "Memory Util: " << (int)memUtil << "%" << std::endl;
        std::cout << "\nRunning processes and memory usage:" << std::endl;
        for (const auto& block : layout) {
            if (block.isAllocated) {
                std::cout << block.processName << " " << block.size / 1024 << "MiB" << std::endl;
                ++procCount;
            }
        }
        if (procCount == 0) std::cout << "(none)" << std::endl;
        printEnter();
        return true;
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

    scheduler = new Scheduler(global_core_count, global_algo, memoryManager, global_mem_per_proc, global_quantum);
    std::cout << "Scheduler initialized: ";
    std::cout << (global_algo == SchedulingAlgorithm::FCFS ? "FCFS" : "RR");
    std::cout << ", cores: " << global_core_count << ", quantum: " << global_quantum << std::endl;
    std::cout << "Memory manager: " << global_mem_per_proc << " bytes per process" << std::endl;
    initialized = true;
    printEnter();
}

void CommandHandler::schedulerStart() {
    if (!scheduler) {
        std::cout << "Run 'initialize' first!" << std::endl;
        printEnter();
        return;
    }

    scheduler->start();
    schedulerRunning = true;

    for (int i = 1; i <= 10; ++i) {
        char buf[16];
        snprintf(buf, sizeof(buf), "P%d", i);
        generateDummyProcess(buf);
    }

    if (!generatorRunning) {
        generatorRunning = true;
        processGeneratorThread = std::thread([this]() {
            int processCount = 11;
            while (generatorRunning && schedulerRunning) {
                char buf[16];
                snprintf(buf, sizeof(buf), "P%d", processCount);
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

    // Reset CPU cycle counters when scheduler stops
    totalCpuCycles = 0;
    activeCpuCycles = 0;
    coresUsed = 0;

    delete scheduler;
    scheduler = nullptr;

    coresUsed.store(0);

    std::cout << "Scheduler stopped." << std::endl;
    printEnter();
}

void CommandHandler::screenList() {
    showProcessList();
    printEnter();
}

void CommandHandler::screenS(const std::string& name, int memorySize) {
    if (name.empty()) {
        std::cout << "Error: screen name cannot be empty." << std::endl;
        printEnter();
        return;
    }

    // Validate process name
    if (name.find_first_of("\\/:*?\"<>|") != std::string::npos) {
        std::cout << "Error: Invalid process name. Cannot contain \\/:*?\"<>|" << std::endl;
        printEnter();
        return;
    }

    // Validate memory size if provided
    if (memorySize != -1) {
        if (!isValidMemorySize(memorySize)) {
            std::cout << "Error: Invalid memory allocation. Memory size must be between 64 and 65536 bytes and be a power of 2." << std::endl;
            printEnter();
            return;
        }
    }

    // Check if process exists; if not, create it
    auto it = std::find_if(processList.begin(), processList.end(), [&](const ProcessInfo& p) { return p.name == name; });
    if (it == processList.end()) {
        try {
            if (memorySize != -1) {
                generateDummyProcessWithMemory(name, memorySize);
                std::cout << "Process '" << name << "' created successfully with " << memorySize << " bytes of memory." << std::endl;
            } else {
                generateDummyProcess(name);
                std::cout << "Process '" << name << "' created successfully." << std::endl;
            }
        }
        catch (const std::exception& e) {
            std::cout << "Error creating process '" << name << "': " << e.what() << std::endl;
            printEnter();
            return;
        }
    }

    if (screenMap.count(name)) {
        std::cout << "Screen '" << name << "' already exists." << std::endl;
    }
    else {
        screenMap[name] = Console(name);
        std::cout << "Created screen: " << name << std::endl;
    }

    bool inScreen = true;
    system("cls");
    // Display process info and logs
    it = std::find_if(processList.begin(), processList.end(), [&](const ProcessInfo& p) { return p.name == name; });
    if (it != processList.end()) {
        std::cout << "\n=== Screen for Process: " << it->name << " ===" << std::endl;
        std::cout << "Process ID: " << it->id << std::endl;
        std::cout << "Instruction: " << it->progress + 1 << " / " << it->total << std::endl;
        std::cout << "Started at: " << it->startTime << std::endl;
        // Show last 10 lines of log
        std::ifstream logFile("process_logs/" + name + ".txt");
        if (logFile) {
            std::vector<std::string> lines;
            std::string line;
            while (std::getline(logFile, line)) lines.push_back(line);
            std::cout << "--- Last 10 PRINT logs ---" << std::endl;
            for (int i = std::max(0, (int)lines.size() - 10); i < (int)lines.size(); ++i) {
                std::cout << lines[i] << std::endl;
            }
        }
        else {
            std::cout << "--- No PRINT logs yet ---" << std::endl;
        }
    }
    else {
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
        }
        else if (input == "process-smi") {
            inScreen = true;
            processSmi(name);
        }
        else {
            std::cout << "Unknown command in screen '" << name << "'. Type 'exit' to leave the screen." << std::endl;
        }
    }
}

void CommandHandler::screenR(const std::string& name) {
    if (name.empty()) {
        std::cout << "Error: screen name cannot be empty." << std::endl;
        printEnter();
        return;
    }

    // Validate process name
    if (name.find_first_of("\\/:*?\"<>|") != std::string::npos) {
        std::cout << "Error: Invalid process name. Cannot contain \\/:*?\"<>|" << std::endl;
        printEnter();
        return;
    }

    auto it = std::find_if(processList.begin(), processList.end(), [&](const ProcessInfo& p) { return p.name == name && !p.finished; });
    if (it == processList.end()) {
        std::cout << "Process " << name << " not found or already finished" << std::endl;
        printEnter();
        return;
    }

    bool inScreen = true;
    system("cls");
    std::cout << "\n=== Screen for Process: " << it->name << " ===" << std::endl;
    std::cout << "Process ID: " << it->id << std::endl;
    std::cout << "Instruction: " << it->progress << " / " << it->total << std::endl;
    std::cout << "Started at: " << it->startTime << std::endl;
    // Show last 10 lines of log
    std::ifstream logFile("process_logs/" + name + ".txt");
    if (logFile) {
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(logFile, line)) lines.push_back(line);
        std::cout << "--- Last 10 PRINT logs ---" << std::endl;
        for (int i = std::max(0, (int)lines.size() - 10); i < (int)lines.size(); ++i) {
            std::cout << lines[i] << std::endl;
        }
    }
    else {
        std::cout << "--- No PRINT logs yet ---" << std::endl;
    }

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
            processSmi(name);
        }
        else if (input == "vmstat") {
            vmstat();
        }
        else {
            std::cout << "Unknown command in screen '" << name << "'. Type 'exit' to leave the screen." << std::endl;
        }
    }
}

void CommandHandler::reportUtil() {
    // Calculate utilization based on current state
    double utilization = 0.0;
    int used = coresUsed.load();

    if (used > 0) {
        utilization = (double)used / global_core_count * 100.0;
    }
    else {
        // If no cores are used, utilization 0%
        utilization = 0.0;
    }

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
                reportFile << p.name << "\t(" << p.startTime << ")";
                if (p.coreID == -1) {
                    reportFile << "  Core: N/A";
                }
                else {
                    reportFile << "  Core: " << p.coreID;
                }
                reportFile << "  " << p.progress << " / " << p.total << std::endl;
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
void CommandHandler::vmstat() {
    if (!memoryManager) {
        std::cout << "Memory manager not initialized!" << std::endl;
        printEnter();
        return;
    }

    std::cout << "\n=== Memory Statistics (vmstat) ===" << std::endl;

    // Get memory layout information
    auto layout = memoryManager->getMemoryLayout();
    int totalMemory = 0;
    int usedMemory = 0;
    int freeMemory = 0;
    int framesUsed = 0;
    int framesFree = 0;

    for (const auto& block : layout) {
        totalMemory += block.size;
        if (block.isAllocated) {
            usedMemory += block.size;
            framesUsed += (block.size + memoryManager->getFrameSize() - 1) / memoryManager->getFrameSize();
        }
        else {
            freeMemory += block.size;
            framesFree += (block.size + memoryManager->getFrameSize() - 1) / memoryManager->getFrameSize();
        }
    }

    // Display memory information
    std::cout << "Total memory: " << totalMemory << " bytes" << std::endl;
    std::cout << "Used memory: " << usedMemory << " bytes (" << (usedMemory * 100 / totalMemory) << "%)" << std::endl;
    std::cout << "Free memory: " << freeMemory << " bytes (" << (freeMemory * 100 / totalMemory) << "%)" << std::endl;
    std::cout << "Frames used: " << framesUsed << std::endl;
    std::cout << "Frames free: " << framesFree << std::endl;

    // CPU utilization (using existing counters)
    double utilization = (activeCpuCycles * 100.0) / totalCpuCycles;
    std::cout << "\nCPU utilization: " << std::fixed << std::setprecision(2) << utilization << "%" << std::endl;
    std::cout << "Active cycles: " << activeCpuCycles << std::endl;
    std::cout << "Total cycles: " << totalCpuCycles << std::endl;

    printEnter();
}

void CommandHandler::processSmi(const std::string& processName) {
    // Find the process in the process list
    auto it = std::find_if(processList.begin(), processList.end(),
        [&](const ProcessInfo& p) { return p.name == processName; });

    if (it != processList.end()) {
        std::cout << "\nProcess name: " << it->name << std::endl;
        std::cout << "ID: " << it->id << std::endl;
        std::cout << "Logs:" << std::endl;

        // Read and display the last log entry
        std::ifstream logFile("process_logs/" + it->name + ".txt");
        if (logFile) {
            std::vector<std::string> lines;
            std::string line;
            while (std::getline(logFile, line)) {
                lines.push_back(line);
            }
            if (!lines.empty()) {
                // Display the last log entry
                std::cout << lines.back() << std::endl;
            }
        }

        std::cout << "Current instruction line: " << it->progress << std::endl;
        std::cout << "Lines of code: " << it->total << std::endl;

        if (it->finished) {
            std::cout << "Finished!" << std::endl;
        }
    }
    else {
        std::cout << "Process not found in process list." << std::endl;
    }
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
    std::cout << Default << "\nLast Updated: " << Y << "06-28-2025" << Default << std::endl;
    std::cout << Default << "\n" << std::string(50, '-') << Default << std::endl;
    std::cout << Y << "Type 'exit' to quit, 'clear' to clear the screen" << Default << std::endl;
    std::cout << "root:\\> ";
}

bool CommandHandler::isPowerOfTwo(int n) const {
    return n > 0 && (n & (n - 1)) == 0;
}

bool CommandHandler::isValidMemorySize(int size) const {
    return size >= 64 && size <= 65536 && isPowerOfTwo(size);
}

void CommandHandler::printEnter() {
    std::cout << "\nroot:\\> ";
}