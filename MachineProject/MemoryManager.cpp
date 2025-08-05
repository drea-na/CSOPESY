#include "MemoryManager.h"
#include <iostream>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cstring>

MemoryManager::MemoryManager(int totalMem, int frameSz, int maxMemPerProc)
    : totalMemory(totalMem), frameSize(frameSz), maxMemoryPerProcess(maxMemPerProc), currentQuantumCycle(0) {
    // Initialize with one free block covering the entire memory (legacy)
    memoryBlocks.push_back(MemoryBlock(0, totalMemory, "", false));
    // Initialize frame table
    int numFrames = totalMemory / frameSize;
    for (int i = 0; i < numFrames; ++i) {
        frameTable.emplace_back(i);
    }
    // Open/create backing store file
    backingStoreFile.open(backingStoreFileName, std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc);
    if (!backingStoreFile.is_open()) {
        // Try to create if not exists
        backingStoreFile.open(backingStoreFileName, std::ios::out | std::ios::binary);
        backingStoreFile.close();
        backingStoreFile.open(backingStoreFileName, std::ios::in | std::ios::out | std::ios::binary);
    }
    nextBackingStoreOffset = 0;
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

// Demand paging: memory access
bool MemoryManager::accessMemory(const std::string& processName, int virtualAddress, bool isWrite, uint16_t* value) {
    std::lock_guard<std::mutex> lock(memoryMutex);
    int pageNumber = virtualAddress / frameSize;
    int offset = virtualAddress % frameSize;
    auto& pageTable = processPageTables[processName];
    if (pageNumber >= (int)pageTable.size()) return false; // Out of bounds
    if (!pageTable[pageNumber].valid) {
        pageFaultHandler(processName, pageNumber);
    }
    int frameNum = pageTable[pageNumber].frameNumber;
    if (frameNum < 0 || frameNum >= (int)frameTable.size()) return false;
    // Simulate physical memory as a static array (for demo)
    static std::vector<uint8_t> physicalMemory(totalMemory, 0);
    int physAddr = frameNum * frameSize + offset;
    if (isWrite) {
        if (value) {
            // Write 2 bytes (uint16_t)
            if (physAddr + 1 >= (int)physicalMemory.size()) return false;
            uint16_t v = *value;
            physicalMemory[physAddr] = v & 0xFF;
            physicalMemory[physAddr + 1] = (v >> 8) & 0xFF;
        }
    } else {
        if (value) {
            if (physAddr + 1 >= (int)physicalMemory.size()) return false;
            *value = physicalMemory[physAddr] | (physicalMemory[physAddr + 1] << 8);
        }
    }
    return true;
}

void MemoryManager::pageFaultHandler(const std::string& processName, int pageNumber) {
    // Find a free frame
    int freeFrame = -1;
    for (auto& frame : frameTable) {
        if (frame.isFree) {
            freeFrame = frame.frameNumber;
            break;
        }
    }
    if (freeFrame == -1) {
        // No free frame, need to evict
        swapOutPage();
        // Try again
        for (auto& frame : frameTable) {
            if (frame.isFree) {
                freeFrame = frame.frameNumber;
                break;
            }
        }
    }
    // Load page into free frame
    frameTable[freeFrame].isFree = false;
    frameTable[freeFrame].processName = processName;
    frameTable[freeFrame].pageNumber = pageNumber;
    processPageTables[processName][pageNumber].valid = true;
    processPageTables[processName][pageNumber].frameNumber = freeFrame;
    loadedPages.push({processName, pageNumber});
    
    // Increment pages paged in counter
    pagesPagedIn++;
    
    // If page was in backing store, load it
    if (processPageTables[processName][pageNumber].backingStoreOffset != -1) {
        loadPageFromBackingStore(processName, pageNumber);
    }
}

void MemoryManager::swapOutPage() {
    // FIFO: evict the oldest loaded page
    if (loadedPages.empty()) return;
    auto victim = loadedPages.front(); loadedPages.pop();
    const std::string& proc = victim.first;
    int pageNum = victim.second;
    int frameNum = processPageTables[proc][pageNum].frameNumber;
    // Write to backing store
    writePageToBackingStore(proc, pageNum);
    
    // Increment pages paged out counter
    pagesPagedOut++;
    
    // Mark frame as free
    frameTable[frameNum].isFree = true;
    frameTable[frameNum].processName = "";
    frameTable[frameNum].pageNumber = -1;
    // Update page table
    processPageTables[proc][pageNum].valid = false;
    processPageTables[proc][pageNum].frameNumber = -1;
}

void MemoryManager::writePageToBackingStore(const std::string& processName, int pageNumber) {
    // Simulate physical memory as a static array
    static std::vector<uint8_t> physicalMemory(totalMemory, 0);
    int frameNum = processPageTables[processName][pageNumber].frameNumber;
    int physAddr = frameNum * frameSize;
    // Write frameSize bytes to backing store
    backingStoreFile.seekp(nextBackingStoreOffset, std::ios::beg);
    backingStoreFile.write(reinterpret_cast<char*>(&physicalMemory[physAddr]), frameSize);
    backingStoreFile.flush();
    processPageTables[processName][pageNumber].backingStoreOffset = nextBackingStoreOffset;
    nextBackingStoreOffset += frameSize;
}

void MemoryManager::loadPageFromBackingStore(const std::string& processName, int pageNumber) {
    // Simulate physical memory as a static array
    static std::vector<uint8_t> physicalMemory(totalMemory, 0);
    int frameNum = processPageTables[processName][pageNumber].frameNumber;
    int physAddr = frameNum * frameSize;
    int offset = processPageTables[processName][pageNumber].backingStoreOffset;
    if (offset == -1) return; // Nothing to load
    backingStoreFile.seekg(offset, std::ios::beg);
    backingStoreFile.read(reinterpret_cast<char*>(&physicalMemory[physAddr]), frameSize);
    // After loading, clear backing store offset
    processPageTables[processName][pageNumber].backingStoreOffset = -1;
}

void MemoryManager::removeProcessPages(const std::string& processName) {
    // Free all frames and page table entries for this process
    auto it = processPageTables.find(processName);
    if (it != processPageTables.end()) {
        for (size_t i = 0; i < it->second.size(); ++i) {
            if (it->second[i].valid) {
                int frameNum = it->second[i].frameNumber;
                if (frameNum >= 0 && frameNum < (int)frameTable.size()) {
                    frameTable[frameNum].isFree = true;
                    frameTable[frameNum].processName = "";
                    frameTable[frameNum].pageNumber = -1;
                }
            }
        }
        processPageTables.erase(it);
    }
    // Remove from loadedPages queue
    std::queue<std::pair<std::string, int>> newQueue;
    while (!loadedPages.empty()) {
        auto entry = loadedPages.front(); loadedPages.pop();
        if (entry.first != processName) newQueue.push(entry);
    }
    loadedPages = std::move(newQueue);
}
