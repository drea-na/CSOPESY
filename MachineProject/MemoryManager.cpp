#include "MemoryManager.h"
#include <iostream>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <vector>
#include <ctime>

std::vector<uint8_t> simulatedMemory(1 << 20, 0); // 1MB simulated memory

MemoryManager::MemoryManager(int totalMem, int frameSz, int maxMemPerProc)
    : totalMemory(totalMem), frameSize(frameSz), maxMemoryPerProcess(maxMemPerProc), currentQuantumCycle(0) {
    // Initialize with one free block covering the entire memory
    memoryBlocks.push_back(MemoryBlock(0, totalMemory, "", false));
    // Demand paging: initialize frame table
    numFrames = totalMemory / frameSize;
    frameTable.resize(numFrames, std::make_pair("", -1));
}

bool MemoryManager::allocateMemory(const std::string& processName, int requiredSize) {
    std::lock_guard<std::mutex> lock(memoryMutex);

    // First-fit algorithm: find the first free block that can accommodate the request
    for (size_t i = 0; i < memoryBlocks.size(); ++i) {
        if (!memoryBlocks[i].isAllocated && memoryBlocks[i].size >= requiredSize) {
            int remainingSize = memoryBlocks[i].size - requiredSize;

            if (remainingSize > 0) {
                memoryBlocks[i].size = requiredSize;
                memoryBlocks[i].processName = processName;
                memoryBlocks[i].isAllocated = true;

                memoryBlocks.insert(memoryBlocks.begin() + i + 1,
                    MemoryBlock(memoryBlocks[i].startAddress + requiredSize, remainingSize, "", false));
            }
            else {
                memoryBlocks[i].processName = processName;
                memoryBlocks[i].isAllocated = true;
            }

            return true;
        }
    }

    return false; 
}

void MemoryManager::deallocateMemory(const std::string& processName) {
    std::lock_guard<std::mutex> lock(memoryMutex);

    for (auto& block : memoryBlocks) {
        if (block.isAllocated && block.processName == processName) {
            block.isAllocated = false;
            block.processName = "";
            break;
        }
    }
    mergeFreeBlocks();
}

bool MemoryManager::canAllocateMemory(int requiredSize) const {
    std::lock_guard<std::mutex> lock(memoryMutex);

    for (const auto& block : memoryBlocks) {
        if (!block.isAllocated && block.size >= requiredSize) {
            return true;
        }
    }
    return false;
}

int MemoryManager::getNumberOfProcessesInMemory() const {
    std::lock_guard<std::mutex> lock(memoryMutex);

    int count = 0;
    for (const auto& block : memoryBlocks) {
        if (block.isAllocated) {
            count++;
        }
    }
    return count;
}

int MemoryManager::getTotalExternalFragmentation() const {
    std::lock_guard<std::mutex> lock(memoryMutex);

    int totalFragmentation = 0;
    for (const auto& block : memoryBlocks) {
        if (!block.isAllocated) {
            totalFragmentation += block.size;
        }
    }
    return totalFragmentation;
}

std::vector<MemoryBlock> MemoryManager::getMemoryLayout() const {
    std::lock_guard<std::mutex> lock(memoryMutex);
    return memoryBlocks;
}

void MemoryManager::mergeFreeBlocks() {
    for (size_t i = 0; i < memoryBlocks.size() - 1; ++i) {
        if (!memoryBlocks[i].isAllocated && !memoryBlocks[i + 1].isAllocated) {
            memoryBlocks[i].size += memoryBlocks[i + 1].size;
            memoryBlocks.erase(memoryBlocks.begin() + i + 1);
            --i; 
        }
    }
}

std::string MemoryManager::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm_time;
    localtime_s(&tm_time, &time);
    std::stringstream ss;
    ss << std::put_time(&tm_time, "%m/%d/%Y %I:%M:%S%p");
    return ss.str();
}

void MemoryManager::generateMemorySnapshot(int quantumCycle) {
    std::string filename = "memory_stamp_" + std::to_string(quantumCycle) + ".txt";
    writeMemorySnapshotToFile(filename, quantumCycle);
}

void MemoryManager::writeMemorySnapshotToFile(const std::string& filename, int quantumCycle) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filename << " for writing." << std::endl;
        return;
    }

    std::lock_guard<std::mutex> lock(memoryMutex);

    file << "Timestamp: " << getCurrentTimestamp() << std::endl;

    int processCount = 0;
    for (const auto& block : memoryBlocks) {
        if (block.isAllocated) {
            processCount++;
        }
    }
    file << "Number of processes in memory: " << processCount << std::endl;

    int fragmentationBytes = 0;
    for (const auto& block : memoryBlocks) {
        if (!block.isAllocated) {
            fragmentationBytes += block.size;
        }
    }
    double fragmentationKB = fragmentationBytes; // / 1024.0
    file << "Total external fragmentation in KB: " << std::fixed << std::setprecision(2) << fragmentationKB << std::endl;

    file << std::endl;
    file << "----end---- = " << totalMemory << std::endl;
    file << std::endl;

    // Print memory blocks in reverse order (highest address first)
    std::vector<MemoryBlock> sortedBlocks = memoryBlocks;
    std::sort(sortedBlocks.begin(), sortedBlocks.end(),
        [](const MemoryBlock& a, const MemoryBlock& b) {
            return a.startAddress > b.startAddress;
        });

    for (const auto& block : sortedBlocks) {
        if (block.isAllocated) {
            file << block.startAddress + block.size - 1 << std::endl;
            file << block.processName << std::endl;
            file << block.startAddress << std::endl;
            file << std::endl;
        }
    }

    file << "----start---- = 0" << std::endl;
    file << std::endl;

    file.close();
}

