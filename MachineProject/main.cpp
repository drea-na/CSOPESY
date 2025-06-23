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

// Read config.txt
void readConfig() {
    std::ifstream fin("config.txt");
    if (!fin) {
        std::cout << "config.txt not found. Using defaults." << std::endl;
        return;
    }
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
}

void generateDummyProcess(const std::string& name) {
    // Place process logs in process_logs/
    system("mkdir process_logs >nul 2>&1"); // Windows: suppress output if exists
    Process* p = new Process("process_logs/" + name, true);
    if (scheduler) {
        scheduler->addProcess(p);

        // Add to process tracking list
        {
            std::lock_guard<std::mutex> lock(processMutex);
            ProcessInfo info;
            info.name = name;
            info.startTime = Console().getCurrentTimestamp();
            info.coreID = -1; // Not assigned to core yet
            info.progress = 0;
            info.total = 100;
            info.finished = false;
            processList.push_back(info);
        }
    }
    else {
        delete p; // Clean up if no scheduler available
    }
}

void showProcessList() {
    std::lock_guard<std::mutex> lock(processMutex);

    double utilization = 0.0;
    if (totalCpuCycles > 0) {
        utilization = (double)activeCpuCycles / totalCpuCycles * 100.0;
    }

    int used = coresUsed.load();
    int available = global_core_count - used;

    std::cout << "CPU utilization: " << std::fixed << std::setprecision(2) << utilization << "%" << std::endl;
    std::cout << "Cores used: " << used << std::endl;
    std::cout << "Cores available: " << available << std::endl;

    std::cout << "\n-------------------------------" << std::endl;
    std::cout << "Running processes:\n";

    for (const auto& p : processList) {
        if (!p.finished) {
            std::cout << p.name << "\t(" << p.startTime << ")"
                << "  Core: " << p.coreID
                << "  " << p.progress << " / " << p.total << std::endl;
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

void cpuWorker(int coreID) {
    while (schedulerRunning) {
        Console currentProcess;

        {
            std::unique_lock<std::mutex> lock(queueMutex);
            cv.wait(lock, [] { return !readyQueue.empty() || !schedulerRunning; });

            if (!schedulerRunning && readyQueue.empty()) return;

            currentProcess = readyQueue.front();
            readyQueue.pop();
        }

        // Simulate executing 100 print commands
        for (int i = 1; i <= 100; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10)); // simulate work

            // Log to file
            std::ofstream outFile(currentProcess.getName() + ".txt", std::ios::app);
            auto now = Console().getCurrentTimestamp();
            outFile << "(" << now << ") Core:" << coreID << " - Hello world from " << currentProcess.getName() << "!\n";
            outFile.close();

            // Update progress
            {
                std::lock_guard<std::mutex> lock(processMutex);
                for (auto& info : processList) {
                    if (info.name == currentProcess.getName()) {
                        info.coreID = coreID;
                        info.progress = i;
                        if (i == 100) info.finished = true;
                        break;
                    }
                }
            }
        }
    }
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
        }
        else {
            handler.handleCommands(command);
        }

        cpuCycles++;
    }


    return 0;
}
