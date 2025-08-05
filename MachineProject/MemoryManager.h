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
#include <queue>
#include <unordered_map>

extern std::vector<uint8_t> simulatedMemory;

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

    // Demand paging structures
    struct PageInfo {
        int frameIndex;      // -1 if not in memory
        bool inMemory;
        bool dirty;
    };
    // processName -> (pageNumber -> PageInfo)
    std::unordered_map<std::string, std::unordered_map<int, PageInfo>> pageTable;
    // Frame table: frameIndex -> (processName, pageNumber)
    std::vector<std::pair<std::string, int>> frameTable;
    // FIFO queue for page replacement: stores (processName, pageNumber)
    std::queue<std::pair<std::string, int>> fifoQueue;
    // Number of frames in memory
    int numFrames;

    // Backing store filename
    std::string backingStoreFilename = "csopesy-backing-store.txt";

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

    // Demand paging/page replacement/backing store methods
    // Simulate accessing a page (triggers page fault if not present)
    void accessPage(const std::string& processName, int pageNumber, bool isWrite = false);
    // Handle a page fault
    void handlePageFault(const std::string& processName, int pageNumber, bool isWrite);
    // Select a victim page using FIFO
    std::pair<std::string, int> selectVictimPage();
    // Evict a page from memory
    void evictPage(const std::string& processName, int pageNumber);
    // Write a page to the backing store
    void writePageToBackingStore(const std::string& processName, int pageNumber);
    // Read a page from the backing store
    void readPageFromBackingStore(const std::string& processName, int pageNumber);
    // Utility: get number of frames
    int getNumFrames() const { return numFrames; }

    // Simulate reading a uint16 value from a process's memory (returns false if access violation)
    bool readMemory(const std::string& processName, int processMemSize, uint32_t address, uint16_t& outValue);
    // Simulate writing a uint16 value to a process's memory (returns false if access violation)
    bool writeMemory(const std::string& processName, int processMemSize, uint32_t address, uint16_t value);


private:
    void mergeFreeBlocks();
    std::string getCurrentTimestamp() const;
    void writeMemorySnapshotToFile(const std::string& filename, int quantumCycle);
};

#endif
