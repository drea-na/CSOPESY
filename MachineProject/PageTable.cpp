#include "PageTable.h"
#include <iostream>
#include <algorithm>
#include <chrono>
#include <random>

// Global backing store instance
static BackingStore* globalBackingStore = nullptr;

// PageTable implementation
PageTable::PageTable(const std::string& procName, uint32_t pageSz, uint32_t virtualSpace)
    : processName(procName), pageSize(pageSz), virtualAddressSpace(virtualSpace), 
      currentTimestamp(0), pageFaultCount(0) {
    
    // Initialize global backing store if not exists (lazy initialization)
    // We'll initialize it when needed to avoid memory issues
}

uint32_t PageTable::virtualToPageNumber(uint32_t virtualAddr) const {
    return virtualAddr / pageSize;
}

uint32_t PageTable::getPageOffset(uint32_t virtualAddr) const {
    return virtualAddr % pageSize;
}

bool PageTable::translateAddress(uint32_t virtualAddr, uint32_t& physicalAddr) {
    std::lock_guard<std::mutex> lock(tableMutex);
    
    uint32_t pageNum = virtualToPageNumber(virtualAddr);
    uint32_t offset = getPageOffset(virtualAddr);
    
    auto it = entries.find(pageNum);
    if (it == entries.end() || !it->second.valid) {
        // Page fault - page not in memory
        pageFaultCount++;
        return false;
    }
    
    // Page is valid, calculate physical address
    physicalAddr = (it->second.frameNumber * pageSize) + offset;
    
    // Update reference bit and LRU
    it->second.referenced = true;
    updateLRU(pageNum);
    
    return true;
}

bool PageTable::isPageValid(uint32_t virtualPageNum) {
    std::lock_guard<std::mutex> lock(tableMutex);
    auto it = entries.find(virtualPageNum);
    return (it != entries.end() && it->second.valid);
}

void PageTable::setPageValid(uint32_t virtualPageNum, uint32_t frameNum) {
    std::lock_guard<std::mutex> lock(tableMutex);
    
    PageTableEntry& entry = entries[virtualPageNum];
    entry.frameNumber = frameNum;
    entry.valid = true;
    entry.dirty = false;
    entry.referenced = true;
    entry.timestamp = ++currentTimestamp;
    
    updateLRU(virtualPageNum);
}

void PageTable::setPageInvalid(uint32_t virtualPageNum) {
    std::lock_guard<std::mutex> lock(tableMutex);
    auto it = entries.find(virtualPageNum);
    if (it != entries.end()) {
        it->second.valid = false;
    }
}

void PageTable::setPageDirty(uint32_t virtualPageNum) {
    std::lock_guard<std::mutex> lock(tableMutex);
    auto it = entries.find(virtualPageNum);
    if (it != entries.end()) {
        it->second.dirty = true;
    }
}

void PageTable::setPageReferenced(uint32_t virtualPageNum) {
    std::lock_guard<std::mutex> lock(tableMutex);
    auto it = entries.find(virtualPageNum);
    if (it != entries.end()) {
        it->second.referenced = true;
        updateLRU(virtualPageNum);
    }
}

uint32_t PageTable::selectVictimPage() {
    std::lock_guard<std::mutex> lock(tableMutex);
    
    if (lruQueue.empty()) {
        return 0; // No pages to evict
    }
    
    // Select the least recently used page (front of queue)
    uint32_t victimPage = lruQueue.front();
    lruQueue.erase(lruQueue.begin());
    
    return victimPage;
}

void PageTable::updateLRU(uint32_t virtualPageNum) {
    // Remove page from current position in LRU queue
    auto it = std::find(lruQueue.begin(), lruQueue.end(), virtualPageNum);
    if (it != lruQueue.end()) {
        lruQueue.erase(it);
    }
    
    // Add to end (most recently used)
    lruQueue.push_back(virtualPageNum);
}

bool PageTable::handlePageFault(uint32_t virtualAddr, uint32_t& physicalAddr) {
    std::lock_guard<std::mutex> lock(tableMutex);
    
    uint32_t pageNum = virtualToPageNumber(virtualAddr);
    uint32_t offset = getPageOffset(virtualAddr);
    
    // Initialize global backing store if not exists
    if (!globalBackingStore) {
        globalBackingStore = new BackingStore();
        globalBackingStore->initializeBackingStore();
    }
    
    // Check if page exists in backing store
    if (!globalBackingStore->pageExists(processName, pageNum)) {
        // Page doesn't exist, create it with zeros
        std::vector<uint8_t> pageData(pageSize, 0);
        globalBackingStore->savePage(processName, pageNum, pageData);
    }
    
    // Find a free frame or evict a page
    uint32_t frameNum = 0; // This should be provided by MemoryManager
    // For now, we'll use a simple approach - this should be coordinated with MemoryManager
    
    // Load page from backing store
    std::vector<uint8_t> pageData;
    if (globalBackingStore->loadPage(processName, pageNum, pageData)) {
        setPageValid(pageNum, frameNum);
        physicalAddr = (frameNum * pageSize) + offset;
        return true;
    }
    
    return false;
}

void PageTable::loadPageFromBackingStore(uint32_t virtualPageNum, uint32_t frameNum) {
    if (globalBackingStore) {
        std::vector<uint8_t> pageData;
        if (globalBackingStore->loadPage(processName, virtualPageNum, pageData)) {
            // In a real implementation, this would copy data to the physical frame
            // For now, we just mark the page as valid
            setPageValid(virtualPageNum, frameNum);
        }
    }
}

