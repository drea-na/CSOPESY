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
#include <sstream> // Required for std::istringstream

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
extern int nextProcessId;

//external declarations
extern void readConfig();
extern void generateDummyProcess(const std::string& name);
extern void generateDummyProcessWithMemory(const std::string& name, int memorySize);
extern void showProcessList();

bool CommandHandler::handleCommands(const std::string& command) {
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
    else if (command.substr(0, 10) == "screen -c ") {
        std::string remaining = command.substr(10);
        size_t firstSpace = remaining.find(' ');
        if (firstSpace == std::string::npos) {
            std::cout << "Error: Invalid command format. Usage: screen -c <process_name> [<memory_size>] \"<instructions>\"" << std::endl;
            printEnter();
            return false;
        }

        std::string name = remaining.substr(0, firstSpace);
        remaining = remaining.substr(firstSpace + 1);

        size_t quoteStart = remaining.find('"');
        if (quoteStart == std::string::npos) {
            std::cout << "Error: Instructions must be enclosed in double quotes." << std::endl;
            printEnter();
            return false;
        }

        std::string beforeQuote = remaining.substr(0, quoteStart);
        std::string instructionsStr = remaining.substr(quoteStart);

        // Remove quotes around instructions
        if (!instructionsStr.empty() && instructionsStr.front() == '"' && instructionsStr.back() == '"') {
            instructionsStr = instructionsStr.substr(1, instructionsStr.size() - 2);
        }

        int memorySize = 1024; // Default
        try {
            if (!beforeQuote.empty()) {
                memorySize = std::stoi(beforeQuote);
                if (!isValidMemorySize(memorySize)) {
                    std::cout << "Error: Invalid memory allocation. Must be 64-65536 and power of 2." << std::endl;
                    printEnter();
                    return false;
                }
            }
        }
        catch (...) {
            std::cout << "Error: Invalid memory size format." << std::endl;
            printEnter();
            return false;
        }

        // Split instructions by semicolon
        std::vector<std::string> instrs;
        size_t start = 0, end = 0;
        while ((end = instructionsStr.find(';', start)) != std::string::npos) {
            std::string instr = instructionsStr.substr(start, end - start);
            if (!instr.empty()) instrs.push_back(instr);
            start = end + 1;
        }
        if (start < instructionsStr.size()) {
            std::string instr = instructionsStr.substr(start);
            if (!instr.empty()) instrs.push_back(instr);
        }

        if (instrs.size() < 1 || instrs.size() > 50) {
            std::cout << "Error: Instruction count must be between 1 and 50." << std::endl;
            printEnter();
            return false;
        }

        Process* p = new Process(name);
        p->memoryManager = memoryManager;
        p->memorySize = memorySize;

        for (std::string line : instrs) {
            std::istringstream iss(line);
            std::string cmd;
            iss >> cmd;

            Process::Instruction instr;

            if (cmd == "PRINT") {
                instr.type = Process::InstrType::PRINT;

                std::string rest;
                std::getline(iss, rest);

                size_t openParen = rest.find('(');
                size_t closeParen = rest.rfind(')');

                if (openParen != std::string::npos && closeParen != std::string::npos && closeParen > openParen) {
                    std::string expr = rest.substr(openParen + 1, closeParen - openParen - 1);

                    // Split by '+' while respecting quoted strings
                    std::vector<std::string> parts;
                    std::string current;
                    bool inQuotes = false;
                    for (char ch : expr) {
                        if (ch == '"') {
                            inQuotes = !inQuotes;
                        }
                        if (ch == '+' && !inQuotes) {
                            if (!current.empty()) {
                                current.erase(0, current.find_first_not_of(" \t"));
                                current.erase(current.find_last_not_of(" \t") + 1);
                                parts.push_back(current);
                                current.clear();
                            }
                        }
                        else {
                            current += ch;
                        }
                    }
                    if (!current.empty()) {
                        current.erase(0, current.find_first_not_of(" \t"));
                        current.erase(current.find_last_not_of(" \t") + 1);
                        parts.push_back(current);
                    }

                    for (const std::string& part : parts) {
                        instr.args.push_back(part);
                    }
                }
                else {
                    std::cout << "Error: Invalid PRINT format." << std::endl;
                    delete p;
                    printEnter();
                    return false;
                }
            }
            else if (cmd == "DECLARE") {
                instr.type = Process::InstrType::DECLARE;
                std::string var, val;
                if (iss >> var >> val) {
                    instr.args.push_back(var);
                    instr.args.push_back(val);
                }
            }
            else if (cmd == "ADD") {
                instr.type = Process::InstrType::ADD;
                std::string a, b, c;
                if (iss >> a >> b >> c) {
                    instr.args = { a, b, c };
                }
            }
            else if (cmd == "SUBTRACT") {
                instr.type = Process::InstrType::SUBTRACT;
                std::string a, b, c;
                if (iss >> a >> b >> c) {
                    instr.args = { a, b, c };
                }
            }
            else if (cmd == "SLEEP") {
                instr.type = Process::InstrType::SLEEP;
                std::string ticks;
                if (iss >> ticks) instr.args.push_back(ticks);
            }
            else if (cmd == "READ") {
                instr.type = Process::InstrType::READ;
                std::string var, addr;
                if (iss >> var >> addr) {
                    instr.args = { var, addr };
                }
            }
            else if (cmd == "WRITE") {
                instr.type = Process::InstrType::WRITE;
                std::string addr, val;
                if (iss >> addr >> val) {
                    instr.args = { addr, val };
                }
            }
            else {
                std::cout << "Error: Unknown instruction '" << cmd << "'." << std::endl;
                delete p;
                printEnter();
                return false;
            }

            p->instructions.push_back(instr);
        }

        p->totalCommands = p->instructions.size();
        p->executedCommands = 0;

        if (scheduler) {
            scheduler->addProcessWithMemory(p, memorySize);
            std::lock_guard<std::mutex> lock(processMutex);
            ProcessInfo info(nextProcessId++, name);
            info.coreID = -1;
            info.progress = 0;
            info.total = p->totalCommands;
            info.finished = false;
            processList.push_back(info);
        }
        else {
            delete p;
        }

        std::cout << "Process '" << name << "' created successfully with " << memorySize << " bytes of memory." << std::endl;
    }
    else if (command.substr(0, 10) == "screen -s ") {
        std::string remaining = command.substr(10);
        size_t spacePos = remaining.find(' ');
        if (spacePos != std::string::npos) {
            std::string name = remaining.substr(0, spacePos);
            std::string memorySizeStr = remaining.substr(spacePos + 1);
            try {
                int memorySize = std::stoi(memorySizeStr);
                screenS(name, memorySize);
            }
            catch (...) {
                std::cout << "Error: Invalid memory size format." << std::endl;
                printEnter();
                return false;
            }
        }
        else {
            screenS(remaining);
        }
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
    // Check for memory access violation
    if (it->accessViolation) {
        std::cout << "Process " << name << " shut down due to memory access violation error that occurred at "
                  << it->violationTime << ". 0x" << std::hex << it->violationAddress << " invalid." << std::endl;
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
    } else {
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
        } else if (input == "process-smi") {
            inScreen = true;
            processSmi(name);
        } else if (input == "vmstat") {
            vmstat();
        } else {
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