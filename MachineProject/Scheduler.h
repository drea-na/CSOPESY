#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <queue>
#include <deque>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <string>
#include "Process.h"

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

    int coreCount;
    int quantumCycles; // For RR
    SchedulingAlgorithm algorithm;
    std::vector<std::thread> workerThreads;

public:
    Scheduler(int coreCount_, SchedulingAlgorithm algo, int quantum = 1)
        : coreCount(coreCount_), algorithm(algo), quantumCycles(quantum) {
        for (int i = 0; i < coreCount; ++i) {
            workerThreads.emplace_back(&Scheduler::worker, this, i);
        }
    }

    ~Scheduler() {
        stopFlag = true;
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
                if (stopFlag || processQueue.empty()) continue;
                if (algorithm == SchedulingAlgorithm::FCFS) {
                    p = processQueue.front();
                    processQueue.pop_front();
                } else if (algorithm == SchedulingAlgorithm::RR) {
                    p = processQueue.front();
                    processQueue.pop_front();
                }
            }
            if (!p) continue;
            if (algorithm == SchedulingAlgorithm::FCFS) {
                while (p->executedCommands < p->totalCommands) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                    p->logPrint(coreId);
                    ++p->executedCommands;
                }
            } else if (algorithm == SchedulingAlgorithm::RR) {
                int cycles = 0;
                while (p->executedCommands < p->totalCommands && cycles < quantumCycles) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                    p->logPrint(coreId);
                    ++p->executedCommands;
                    ++cycles;
                }
                // If not finished, requeue
                if (p->executedCommands < p->totalCommands) {
                    std::lock_guard<std::mutex> lock(queueMutex);
                    processQueue.push_back(p);
                    cv.notify_one();
                    continue;
                }
            }
            delete p;
        }
    }
};

#endif
