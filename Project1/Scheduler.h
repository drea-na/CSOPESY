#pragma once
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

enum class SchedulingAlgorithm {
    FCFS,
    RR
};

class Scheduler {
private:
    std::deque<Process*> processQueue;
    std::mutex queueMutex;
    std::condition_variable cv;
    std::atomic<bool> stopFlag = false;
    std::atomic<bool> started = false;

    int coreCount;
    int quantumCycles;
    SchedulingAlgorithm algorithm;
    std::vector<std::thread> workerThreads;

    //Add these two lines:
    std::mutex processMutex;
    std::vector<ProcessInfo> processList;

public:
    Scheduler(int coreCount_, SchedulingAlgorithm algo, int quantum = 1);
    ~Scheduler();
    void stop();  // Graceful shutdown
    void start();
    void addProcess(Process* p);
    void worker(int coreId);
    std::vector<ProcessInfo> getProcessList();
    ProcessInfo* getProcessInfoByName(const std::string& name);
    int getCoreCount() const;
};

#endif