#ifndef MEMORYMANAGER_H
#define MEMORYMANAGER_H

#include <vector>
#include <map>
#include <queue>
#include <mutex>
#include <fstream>
#include <string>
#include <cstdint>
#include <memory>

// Memory frame/page structure
struct MemoryFrame {
    uint16_t data[256]; // 512 bytes per frame (256 * 2 bytes)
    bool isOccupied;
    int processId;
    int pageNumber;
    bool isDirty;
    
    MemoryFrame() : isOccupied(false), processId(-1), pageNumber(-1), isDirty(false) {
        for (int i = 0; i < 256; i++) {
            data[i] = 0;
        }
    }
};

// Process memory information
struct ProcessMemory {
    int processId;
    std::string processName;
    int totalMemory;
    int allocatedMemory;
    std::vector<int> pageTable; // Maps virtual pages to physical frames (-1 if not in memory)
    std::map<std::string, uint16_t> symbolTable; // Variables stored in 64-byte segment
    bool isActive;
    
    // Default constructor for std::map compatibility
    ProcessMemory() : processId(-1), processName(""), totalMemory(0), 
                      allocatedMemory(0), isActive(false) {}
    
    ProcessMemory(int id, const std::string& name, int memory) 
        : processId(id), processName(name), totalMemory(memory), 
          allocatedMemory(0), isActive(false) {}
};

class MemoryManager {
private:
    std::vector<MemoryFrame> physicalMemory;
    std::map<int, ProcessMemory> processes;
    std::queue<int> freeFrames;
    std::mutex memoryMutex;
    std::ofstream backingStore;
    
    // Configuration
    int maxOverallMemory;
    int memoryPerFrame;
    int minMemoryPerProcess;
    int maxMemoryPerProcess;
    int totalFrames;
    
    // Statistics
    int pagesPagedIn;
    int pagesPagedOut;
    int totalMemoryUsed;
    
    // Backing store operations
    void saveToBackingStore(int processId, int pageNumber);
    void loadFromBackingStore(int processId, int pageNumber, int frameNumber);
    int findVictimFrame();
    void updateBackingStoreFile();

public:
    MemoryManager(int maxMem, int frameSize, int minProcMem, int maxProcMem);
    ~MemoryManager();
    
    // Process memory management
    bool allocateProcessMemory(int processId, const std::string& processName, int memorySize);
    void deallocateProcessMemory(int processId);
    bool isValidMemorySize(int size);
    
    // Memory access operations
    uint16_t readMemory(int processId, uint32_t address);
    bool writeMemory(int processId, uint32_t address, uint16_t value);
    bool handlePageFault(int processId, int pageNumber);
    
    // Variable operations (symbol table)
    bool setVariable(int processId, const std::string& varName, uint16_t value);
    uint16_t getVariable(int processId, const std::string& varName);
    
    // Memory visualization
    void printProcessSmi();
    void printVmstat();
    
    // Getters for statistics
    int getTotalMemory() const { return maxOverallMemory; }
    int getUsedMemory() const { return totalMemoryUsed; }
    int getFreeMemory() const { return maxOverallMemory - totalMemoryUsed; }
    int getPagesPagedIn() const { return pagesPagedIn; }
    int getPagesPagedOut() const { return pagesPagedOut; }
    int getTotalFrames() const { return totalFrames; }
    
    // Process information
    bool processExists(int processId) const;
    const ProcessMemory* getProcessMemory(int processId) const;
    std::vector<ProcessMemory> getAllProcesses() const;
};

#endif 