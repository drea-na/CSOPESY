#include "CommandHandler.h"
#include "Process.h"
#include "MemoryManager.h"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <map>
#include <mutex>
#include <vector>
#include <queue>
#include <algorithm>
#include <condition_variable>
#include <sstream>

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
extern void generateDummyProcess(const std::string& name, int memorySize = 0);
extern void showProcessList();
extern int nextProcessId;
extern MemoryManager* memoryManager;
extern int global_min_mem_per_proc;
extern int global_max_mem_per_proc;

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
        std::string rest = command.substr(10);
        size_t spacePos = rest.find(' ');
        if (spacePos != std::string::npos) {
            std::string name = rest.substr(0, spacePos);
            int memorySize = std::stoi(rest.substr(spacePos + 1));
            screenS(name, memorySize);
        } else {
            screenS(rest);
        }
    }
    else if (command.substr(0, 10) == "screen -c ") {
        std::string rest = command.substr(10);
        size_t firstSpace = rest.find(' ');
        if (firstSpace != std::string::npos) {
            std::string name = rest.substr(0, firstSpace);
            size_t secondSpace = rest.find(' ', firstSpace + 1);
            if (secondSpace != std::string::npos) {
                int memorySize = std::stoi(rest.substr(firstSpace + 1, secondSpace - firstSpace - 1));
                std::string instructions = rest.substr(secondSpace + 1);
                screenC(name, memorySize, instructions);
            } else {
                std::cout << "Error: Invalid screen -c format. Use: screen -c <name> <memory_size> \"<instructions>\"" << std::endl;
                printEnter();
            }
        } else {
            std::cout << "Error: Invalid screen -c format. Use: screen -c <name> <memory_size> \"<instructions>\"" << std::endl;
            printEnter();
        }
    }
    else if (command == "vmstat") {
        vmstat();
    }
    else if (command == "process-smi") {
        processSmi("");
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
        return;
    }

    scheduler->start();
    schedulerRunning = true;

    for (int i = 1; i <= 10; ++i) {
        char buf[16];
        snprintf(buf, sizeof(buf), "process%02d", i);
        generateDummyProcess(buf, 0);
    }

    if (!generatorRunning) {
        generatorRunning = true;
        processGeneratorThread = std::thread ([this]() {
            int processCount = 11;
            while (generatorRunning && schedulerRunning) {
                char buf[16];
                snprintf(buf, sizeof(buf), "process%02d", processCount);
                generateDummyProcess(buf, 0);
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
    
    // Validate memory size if specified
    if (memorySize > 0) {
        if (!memoryManager || !memoryManager->isValidMemorySize(memorySize)) {
            std::cout << "Error: Invalid memory size " << memorySize << " bytes. Must be power of 2 between 64 and 65536 bytes." << std::endl;
            printEnter();
            return;
        }
    }
    
    // Check if process exists; if not, create it
    auto it = std::find_if(processList.begin(), processList.end(), [&](const ProcessInfo& p){ return p.name == name; });
    if (it == processList.end()) {
        try {
            generateDummyProcess(name, memorySize);
            std::cout << "Process '" << name << "' created successfully with " << memorySize << " bytes memory." << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Error creating process '" << name << "': " << e.what() << std::endl;
            printEnter();
            return;
        }
    }
    
    if (screenMap.count(name)) {
        std::cout << "Screen '" << name << "' already exists." << std::endl;
    } else {
        screenMap[name] = Console(name);
        std::cout << "Created screen: " << name << std::endl;
    }
    
    bool inScreen = true;
    system("cls");
    // Display process info and logs
    it = std::find_if(processList.begin(), processList.end(), [&](const ProcessInfo& p){ return p.name == name; });
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
            for (int i = std::max(0, (int)lines.size()-10); i < (int)lines.size(); ++i) {
                std::cout << lines[i] << std::endl;
            }
        } else {
            std::cout << "--- No PRINT logs yet ---" << std::endl;
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
            processSmi(name);
        } else {
            std::cout << "Unknown command in screen '" << name << "'. Type 'exit' to leave the screen." << std::endl;
        }
    }
    
    // Check for memory access violations in logs
    std::ifstream logFile("process_logs/" + name + ".txt");
    if (logFile) {
        std::string line;
        while (std::getline(logFile, line)) {
            if (line.find("Memory access violation") != std::string::npos) {
                // Extract timestamp and address from the log
                size_t timeStart = line.find("(");
                size_t timeEnd = line.find(")");
                if (timeStart != std::string::npos && timeEnd != std::string::npos) {
                    std::string timestamp = line.substr(timeStart + 1, timeEnd - timeStart - 1);
                    size_t addrStart = line.find("0x");
                    if (addrStart != std::string::npos) {
                        std::string address = line.substr(addrStart, line.find(" ", addrStart) - addrStart);
                        std::cout << "Process " << name << " shut down due to memory access violation error that occurred at " 
                                  << timestamp << ". " << address << " invalid." << std::endl;
                    }
                }
            }
        }
    }
}

void CommandHandler::screenC(const std::string& name, int memorySize, const std::string& instructions) {
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
    
    // Validate memory size
    if (!memoryManager || !memoryManager->isValidMemorySize(memorySize)) {
        std::cout << "Error: Invalid memory size " << memorySize << " bytes. Must be power of 2 between 64 and 65536 bytes." << std::endl;
        printEnter();
        return;
    }
    
    // Parse instructions (1-50 semicolon-separated)
    std::vector<std::string> instructionList;
    std::stringstream ss(instructions);
    std::string instruction;
    int count = 0;
    
    while (std::getline(ss, instruction, ';') && count < 50) {
        // Trim whitespace
        instruction.erase(0, instruction.find_first_not_of(" \t"));
        instruction.erase(instruction.find_last_not_of(" \t") + 1);
        if (!instruction.empty()) {
            instructionList.push_back(instruction);
            count++;
        }
    }
    
    if (instructionList.empty()) {
        std::cout << "Error: No valid instructions provided." << std::endl;
        printEnter();
        return;
    }
    
    if (count >= 50) {
        std::cout << "Warning: Maximum 50 instructions allowed. Only first 50 will be used." << std::endl;
    }
    
    // Create process with custom instructions
    try {
        // Create process with memory allocation
        int processId = nextProcessId++;
        
        if (!memoryManager->allocateProcessMemory(processId, name, memorySize)) {
            std::cout << "Error: Failed to allocate memory for process " << name << std::endl;
            printEnter();
            return;
        }
        
        Process* p = new Process(name, processId, memoryManager, memorySize);
        
        // Parse and add custom instructions
        for (const std::string& instr : instructionList) {
            // Simple instruction parsing - this would need to be enhanced for full instruction support
            std::stringstream instrStream(instr);
            std::string op;
            instrStream >> op;
            
            if (op == "DECLARE") {
                std::string varName, value;
                instrStream >> varName >> value;
                p->addInstruction(Process::InstrType::DECLARE, {varName, value});
            } else if (op == "ADD") {
                std::string var1, var2, var3;
                instrStream >> var1 >> var2 >> var3;
                p->addInstruction(Process::InstrType::ADD, {var1, var2, var3});
            } else if (op == "SUBTRACT") {
                std::string var1, var2, var3;
                instrStream >> var1 >> var2 >> var3;
                p->addInstruction(Process::InstrType::SUBTRACT, {var1, var2, var3});
            } else if (op == "PRINT") {
                std::string varName;
                instrStream >> varName;
                p->addInstruction(Process::InstrType::PRINT, {varName});
            } else if (op == "SLEEP") {
                std::string ticks;
                instrStream >> ticks;
                p->addInstruction(Process::InstrType::SLEEP, {ticks});
            } else if (op == "READ") {
                std::string varName, address;
                instrStream >> varName >> address;
                p->addInstruction(Process::InstrType::READ, {varName, address});
            } else if (op == "WRITE") {
                std::string address, value;
                instrStream >> address >> value;
                p->addInstruction(Process::InstrType::WRITE, {address, value});
            }
        }
        
        if (scheduler) {
            scheduler->addProcess(p);
            // Add to process tracking list
            {
                std::lock_guard<std::mutex> lock(processMutex);
                ProcessInfo info(processId, name);
                info.coreID = -1;
                info.progress = 0;
                info.total = p->totalCommands;
                info.finished = false;
                processList.push_back(info);
            }
            std::cout << "Process '" << name << "' created successfully with " << memorySize << " bytes memory and " << instructionList.size() << " custom instructions." << std::endl;
        } else {
            memoryManager->deallocateProcessMemory(processId);
            delete p;
            std::cout << "Error: Scheduler not initialized." << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cout << "Error creating process '" << name << "': " << e.what() << std::endl;
    }
    
    printEnter();
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
    
    auto it = std::find_if(processList.begin(), processList.end(), [&](const ProcessInfo& p){ return p.name == name && !p.finished; });
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
        for (int i = std::max(0, (int)lines.size()-10); i < (int)lines.size(); ++i) {
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
    } else {
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
                } else {
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

void CommandHandler::processSmi(const std::string& processName) {
    if (processName.empty()) {
        // Global memory summary
        if (memoryManager) {
            memoryManager->printProcessSmi();
        } else {
            std::cout << "Memory manager not initialized." << std::endl;
        }
    } else {
        // Process-specific information
        auto it = std::find_if(processList.begin(), processList.end(), 
                              [&](const ProcessInfo& p){ return p.name == processName; });
        
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
        } else {
            std::cout << "Process not found in process list." << std::endl;
        }
    }
    printEnter();
}

void CommandHandler::vmstat() {
    if (memoryManager) {
        memoryManager->printVmstat();
    } else {
        std::cout << "Memory manager not initialized." << std::endl;
    }
    printEnter();
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

void CommandHandler::printEnter() {
    std::cout << "\nroot:\\> ";
}