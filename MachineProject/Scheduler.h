#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <queue>
#include <deque>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include "Process.h"
#include "MemoryManager.h"

extern std::atomic<int> coresUsed;
extern std::atomic<int> totalCpuCycles;
extern std::atomic<int> activeCpuCycles;
extern std::vector<ProcessInfo> processList;
extern std::mutex processMutex;

// Scheduling algorithms
enum class SchedulingAlgorithm {
    FCFS,
    RR
};

class Scheduler {
private:
    std::deque<Process*> processQueue; // deque for RR pop/push
    std::deque<Process*> waitingQueue; // processes waiting for memory
    mutable std::mutex queueMutex;
    std::condition_variable cv;
    std::atomic<bool> stopFlag = false;
    std::atomic<bool> started = false;

    int coreCount;
    int quantumCycles; // For RR
    SchedulingAlgorithm algorithm;
    std::vector<std::thread> workerThreads;
    MemoryManager* memoryManager;
    int memoryPerProcess;
    std::atomic<int> currentQuantumCycle;


public:
    Scheduler(int coreCount_, SchedulingAlgorithm algo, MemoryManager* memMgr, int memPerProc, int quantum = 1)
        : coreCount(coreCount_), algorithm(algo), quantumCycles(quantum), memoryManager(memMgr), memoryPerProcess(memPerProc), currentQuantumCycle(0) {
    }

    void start() {
        if (!started) {
            started = true;
            stopFlag = false;
            for (int i = 0; i < coreCount; ++i) {
                workerThreads.emplace_back(&Scheduler::worker, this, i);
            }
        }
    }

    ~Scheduler() {
        stopFlag = true;
        started = false;
        cv.notify_all();
        for (auto& t : workerThreads)
            if (t.joinable())
                t.join();
    }

    void addProcess(Process* p) {
        std::lock_guard<std::mutex> lock(queueMutex);

        // Try to allocate memory for the process
        if (memoryManager && memoryManager->canAllocateMemory(memoryPerProcess)) {
            if (memoryManager->allocateMemory(p->name, memoryPerProcess)) {
                processQueue.push_back(p);
                cv.notify_one();
            }
            else {
                // Memory allocation failed, add to waiting queue
                waitingQueue.push_back(p);
            }
        }
        else {
            // Not enough memory, add to waiting queue
            waitingQueue.push_back(p);
        }
    }

    void worker(int coreId) {
        while (!stopFlag) {
            Process* p = nullptr;
            {
                std::unique_lock<std::mutex> lock(queueMutex);
                cv.wait(lock, [&]() { return !processQueue.empty() || stopFlag; });
                if (stopFlag) break;

                if (processQueue.empty()) {
                    totalCpuCycles++;
                    continue;
                }
                // Pop process based on scheduling algorithm
                if (algorithm == SchedulingAlgorithm::FCFS) {
                    p = processQueue.front();
                    processQueue.pop_front();
                }
                else if (algorithm == SchedulingAlgorithm::RR) {
                    p = processQueue.front();
                    processQueue.pop_front();
                }
            }
            if (!p) continue;

            coresUsed++;

            try {
                if (algorithm == SchedulingAlgorithm::FCFS) {
                    while (p->executedCommands < p->totalCommands) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(20));
                        int dummyTick = 0;
                        bool running = p->executeNextInstruction(coreId);
                        totalCpuCycles++;
                        activeCpuCycles++;

                        // Generate memory snapshot every quantum cycle
                        int newQuantumCycle = (totalCpuCycles.load() / quantumCycles) + 1;
                        if (newQuantumCycle != currentQuantumCycle.load()) {
                            currentQuantumCycle.store(newQuantumCycle);
                            generateMemorySnapshot(newQuantumCycle);
                        }

                        {
                            std::lock_guard<std::mutex> lock(processMutex);
                            for (auto& info : processList) {
                                if (info.name == p->name) {
                                    info.coreID = coreId;
                                    info.progress = p->executedCommands;
                                    if (!running) {
                                        info.finished = true;
                                    }
                                    break;
                                }
                            }
                        }
                        if (!running) break;
                    }
                }
                else if (algorithm == SchedulingAlgorithm::RR) {
                    int cycles = 0;
                    bool running = true;
                    while (p->executedCommands < p->totalCommands && cycles < quantumCycles && running) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(20));
                        int dummyTick = 0;
                        running = p->executeNextInstruction(coreId);
                        ++cycles;
                        totalCpuCycles++;
                        activeCpuCycles++;

                        // Generate memory snapshot every quantum cycle
                        int newQuantumCycle = (totalCpuCycles.load() / quantumCycles) + 1;
                        if (newQuantumCycle != currentQuantumCycle.load()) {
                            currentQuantumCycle.store(newQuantumCycle);
                            generateMemorySnapshot(newQuantumCycle);
                        }

                        {
                            std::lock_guard<std::mutex> lock(processMutex);
                            for (auto& info : processList) {
                                if (info.name == p->name) {
                                    info.coreID = coreId;
                                    info.progress = p->executedCommands;
                                    if (!running) {
                                        info.finished = true;
                                    }
                                    break;
                                }
                            }
                        }
                    }
                    // If not finished, requeue
                    if (p->executedCommands < p->totalCommands && running) {
                        std::lock_guard<std::mutex> lock(queueMutex);
                        processQueue.push_back(p);
                        cv.notify_one();
                        coresUsed--;
                        continue;
                    }
                }
            }
            catch (const std::exception& e) {
                // Handle process execution errors
                std::lock_guard<std::mutex> lock(processMutex);
                for (auto& info : processList) {
                    if (info.name == p->name) {
                        info.finished = true;
                        break;
                    }
                }
            }

            coresUsed--;

            // Deallocate memory when process finishes
            if (memoryManager) {
                memoryManager->deallocateMemory(p->name);

                // Try to move waiting processes to ready queue
                while (!waitingQueue.empty()) {
                    Process* waitingProcess = waitingQueue.front();
                    if (memoryManager->canAllocateMemory(memoryPerProcess) &&
                        memoryManager->allocateMemory(waitingProcess->name, memoryPerProcess)) {
                        waitingQueue.pop_front();
                        processQueue.push_back(waitingProcess);
                        cv.notify_one();
                    }
                    else {
                        break; // No more memory available
                    }
                }
            }

            delete p;
        }
    }

    std::vector<ProcessInfo> getProcessList() {
        std::lock_guard<std::mutex> lock(processMutex);
        return processList;
    }

    void generateMemorySnapshot(int quantumCycle) {
        if (memoryManager) {
            memoryManager->generateMemorySnapshot(quantumCycle);
        }
    }

    int getWaitingQueueSize() const {
        std::lock_guard<std::mutex> lock(queueMutex);
        return waitingQueue.size();
    }
};

#endif