// Simulate accessing a page (triggers page fault if not present)
void MemoryManager::accessPage(const std::string& processName, int pageNumber, bool isWrite) {
    std::lock_guard<std::mutex> lock(memoryMutex);
    auto& procPages = pageTable[processName];
    if (procPages.find(pageNumber) == procPages.end() || !procPages[pageNumber].inMemory) {
        // Page fault
        handlePageFault(processName, pageNumber, isWrite);
    } else {
        // Page is in memory, mark dirty if write
        if (isWrite) procPages[pageNumber].dirty = true;
    }
}

void MemoryManager::handlePageFault(const std::string& processName, int pageNumber, bool isWrite) {
    // Find a free frame
    int freeFrame = -1;
    for (int i = 0; i < numFrames; ++i) {
        if (frameTable[i].first.empty()) {
            freeFrame = i;
            break;
        }
    }
    if (freeFrame == -1) {
        // No free frame, need to evict
        auto victim = selectVictimPage();
        evictPage(victim.first, victim.second);
        // After eviction, find the now-free frame
        for (int i = 0; i < numFrames; ++i) {
            if (frameTable[i].first.empty()) {
                freeFrame = i;
                break;
            }
        }
    }
    // Load the page into the free frame
    frameTable[freeFrame] = {processName, pageNumber};
    pageTable[processName][pageNumber] = {freeFrame, true, isWrite};
    // If page was in backing store, read it
    readPageFromBackingStore(processName, pageNumber);
    // Add to FIFO queue
    fifoQueue.push({processName, pageNumber});
}

std::pair<std::string, int> MemoryManager::selectVictimPage() {
    // FIFO: evict the oldest page
    if (fifoQueue.empty()) return {"", -1};
    auto victim = fifoQueue.front();
    fifoQueue.pop();
    return victim;
}

void MemoryManager::evictPage(const std::string& processName, int pageNumber) {
    auto& pageInfo = pageTable[processName][pageNumber];
    int frameIdx = pageInfo.frameIndex;
    // Write to backing store if dirty
    if (pageInfo.dirty) {
        writePageToBackingStore(processName, pageNumber);
    }
    // Mark frame as free
    frameTable[frameIdx] = {"", -1};
    pageInfo.inMemory = false;
    pageInfo.frameIndex = -1;
    pageInfo.dirty = false;
}

void MemoryManager::writePageToBackingStore(const std::string& processName, int pageNumber) {
    std::ofstream backingStore(backingStoreFilename, std::ios::app);
    if (backingStore.is_open()) {
        backingStore << processName << " " << pageNumber << "\n";
        // Simulate writing page data (could add more info here)
        backingStore.close();
    }
}

void MemoryManager::readPageFromBackingStore(const std::string& processName, int pageNumber) {
    // Simulate reading: remove entry from file if present
    std::ifstream inFile(backingStoreFilename);
    std::ofstream outFile("temp_bstore.txt");
    std::string line;
    while (std::getline(inFile, line)) {
        std::istringstream iss(line);
        std::string pname; int pnum;
        if (iss >> pname >> pnum) {
            if (!(pname == processName && pnum == pageNumber)) {
                outFile << line << "\n";
            }
        }
    }
    inFile.close();
    outFile.close();
    std::remove(backingStoreFilename.c_str());
    std::rename("temp_bstore.txt", backingStoreFilename.c_str());
}

// Simulated physical memory (shared by all processes)

bool MemoryManager::readMemory(const std::string& processName, int processMemSize, uint32_t address, uint16_t& outValue) {
    std::lock_guard<std::mutex> lock(memoryMutex);
    // Bounds check: address must be within process's allocated memory
    if (address + 1 >= (uint32_t)processMemSize) return false;
    // Demand paging: ensure both bytes are paged in
    int pageSize = frameSize;
    int pageNum1 = address / pageSize;
    int pageNum2 = (address + 1) / pageSize;
    accessPage(processName, pageNum1, false);
    if (pageNum2 != pageNum1) accessPage(processName, pageNum2, false);
    // Simulate memory as a global array, offset by processName hash
    size_t base = std::hash<std::string>{}(processName) % (simulatedMemory.size() - processMemSize + 1);
    outValue = simulatedMemory[base + address] | (simulatedMemory[base + address + 1] << 8);
    return true;
}

bool MemoryManager::writeMemory(const std::string& processName, int processMemSize, uint32_t address, uint16_t value) {
    std::lock_guard<std::mutex> lock(memoryMutex);
    if (address + 1 >= (uint32_t)processMemSize) return false;
    int pageSize = frameSize;
    int pageNum1 = address / pageSize;
    int pageNum2 = (address + 1) / pageSize;
    accessPage(processName, pageNum1, true);
    if (pageNum2 != pageNum1) accessPage(processName, pageNum2, true);
    size_t base = std::hash<std::string>{}(processName) % (simulatedMemory.size() - processMemSize + 1);
    simulatedMemory[base + address] = value & 0xFF;
    simulatedMemory[base + address + 1] = (value >> 8) & 0xFF;
    return true;
}
