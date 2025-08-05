#ifndef MEMORYMANAGER_H
#define MEMORYMANAGER_H

#include <vector>
#include <string>
#include <memory>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <mutex>

struct MemoryBlock {
    int startAddress;
    int size;
    std::string processName;
    bool isAllocated;

    MemoryBlock(int start, int sz, const std::string& proc = "", bool allocated = false)
        : startAddress(start), size(sz), processName(proc), isAllocated(allocated) {
    }
};

class MemoryManager {
private:
    std::vector<MemoryBlock> memoryBlocks;
    int totalMemory;
    int frameSize;
    int maxMemoryPerProcess;
    mutable std::mutex memoryMutex;
    int currentQuantumCycle;

public:
    MemoryManager(int totalMem = 16384, int frameSz = 16, int maxMemPerProc = 4096);

    // Memory allocation methods
    bool allocateMemory(const std::string& processName, int requiredSize);
    void deallocateMemory(const std::string& processName);
    bool canAllocateMemory(int requiredSize) const;

    // Memory information methods
    int getNumberOfProcessesInMemory() const;
    int getTotalExternalFragmentation() const;
    std::vector<MemoryBlock> getMemoryLayout() const;
    // Add to MemoryManager class in MemoryManager.h
    int getFrameSize() const { return frameSize; }

    // File output methods
    void generateMemorySnapshot(int quantumCycle);
    void setCurrentQuantumCycle(int cycle) { currentQuantumCycle = cycle; }


private:
    void mergeFreeBlocks();
    std::string getCurrentTimestamp() const;
    void writeMemorySnapshotToFile(const std::string& filename, int quantumCycle);
};

#endif
