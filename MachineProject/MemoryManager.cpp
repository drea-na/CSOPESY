#include "MemoryManager.h"
#include <iostream>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <cmath>

MemoryManager::MemoryManager(int maxMem, int frameSize, int minProcMem, int maxProcMem)
    : maxOverallMemory(maxMem * 1024), memoryPerFrame(frameSize * 1024), 
      minMemoryPerProcess(minProcMem * 1024), maxMemoryPerProcess(maxProcMem * 1024),
      pagesPagedIn(0), pagesPagedOut(0), totalMemoryUsed(0) {
    
    // Calculate total frames
    totalFrames = maxOverallMemory / memoryPerFrame;
    
    // Initialize physical memory frames
    physicalMemory.resize(totalFrames);
    
    // Initialize free frame queue
    for (int i = 0; i < totalFrames; i++) {
        freeFrames.push(i);
    }
    
    // Open backing store file
    backingStore.open("csopesy-backing-store.txt", std::ios::out | std::ios::trunc);
    if (!backingStore.is_open()) {
        std::cerr << "Warning: Could not open backing store file" << std::endl;
    }
}

MemoryManager::~MemoryManager() {
    if (backingStore.is_open()) {
        backingStore.close();
    }
}

bool MemoryManager::isValidMemorySize(int size) {
    // Check if size is power of 2 and within range [2^6, 2^16]
    if (size < 64 || size > 65536) return false;
    return (size & (size - 1)) == 0; // Power of 2 check
}

bool MemoryManager::allocateProcessMemory(int processId, const std::string& processName, int memorySize) {
    std::lock_guard<std::mutex> lock(memoryMutex);
    
    if (!isValidMemorySize(memorySize)) {
        return false;
    }
    
    // Check if process already exists
    if (processes.find(processId) != processes.end()) {
        return false;
    }
    
    // Calculate number of pages needed
    int pagesNeeded = memorySize / memoryPerFrame;
    if (memorySize % memoryPerFrame != 0) pagesNeeded++;
    
    // In demand paging, we don't need to reserve all memory upfront
    // We only need to ensure the process can fit within the total memory limit
    // The actual memory will be allocated on-demand when pages are accessed
    
    // Create process memory entry
    ProcessMemory procMem(processId, processName, memorySize);
    procMem.pageTable.resize(pagesNeeded, -1); // All pages initially not in memory
    
    processes[processId] = procMem;
    // Don't add to totalMemoryUsed until pages are actually loaded into memory
    
    return true;
}

void MemoryManager::deallocateProcessMemory(int processId) {
    std::lock_guard<std::mutex> lock(memoryMutex);
    
    auto it = processes.find(processId);
    if (it == processes.end()) return;
    
    ProcessMemory& procMem = it->second;
    
    // Free all allocated frames and update totalMemoryUsed
    for (int frameIndex : procMem.pageTable) {
        if (frameIndex != -1) {
            physicalMemory[frameIndex].isOccupied = false;
            physicalMemory[frameIndex].processId = -1;
            physicalMemory[frameIndex].pageNumber = -1;
            physicalMemory[frameIndex].isDirty = false;
            freeFrames.push(frameIndex);
            totalMemoryUsed -= memoryPerFrame; // Subtract the frame size
        }
    }
    
    processes.erase(it);
}

uint16_t MemoryManager::readMemory(int processId, uint32_t address) {
    std::lock_guard<std::mutex> lock(memoryMutex);
    
    auto it = processes.find(processId);
    if (it == processes.end()) {
        throw std::runtime_error("Process not found");
    }
    
    ProcessMemory& procMem = it->second;
    
    // Check if address is within process memory bounds
    if (address >= procMem.totalMemory) {
        throw std::runtime_error("Memory access violation: address out of bounds");
    }
    
    // Calculate page number and offset
    int pageNumber = address / memoryPerFrame;
    int offset = (address % memoryPerFrame) / 2; // Divide by 2 since each element is 2 bytes
    
    // Check if page number is within bounds
    if (pageNumber >= procMem.pageTable.size()) {
        throw std::runtime_error("Memory access violation: page number out of bounds");
    }
    
    // Check if page is in memory
    if (procMem.pageTable[pageNumber] == -1) {
        // Page fault - handle it
        if (!handlePageFault(processId, pageNumber)) {
            throw std::runtime_error("Failed to handle page fault");
        }
    }
    
    int frameIndex = procMem.pageTable[pageNumber];
    
    // Check if frame index is valid
    if (frameIndex < 0 || frameIndex >= physicalMemory.size()) {
        throw std::runtime_error("Memory access violation: invalid frame index");
    }
    
    // Check if offset is within bounds
    if (offset < 0 || offset >= 256) { // Assuming 256 uint16_t elements per frame
        throw std::runtime_error("Memory access violation: offset out of bounds");
    }
    
    return physicalMemory[frameIndex].data[offset];
}

