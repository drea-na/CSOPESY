#ifndef PAGETABLE_H
#define PAGETABLE_H

#include <vector>
#include <map>
#include <string>
#include <cstdint>
#include <mutex>
#include <fstream>
#include <sstream>
#include <iomanip>

// Page Table Entry (PTE)
struct PageTableEntry {
    uint32_t frameNumber;      // Physical frame number
    bool valid;                // Valid bit (page is in memory)
    bool dirty;                // Dirty bit (page has been modified)
    bool referenced;           // Referenced bit (for LRU)
    uint32_t timestamp;        // Timestamp for LRU replacement
    
    PageTableEntry() : frameNumber(0), valid(false), dirty(false), referenced(false), timestamp(0) {}
};

// Page Table class for demand paging
class PageTable {
private:
    std::map<uint32_t, PageTableEntry> entries;  // Virtual page number -> PTE
    std::string processName;
    uint32_t pageSize;
    uint32_t virtualAddressSpace;
    mutable std::mutex tableMutex;
    
    // LRU tracking
    std::vector<uint32_t> lruQueue;
    uint32_t currentTimestamp;
    
public:
    PageTable(const std::string& procName, uint32_t pageSz = 4096, uint32_t virtualSpace = 0x100000);
    
    // Address translation
    bool translateAddress(uint32_t virtualAddr, uint32_t& physicalAddr);
    
    // Page table management
    bool isPageValid(uint32_t virtualPageNum);
    void setPageValid(uint32_t virtualPageNum, uint32_t frameNum);
    void setPageInvalid(uint32_t virtualPageNum);
    void setPageDirty(uint32_t virtualPageNum);
    void setPageReferenced(uint32_t virtualPageNum);
    
    // Page replacement (LRU)
    uint32_t selectVictimPage();
    void updateLRU(uint32_t virtualPageNum);
    
    // Page fault handling
    bool handlePageFault(uint32_t virtualAddr, uint32_t& physicalAddr);
    
    // Getters
    uint32_t getPageSize() const { return pageSize; }
    uint32_t getVirtualAddressSpace() const { return virtualAddressSpace; }
    std::string getProcessName() const { return processName; }
    
    // Helper methods for Process class
    uint32_t virtualToPageNumber(uint32_t virtualAddr) const;
    
    // Debug and statistics
    void printPageTable() const;
    uint32_t getPageFaultCount() const { return pageFaultCount; }
    uint32_t getTotalPages() const { return entries.size(); }
    
private:
    uint32_t pageFaultCount;
    uint32_t getPageOffset(uint32_t virtualAddr) const;
    void loadPageFromBackingStore(uint32_t virtualPageNum, uint32_t frameNum);
    void savePageToBackingStore(uint32_t virtualPageNum, uint32_t frameNum);
};

// Backing Store Manager
class BackingStore {
private:
    std::string backingStoreFile;
    std::map<std::string, std::map<uint32_t, std::vector<uint8_t>>> processPages;
    mutable std::mutex storeMutex;
    
public:
    BackingStore(const std::string& filename = "csopesy-backing-store.txt");
    
    // Page operations
    bool loadPage(const std::string& processName, uint32_t pageNum, std::vector<uint8_t>& data);
    bool savePage(const std::string& processName, uint32_t pageNum, const std::vector<uint8_t>& data);
    bool pageExists(const std::string& processName, uint32_t pageNum);
    
    // File management
    void initializeBackingStore();
    void cleanupBackingStore();
    
private:
    void writeToFile();
    void readFromFile();
};

#endif // PAGETABLE_H 