#include "Scheduler.h"
#include <chrono>

Scheduler::Scheduler(int coreCount_, SchedulingAlgorithm algo, int quantum)
    : coreCount(coreCount_), algorithm(algo), quantumCycles(quantum) {
}

Scheduler::~Scheduler() {
    stopFlag = true;
    started = false;
    cv.notify_all();
    for (auto& t : workerThreads)
        if (t.joinable()) t.join();
}

void Scheduler::start() {
    if (!started) {
        started = true;
        stopFlag = false;
        for (int i = 0; i < coreCount; ++i)
            workerThreads.emplace_back(&Scheduler::worker, this, i);
    }
}

void Scheduler::addProcess(Process* p) {
    std::lock_guard<std::mutex> lock(queueMutex);
    processQueue.push_back(p);
    {
        std::lock_guard<std::mutex> infoLock(processMutex);
        processList.push_back({
            static_cast<int>(processList.size()),
            p->name,
            "",          // startTime left blank for now
            -1,          // coreID (not yet assigned)
            0,           // progress
            p->totalCommands,
            false        // finished
            });
    }
    cv.notify_one();
}

void Scheduler::worker(int coreId) {
    while (!stopFlag) {
        Process* p = nullptr;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            cv.wait(lock, [&]() { return !processQueue.empty() || stopFlag; });
            if (stopFlag || processQueue.empty()) continue;
            p = processQueue.front();
            processQueue.pop_front();
        }

        if (!p) continue;

        if (algorithm == SchedulingAlgorithm::FCFS) {
            while (p->executedCommands < p->totalCommands) {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                int dummyTick = 0;
                bool running = p->executeNextInstruction(coreId, dummyTick);

                {
                    std::lock_guard<std::mutex> lock(processMutex);
                    for (auto& info : processList) {
                        if (info.name == p->name) {
                            info.coreID = coreId;
                            info.progress = p->executedCommands;
                            if (!running) info.finished = true;
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
                running = p->executeNextInstruction(coreId, dummyTick);
                ++cycles;

                {
                    std::lock_guard<std::mutex> lock(processMutex);
                    for (auto& info : processList) {
                        if (info.name == p->name) {
                            info.coreID = coreId;
                            info.progress = p->executedCommands;
                            if (!running) info.finished = true;
                            break;
                        }
                    }
                }
            }

            if (p->executedCommands < p->totalCommands && running) {
                std::lock_guard<std::mutex> lock(queueMutex);
                processQueue.push_back(p);
                cv.notify_one();
                continue;
            }
        }

        delete p;
    }
}

std::vector<ProcessInfo> Scheduler::getProcessList() {
    std::lock_guard<std::mutex> lock(processMutex);
    return processList;
}

ProcessInfo* Scheduler::getProcessInfoByName(const std::string& name) {
    std::lock_guard<std::mutex> lock(processMutex);
    for (auto& info : processList) {
        if (info.name == name) return &info;
    }
    return nullptr;
}

void Scheduler::stop() {
    stopFlag = true;
    cv.notify_all();
    for (auto& t : workerThreads) {
        if (t.joinable()) t.join();
    }
    workerThreads.clear();
    started = false;  // <--- Add this line to allow restart
}
