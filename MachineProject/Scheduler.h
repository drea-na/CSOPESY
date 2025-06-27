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
    std::deque<Process*> processQueue; // Use deque for RR pop/push
    std::mutex queueMutex;
    std::condition_variable cv;
    std::atomic<bool> stopFlag = false;
    std::atomic<bool> started = false;

    int coreCount;
    int quantumCycles; // For RR
    SchedulingAlgorithm algorithm;
    std::vector<std::thread> workerThreads;


public:
    Scheduler(int coreCount_, SchedulingAlgorithm algo, int quantum = 1)
        : coreCount(coreCount_), algorithm(algo), quantumCycles(quantum) {
        // Don't start worker threads immediately
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
        processQueue.push_back(p);
        cv.notify_one();
    }

    void worker(int coreId) {
        while (!stopFlag) {
            Process* p = nullptr;
            {
                std::unique_lock<std::mutex> lock(queueMutex);
                cv.wait(lock, [&]() { return !processQueue.empty() || stopFlag; });
                if (stopFlag) break;
                
                if (processQueue.empty()) {
					totalCpuCycles ++;
                    continue; 
                }
				// Pop process based on scheduling algorithm
                if (algorithm == SchedulingAlgorithm::FCFS) {
                    p = processQueue.front();
                    processQueue.pop_front();
                } else if (algorithm == SchedulingAlgorithm::RR) {
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
                } else if (algorithm == SchedulingAlgorithm::RR) {
                    int cycles = 0;
                    bool running = true;
                    while (p->executedCommands < p->totalCommands && cycles < quantumCycles && running) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(20));
                        int dummyTick = 0;
                        running = p->executeNextInstruction(coreId);
                        ++cycles;
                        totalCpuCycles++;
                        activeCpuCycles++;
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
            } catch (const std::exception& e) {
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
            delete p;
        }
    }

    std::vector<ProcessInfo> getProcessList() {
        std::lock_guard<std::mutex> lock(processMutex);
        return processList;
    }
};

#endif