void PageTable::savePageToBackingStore(uint32_t virtualPageNum, uint32_t frameNum) {
    if (globalBackingStore) {
        // In a real implementation, this would copy data from the physical frame
        // For now, we just create dummy data
        std::vector<uint8_t> pageData(pageSize, 0);
        globalBackingStore->savePage(processName, virtualPageNum, pageData);
    }
}

void PageTable::printPageTable() const {
    std::lock_guard<std::mutex> lock(tableMutex);
    
    std::cout << "\n=== Page Table for Process: " << processName << " ===" << std::endl;
    std::cout << "Page Size: " << pageSize << " bytes" << std::endl;
    std::cout << "Virtual Address Space: 0x" << std::hex << virtualAddressSpace << std::dec << std::endl;
    std::cout << "Page Faults: " << pageFaultCount << std::endl;
    std::cout << "Total Pages: " << entries.size() << std::endl;
    std::cout << std::endl;
    
    std::cout << "VPN\tFrame\tValid\tDirty\tRef\tTimestamp" << std::endl;
    std::cout << "---\t-----\t-----\t-----\t---\t---------" << std::endl;
    
    for (const auto& entry : entries) {
        std::cout << "0x" << std::hex << entry.first << "\t"
                  << std::dec << entry.second.frameNumber << "\t"
                  << (entry.second.valid ? "Y" : "N") << "\t"
                  << (entry.second.dirty ? "Y" : "N") << "\t"
                  << (entry.second.referenced ? "Y" : "N") << "\t"
                  << entry.second.timestamp << std::endl;
    }
    std::cout << std::dec;
}

// BackingStore implementation
BackingStore::BackingStore(const std::string& filename) : backingStoreFile(filename) {
    readFromFile();
}

bool BackingStore::loadPage(const std::string& processName, uint32_t pageNum, std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(storeMutex);
    
    auto processIt = processPages.find(processName);
    if (processIt == processPages.end()) {
        return false;
    }
    
    auto pageIt = processIt->second.find(pageNum);
    if (pageIt == processIt->second.end()) {
        return false;
    }
    
    data = pageIt->second;
    return true;
}

bool BackingStore::savePage(const std::string& processName, uint32_t pageNum, const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(storeMutex);
    
    processPages[processName][pageNum] = data;
    writeToFile();
    return true;
}

bool BackingStore::pageExists(const std::string& processName, uint32_t pageNum) {
    std::lock_guard<std::mutex> lock(storeMutex);
    
    auto processIt = processPages.find(processName);
    if (processIt == processPages.end()) {
        return false;
    }
    
    return processIt->second.find(pageNum) != processIt->second.end();
}

void BackingStore::initializeBackingStore() {
    std::lock_guard<std::mutex> lock(storeMutex);
    
    // Create the backing store file if it doesn't exist
    std::ofstream file(backingStoreFile, std::ios::app);
    if (file.is_open()) {
        file.close();
    }
    
    readFromFile();
}

void BackingStore::cleanupBackingStore() {
    std::lock_guard<std::mutex> lock(storeMutex);
    writeToFile();
}

void BackingStore::writeToFile() {
    std::ofstream file(backingStoreFile, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open backing store file for writing: " << backingStoreFile << std::endl;
        return;
    }
    
    // Write process count
    size_t processCount = processPages.size();
    file.write(reinterpret_cast<const char*>(&processCount), sizeof(processCount));
    
    for (const auto& process : processPages) {
        // Write process name length and name
        size_t nameLength = process.first.length();
        file.write(reinterpret_cast<const char*>(&nameLength), sizeof(nameLength));
        file.write(process.first.c_str(), nameLength);
        
        // Write page count for this process
        size_t pageCount = process.second.size();
        file.write(reinterpret_cast<const char*>(&pageCount), sizeof(pageCount));
        
        for (const auto& page : process.second) {
            // Write page number
            file.write(reinterpret_cast<const char*>(&page.first), sizeof(page.first));
            
            // Write page data size and data
            size_t dataSize = page.second.size();
            file.write(reinterpret_cast<const char*>(&dataSize), sizeof(dataSize));
            file.write(reinterpret_cast<const char*>(page.second.data()), dataSize);
        }
    }
}

void BackingStore::readFromFile() {
    std::ifstream file(backingStoreFile, std::ios::binary);
    if (!file.is_open()) {
        // File doesn't exist yet, that's okay
        return;
    }
    
    processPages.clear();
    
    // Read process count
    size_t processCount;
    file.read(reinterpret_cast<char*>(&processCount), sizeof(processCount));
    
    for (size_t i = 0; i < processCount; ++i) {
        // Read process name
        size_t nameLength;
        file.read(reinterpret_cast<char*>(&nameLength), sizeof(nameLength));
        
        std::string processName(nameLength, '\0');
        file.read(&processName[0], nameLength);
        
        // Read page count for this process
        size_t pageCount;
        file.read(reinterpret_cast<char*>(&pageCount), sizeof(pageCount));
        
        for (size_t j = 0; j < pageCount; ++j) {
            // Read page number
            uint32_t pageNum;
            file.read(reinterpret_cast<char*>(&pageNum), sizeof(pageNum));
            
            // Read page data
            size_t dataSize;
            file.read(reinterpret_cast<char*>(&dataSize), sizeof(dataSize));
            
            std::vector<uint8_t> pageData(dataSize);
            file.read(reinterpret_cast<char*>(pageData.data()), dataSize);
            
            processPages[processName][pageNum] = pageData;
        }
    }
} 