bool MemoryManager::writeMemory(int processId, uint32_t address, uint16_t value) {
    std::lock_guard<std::mutex> lock(memoryMutex);
    
    auto it = processes.find(processId);
    if (it == processes.end()) {
        return false;
    }
    
    ProcessMemory& procMem = it->second;
    
    // Check if address is within process memory bounds
    if (address >= procMem.totalMemory) {
        return false;
    }
    
    // Calculate page number and offset
    int pageNumber = address / memoryPerFrame;
    int offset = (address % memoryPerFrame) / 2;
    
    // Check if page number is within bounds
    if (pageNumber >= procMem.pageTable.size()) {
        return false;
    }
    
    // Check if page is in memory
    if (procMem.pageTable[pageNumber] == -1) {
        // Page fault - handle it
        if (!handlePageFault(processId, pageNumber)) {
            return false;
        }
    }
    
    int frameIndex = procMem.pageTable[pageNumber];
    
    // Check if frame index is valid
    if (frameIndex < 0 || frameIndex >= physicalMemory.size()) {
        return false;
    }
    
    // Check if offset is within bounds
    if (offset < 0 || offset >= 256) { // Assuming 256 uint16_t elements per frame
        return false;
    }
    
    physicalMemory[frameIndex].data[offset] = value;
    physicalMemory[frameIndex].isDirty = true;
    
    return true;
}

bool MemoryManager::handlePageFault(int processId, int pageNumber) {
    auto it = processes.find(processId);
    if (it == processes.end()) return false;
    
    ProcessMemory& procMem = it->second;
    
    // Check if page number is within bounds
    if (pageNumber >= procMem.pageTable.size()) {
        return false;
    }
    
    // Find a free frame
    int frameIndex = -1;
    if (!freeFrames.empty()) {
        frameIndex = freeFrames.front();
        freeFrames.pop();
    } else {
        // No free frames, need to evict
        frameIndex = findVictimFrame();
        if (frameIndex == -1) return false;
        
        // Check if frame index is valid
        if (frameIndex < 0 || frameIndex >= physicalMemory.size()) {
            return false;
        }
        
        // Save current frame to backing store if dirty
        if (physicalMemory[frameIndex].isDirty) {
            saveToBackingStore(physicalMemory[frameIndex].processId, 
                              physicalMemory[frameIndex].pageNumber);
            pagesPagedOut++;
        }
        
        // Update page table of evicted process
        auto evictedIt = processes.find(physicalMemory[frameIndex].processId);
        if (evictedIt != processes.end()) {
            int evictedPageNumber = physicalMemory[frameIndex].pageNumber;
            if (evictedPageNumber >= 0 && evictedPageNumber < evictedIt->second.pageTable.size()) {
                evictedIt->second.pageTable[evictedPageNumber] = -1;
            }
            // Update total memory used when a page is evicted
            totalMemoryUsed -= memoryPerFrame;
        }
    }
    
    // Load page into frame
    loadFromBackingStore(processId, pageNumber, frameIndex);
    pagesPagedIn++;
    
    // Update frame information
    physicalMemory[frameIndex].isOccupied = true;
    physicalMemory[frameIndex].processId = processId;
    physicalMemory[frameIndex].pageNumber = pageNumber;
    physicalMemory[frameIndex].isDirty = false;
    
    // Update process page table
    procMem.pageTable[pageNumber] = frameIndex;
    
    // Update total memory used when a page is loaded
    totalMemoryUsed += memoryPerFrame;
    
    return true;
}

int MemoryManager::findVictimFrame() {
    // Simple FIFO replacement - find first occupied frame
    for (int i = 0; i < totalFrames; i++) {
        if (physicalMemory[i].isOccupied) {
            return i;
        }
    }
    return -1;
}

void MemoryManager::saveToBackingStore(int processId, int pageNumber) {
    if (!backingStore.is_open()) return;
    
    auto it = processes.find(processId);
    if (it == processes.end()) return;
    
    // Find the frame containing this page
    int frameIndex = it->second.pageTable[pageNumber];
    if (frameIndex == -1) return;
    
    // Write to backing store
    backingStore << "P" << processId << "_PAGE" << pageNumber << ":";
    for (int i = 0; i < 256; i++) {
        backingStore << physicalMemory[frameIndex].data[i];
        if (i < 255) backingStore << ",";
    }
    backingStore << std::endl;
    backingStore.flush();
}

void MemoryManager::loadFromBackingStore(int processId, int pageNumber, int frameNumber) {
    if (!backingStore.is_open()) return;
    
    // Try to load from backing store
    std::ifstream bsFile("csopesy-backing-store.txt");
    if (!bsFile.is_open()) {
        // No backing store data, initialize with zeros
        for (int i = 0; i < 256; i++) {
            physicalMemory[frameNumber].data[i] = 0;
        }
        return;
    }
    
    std::string line;
    std::string expectedKey = "P" + std::to_string(processId) + "_PAGE" + std::to_string(pageNumber) + ":";
    
    while (std::getline(bsFile, line)) {
        if (line.substr(0, expectedKey.length()) == expectedKey) {
            // Found the page data
            std::string data = line.substr(expectedKey.length());
            std::stringstream ss(data);
            std::string token;
            int i = 0;
            
            while (std::getline(ss, token, ',') && i < 256) {
                physicalMemory[frameNumber].data[i++] = std::stoi(token);
            }
            return;
        }
    }
    
    // Page not found in backing store, initialize with zeros
    for (int i = 0; i < 256; i++) {
        physicalMemory[frameNumber].data[i] = 0;
    }
}

