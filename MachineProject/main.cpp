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
#include <atomic>
#include <condition_variable>
#include <iomanip>
#include "Console.h"
#include "Scheduler.h"
#include "CommandHandler.h"

std::map<std::string, Console> screenMap;
std::queue<Console> readyQueue;
std::mutex queueMutex;
std::condition_variable cv;
bool schedulerRunning = false;

std::vector<ProcessInfo> processList;
std::mutex processMutex;

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
int nextProcessId = 0;

void readConfig() {
    std::ifstream fin("C:\\Users\\hialo\\Documents\\GitHub\\CSOPESY-MO\\CSOPESY\\MachineProject\\config.txt");
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
        else fin.ignore(1000, '\n');
    }

    if (global_core_count < 1 || global_core_count > 128) global_core_count = 4;
    if (global_quantum < 1) global_quantum = 2;
    if (global_batch_process_freq < 1) global_batch_process_freq = 1;
    if (global_min_ins < 1) global_min_ins = 1000;
    if (global_max_ins < global_min_ins) global_max_ins = global_min_ins;
    if (global_delay_per_exec < 0) global_delay_per_exec = 0;
}

void generateDummyProcess(const std::string& name) {
    system("mkdir process_logs >nul 2>&1");

    Process* p = new Process(name, true);
    p->generateRandomInstructions(global_min_ins, global_max_ins);

    if (scheduler) {
        scheduler->addProcess(p);
        std::lock_guard<std::mutex> lock(processMutex);
        ProcessInfo info;
        info.id = nextProcessId++;
        info.name = name;
        info.startTime = Console().getCurrentTimestamp();
        info.coreID = -1;
        info.progress = 0;
        info.total = p->totalCommands;
        info.finished = false;
        processList.push_back(info);
    }
    else {
        delete p;
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
            std::cout << p.name << "\t(" << p.startTime << ")";
            std::cout << "  Core: " << (p.coreID == -1 ? "N/A" : std::to_string(p.coreID));
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

        coresUsed++;
        for (int i = 1; i <= 100; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            totalCpuCycles++;
            activeCpuCycles++;

            std::ofstream outFile("process_logs/" + currentProcess.getName() + ".txt", std::ios::app);
            auto now = Console().getCurrentTimestamp();
            outFile << "(" << now << ") Core:" << coreID << " - Hello world from " << currentProcess.getName() << "!\n";
            outFile.close();

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
        coresUsed--;
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
