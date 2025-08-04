#include <iostream>
#include <string>
#include <cstdlib>
#include <map>
#include <vector>
#include <mutex>
#include <thread>
#include <queue>
#include <fstream>
#include <chrono>
#include <iomanip>
#include "Console.h"
#include "Scheduler.h"
#include "CommandHandler.h"

// Declare the screenMap variable
std::map<std::string, Console> screenMap;

std::queue<Console> readyQueue;
std::mutex queueMutex;
std::condition_variable cv;
bool schedulerRunning = false;

// global variables
std::vector<ProcessInfo> processList;
std::mutex processMutex;

// Scheduler globals
Scheduler* scheduler = nullptr;
int global_core_count = 4;
int global_quantum = 2;
SchedulingAlgorithm global_algo = SchedulingAlgorithm::FCFS;
int global_batch_process_freq = 1;
int global_min_ins = 1000;
int global_max_ins = 2000;
int global_delay_per_exec = 0;

std::atomic<int> coresUsed(0);
std::atomic<int> totalCpuCycles(0);
std::atomic<int> activeCpuCycles(0);

std::thread processGeneratorThread;
std::atomic<bool> generatorRunning(false);

// global process ID counter
int nextProcessId = 0;

// Read config.txt
void readConfig() {
    std::ifstream fin("config.txt");
    if (!fin) {
        std::cout << "config.txt not found. Using defaults." << std::endl;
        return;
    }
    
    try {
        std::string key;
        while (fin >> key) {
            if (key == "num-cpu") fin >> global_core_count;
            else if (key == "scheduler") {
                std::string val; fin >> val;
                if (val == "fcfs") global_algo = SchedulingAlgorithm::FCFS;
                else if (val == "rr") global_algo = SchedulingAlgorithm::RR;
            }
            else if (key == "quantum-cycles") fin >> global_quantum;
            else if (key == "batch-process-freq") fin >> global_batch_process_freq;
            else if (key == "min-ins") fin >> global_min_ins;
            else if (key == "max-ins") fin >> global_max_ins;
            else if (key == "delay-per-exec") fin >> global_delay_per_exec;
            // Add more config as needed
            else fin.ignore(1000, '\n');
        }
    } catch (const std::exception& e) {
        std::cout << "Error reading config.txt: " << e.what() << ". Using defaults." << std::endl;
    }
    
    // Validation
    if (global_core_count < 1 || global_core_count > 128) {
        std::cout << "[config.txt] num-cpu out of range (1-128). Using default (4)." << std::endl;
        global_core_count = 4;
    }
    if (global_quantum < 1) {
        std::cout << "[config.txt] quantum-cycles out of range (>=1). Using default (2)." << std::endl;
        global_quantum = 2;
    }
    if (global_batch_process_freq < 1) {
        std::cout << "[config.txt] batch-process-freq out of range (>=1). Using default (1)." << std::endl;
        global_batch_process_freq = 1;
    }
    if (global_min_ins < 1) {
        std::cout << "[config.txt] min-ins out of range (>=1). Using default (1000)." << std::endl;
        global_min_ins = 1000;
    }
    if (global_max_ins < global_min_ins) {
        std::cout << "[config.txt] max-ins less than min-ins. Setting max-ins = min-ins." << std::endl;
        global_max_ins = global_min_ins;
    }
    if (global_delay_per_exec < 0) {
        std::cout << "[config.txt] delay-per-exec out of range (>=0). Using default (0)." << std::endl;
        global_delay_per_exec = 0;
    }
}

void generateDummyProcess(const std::string& name) {
    // Place process logs in process_logs/
    system("mkdir process_logs >nul 2>&1"); // Windows: suppress output if exists
    Process* p = new Process(name);
    p->generateRandomInstructions(global_min_ins, global_max_ins);
    if (scheduler) {
        scheduler->addProcess(p);
        // Add to process tracking list
        {
            std::lock_guard<std::mutex> lock(processMutex);
            ProcessInfo info(nextProcessId++, name);
            info.coreID = -1; // Not assigned to core yet
            info.progress = 0;
            info.total = p->totalCommands;
            info.finished = false;
            processList.push_back(info);
        }
    } else {
        delete p;
    }
}

void showProcessList() {
    std::lock_guard<std::mutex> lock(processMutex);

    double utilization = 0.0;
    int used = coresUsed.load();
    
    
    if (used > 0) {
        utilization = (double)used / global_core_count * 100.0;
    } else {
        // If no cores are used, utilization 0%
        utilization = 0.0;
    }

    int available = global_core_count - used;

    std::cout << "CPU utilization: " << std::fixed << std::setprecision(2) << utilization << "%" << std::endl;
    std::cout << "Cores used: " << used << std::endl;
    std::cout << "Cores available: " << available << std::endl;

    std::cout << "\n-------------------------------" << std::endl;
    std::cout << "Running processes:\n";

    for (const auto& p : processList) {
        if (!p.finished) {
            std::cout << p.name << "\t(" << p.startTime << ")";
            if (p.coreID == -1) {
                std::cout << "  Core: N/A";
            } else {
                std::cout << "  Core: " << p.coreID;
            }
            std::cout << "  " << p.progress << " / " << p.total << std::endl;
        }
    }

    std::cout << "\nFinished processes:\n";
    for (const auto& p : processList) {
        if (p.finished) {
            std::cout << p.name << "\t(" << p.startTime << ")"
                << "  Finished  " << p.total << " / " << p.total << std::endl;
        }
    }

    std::cout << "-------------------------------\n";
}


int main() {
    CommandHandler handler(screenMap, scheduler);
    bool running = true;
    std::string command;
    int cpuCycles = 0;

    handler.printHeader();
    while (running) {
        std::getline(std::cin, command);
        if (command == "exit") {
            std::cout << "Shutting down... bye bye" << std::endl;
            if (scheduler) delete scheduler;
            running = false;
        } else {
            handler.handleCommands(command);
        }
        cpuCycles++;
    }

    return 0;
}
