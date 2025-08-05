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
#include <map>
#include <queue>
#include <atomic>

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
public:
    struct PageTableEntry {
        bool valid; // in memory?
        int frameNumber; // if valid
        int backingStoreOffset; // if swapped out
        PageTableEntry() : valid(false), frameNumber(-1), backingStoreOffset(-1) {}
    };

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

    // Demand paging methods
    bool accessMemory(const std::string& processName, int virtualAddress, bool isWrite, uint16_t* value = nullptr);
    void pageFaultHandler(const std::string& processName, int pageNumber);
    void swapOutPage();
    void loadPageFromBackingStore(const std::string& processName, int pageNumber);
    void writePageToBackingStore(const std::string& processName, int pageNumber);
    void removeProcessPages(const std::string& processName);
    
    // Page fault statistics getters
    int getPagesPagedIn() const { return pagesPagedIn.load(); }
    int getPagesPagedOut() const { return pagesPagedOut.load(); }

    // Public access to process page tables
    std::map<std::string, std::vector<PageTableEntry>>& getProcessPageTables() { return processPageTables; }

private:
    std::vector<MemoryBlock> memoryBlocks;
    int totalMemory;
    int frameSize;
    int maxMemoryPerProcess;
    mutable std::mutex memoryMutex;
    int currentQuantumCycle;

    // Demand paging structures
    struct FrameEntry {
        int frameNumber;
        std::string processName;
        int pageNumber;
        bool isFree;
        FrameEntry(int num) : frameNumber(num), processName(""), pageNumber(-1), isFree(true) {}
    };
    std::vector<FrameEntry> frameTable; // All physical frames

    std::map<std::string, std::vector<PageTableEntry>> processPageTables; // processName -> page table

    std::queue<std::pair<std::string, int>> loadedPages; // FIFO: (processName, pageNumber)

    std::fstream backingStoreFile; // csopesy-backing-store.txt
    std::string backingStoreFileName = "csopesy-backing-store.txt";
    int nextBackingStoreOffset = 0;
    
    // Page fault statistics
    std::atomic<int> pagesPagedIn{0};
    std::atomic<int> pagesPagedOut{0};

private:
    void mergeFreeBlocks();
    std::string getCurrentTimestamp() const;
    void writeMemorySnapshotToFile(const std::string& filename, int quantumCycle);
};

#endif