bool MemoryManager::setVariable(int processId, const std::string& varName, uint16_t value) {
    std::lock_guard<std::mutex> lock(memoryMutex);
    
    auto it = processes.find(processId);
    if (it == processes.end()) return false;
    
    // Check if symbol table is full (32 variables max)
    if (it->second.symbolTable.size() >= 32) return false;
    
    it->second.symbolTable[varName] = value;
    return true;
}

uint16_t MemoryManager::getVariable(int processId, const std::string& varName) {
    std::lock_guard<std::mutex> lock(memoryMutex);
    
    auto it = processes.find(processId);
    if (it == processes.end()) return 0;
    
    auto varIt = it->second.symbolTable.find(varName);
    if (varIt == it->second.symbolTable.end()) return 0;
    
    return varIt->second;
}

void MemoryManager::printProcessSmi() {
    std::lock_guard<std::mutex> lock(memoryMutex);
    
    std::cout << "\n=== CSOPESY Memory Summary (process-smi) ===" << std::endl;
    std::cout << "Total Memory: " << (maxOverallMemory / 1024) << " KB" << std::endl;
    std::cout << "Used Memory: " << (totalMemoryUsed / 1024) << " KB" << std::endl;
    std::cout << "Free Memory: " << ((maxOverallMemory - totalMemoryUsed) / 1024) << " KB" << std::endl;
    std::cout << "Memory Utilization: " << std::fixed << std::setprecision(2) 
              << (double)totalMemoryUsed / maxOverallMemory * 100.0 << "%" << std::endl;
    
    std::cout << "\nProcess List:" << std::endl;
    std::cout << std::setw(15) << "Process Name" << std::setw(10) << "Memory" 
              << std::setw(10) << "Pages" << std::setw(10) << "Status" << std::endl;
    std::cout << std::string(50, '-') << std::endl;
    
    for (const auto& pair : processes) {
        const ProcessMemory& proc = pair.second;
        int pagesInMemory = 0;
        for (int frame : proc.pageTable) {
            if (frame != -1) pagesInMemory++;
        }
        
        std::cout << std::setw(15) << proc.processName 
                  << std::setw(10) << (proc.totalMemory / 1024) << " KB"
                  << std::setw(10) << pagesInMemory << "/" << proc.pageTable.size()
                  << std::setw(10) << (proc.isActive ? "Active" : "Inactive") << std::endl;
    }
}

void MemoryManager::printVmstat() {
    std::lock_guard<std::mutex> lock(memoryMutex);
    
    std::cout << "\n=== CSOPESY Virtual Memory Statistics (vmstat) ===" << std::endl;
    std::cout << "Memory Statistics:" << std::endl;
    std::cout << "  Total Memory: " << (maxOverallMemory / 1024) << " KB" << std::endl;
    std::cout << "  Used Memory: " << (totalMemoryUsed / 1024) << " KB" << std::endl;
    std::cout << "  Free Memory: " << ((maxOverallMemory - totalMemoryUsed) / 1024) << " KB" << std::endl;
    
    std::cout << "\nPage Statistics:" << std::endl;
    std::cout << "  Total Frames: " << totalFrames << std::endl;
    std::cout << "  Free Frames: " << freeFrames.size() << std::endl;
    std::cout << "  Used Frames: " << (totalFrames - freeFrames.size()) << std::endl;
    std::cout << "  Pages Paged In: " << pagesPagedIn << std::endl;
    std::cout << "  Pages Paged Out: " << pagesPagedOut << std::endl;
    
    std::cout << "\nProcess Statistics:" << std::endl;
    int activeProcesses = 0;
    int totalPages = 0;
    for (const auto& pair : processes) {
        if (pair.second.isActive) activeProcesses++;
        totalPages += pair.second.pageTable.size();
    }
    std::cout << "  Active Processes: " << activeProcesses << std::endl;
    std::cout << "  Total Processes: " << processes.size() << std::endl;
    std::cout << "  Total Pages: " << totalPages << std::endl;
}

bool MemoryManager::processExists(int processId) const {
    return processes.find(processId) != processes.end();
}

const ProcessMemory* MemoryManager::getProcessMemory(int processId) const {
    auto it = processes.find(processId);
    return (it != processes.end()) ? &(it->second) : nullptr;
}

void MemoryManager::updateBackingStoreFile() {
    // This method can be used to update the backing store file
    // For now, it's a placeholder implementation
    if (backingStore.is_open()) {
        backingStore.flush();
    }
}

std::vector<ProcessMemory> MemoryManager::getAllProcesses() const {
    std::vector<ProcessMemory> result;
    for (const auto& pair : processes) {
        result.push_back(pair.second);
    }
    return result;
} 