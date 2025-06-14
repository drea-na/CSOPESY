#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <queue>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include "Process.h"

class Scheduler {
private:
    std::queue<Process*> processQueue;
    std::mutex queueMutex;
    std::condition_variable cv;
    std::atomic<bool> stopFlag = false;

    static const int CORE_COUNT = 4;
    std::vector<std::thread> workerThreads;

public:
    Scheduler() {
        for (int i = 0; i < CORE_COUNT; ++i) {
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
        processQueue.push(p);
        cv.notify_one();
    }

    void worker(int coreId) {
        while (!stopFlag) {
            Process* p = nullptr;
            {
                std::unique_lock<std::mutex> lock(queueMutex);
                cv.wait(lock, [&]() { return !processQueue.empty() || stopFlag; });

                if (stopFlag || processQueue.empty()) continue;

                p = processQueue.front();
                processQueue.pop();
            }

            while (p->executedCommands < p->totalCommands) {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                p->logPrint(coreId);
                ++p->executedCommands;
            }

            delete p;
        }
    }
};

#endif
