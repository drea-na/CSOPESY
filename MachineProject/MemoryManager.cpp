#include "MemoryManager.h"
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <sstream>
#include <iomanip>

MemoryManager::MemoryManager(int totalMem, int frameSz, int maxMemPerProc)
    : totalMemory(totalMem), frameSize(frameSz), maxMemoryPerProcess(maxMemPerProc), currentQuantumCycle(0) {
    // Initialize with one free block covering the entire memory
    memoryBlocks.push_back(MemoryBlock(0, totalMemory, "", false));
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
