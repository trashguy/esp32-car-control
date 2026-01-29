#ifndef VIRTUAL_MEMORY_H
#define VIRTUAL_MEMORY_H

#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>
#include "shared/config.h"

#if VIRTUAL_MEMORY

// Virtual memory system using SD card as backing store with PSRAM cache
// Provides 16MB+ memory buffers for large data processing

// =============================================================================
// Configuration (derived from config.h)
// =============================================================================

#define VMEM_TOTAL_SIZE     (VIRTUAL_MEMORY_SIZE_MB * 1024UL * 1024UL)
#define VMEM_PAGE_SIZE      VIRTUAL_MEMORY_PAGE_SIZE
#define VMEM_CACHE_SIZE     (VIRTUAL_MEMORY_CACHE_MB * 1024UL * 1024UL)
#define VMEM_MAX_PAGES      (VMEM_CACHE_SIZE / VMEM_PAGE_SIZE)
#define VMEM_TOTAL_PAGES    (VMEM_TOTAL_SIZE / VMEM_PAGE_SIZE)

// Swap file path on SD card
#define VMEM_SWAP_FILE      "/vmem_swap.bin"

// =============================================================================
// Page Descriptor
// =============================================================================

typedef struct {
    uint32_t virtualPage;   // Virtual page number (0xFFFFFFFF = unused)
    uint8_t* cachePtr;      // Pointer to PSRAM cache location
    uint32_t lastAccess;    // Timestamp for LRU eviction
    bool dirty;             // Needs write-back before eviction
    bool valid;             // Page contains valid data
} VMemPage;

// =============================================================================
// Statistics
// =============================================================================

typedef struct {
    uint32_t hits;          // Cache hits
    uint32_t misses;        // Cache misses (page faults)
    uint32_t evictions;     // Pages evicted from cache
    uint32_t writebacks;    // Dirty pages written to SD
    uint32_t bytesRead;     // Total bytes read from SD
    uint32_t bytesWritten;  // Total bytes written to SD
    uint32_t pagesLoaded;   // Currently loaded pages
    uint32_t maxPages;      // Maximum pages that fit in cache
} VMemStats;

// =============================================================================
// VirtualMemory Class
// =============================================================================

class VirtualMemory {
public:
    VirtualMemory();
    ~VirtualMemory();

    // Initialize virtual memory system
    // totalSize: total virtual address space (default from config)
    // Returns true on success
    bool init(uint32_t totalSize = VMEM_TOTAL_SIZE);

    // Shutdown and flush all dirty pages
    void shutdown();

    // Check if initialized
    bool isReady() const { return _initialized; }

    // ==========================================================================
    // Memory Operations
    // ==========================================================================

    // Read data from virtual memory
    // vaddr: virtual address (0 to totalSize-1)
    // buffer: destination buffer
    // length: bytes to read
    // Returns bytes read, or -1 on error
    int32_t read(uint32_t vaddr, void* buffer, size_t length);

    // Write data to virtual memory
    // vaddr: virtual address (0 to totalSize-1)
    // data: source data
    // length: bytes to write
    // Returns bytes written, or -1 on error
    int32_t write(uint32_t vaddr, const void* data, size_t length);

    // Zero-fill a range of virtual memory
    bool zero(uint32_t vaddr, size_t length);

    // ==========================================================================
    // Cache Control
    // ==========================================================================

    // Flush all dirty pages to SD card
    bool flush();

    // Flush specific virtual address range
    bool flushRange(uint32_t vaddr, size_t length);

    // Prefetch pages into cache (hint for sequential access)
    void prefetch(uint32_t vaddr, size_t length);

    // Invalidate cache (discard without writing back - use with caution!)
    void invalidate();

    // ==========================================================================
    // Statistics
    // ==========================================================================

    // Get current statistics
    VMemStats getStats() const { return _stats; }

    // Reset statistics counters
    void resetStats();

    // Calculate hit rate (0.0 - 1.0)
    float hitRate() const;

    // Print statistics to Serial
    void printStats();

    // ==========================================================================
    // Configuration Getters
    // ==========================================================================

    uint32_t getTotalSize() const { return _totalSize; }
    uint32_t getPageSize() const { return VMEM_PAGE_SIZE; }
    uint32_t getCacheSize() const { return _cacheSize; }
    uint32_t getMaxCachePages() const { return _maxCachePages; }

private:
    bool _initialized;
    uint32_t _totalSize;
    uint32_t _cacheSize;
    uint32_t _maxCachePages;
    uint32_t _totalPages;

    // Page table (all possible virtual pages)
    // Maps virtual page number to cache slot (-1 if not cached)
    int32_t* _pageTable;

    // Cache slots
    VMemPage* _cacheSlots;

    // PSRAM cache buffer
    uint8_t* _cacheBuffer;

    // Statistics
    VMemStats _stats;

    // Internal helpers
    int32_t findCacheSlot(uint32_t virtualPage);
    int32_t loadPage(uint32_t virtualPage);
    int32_t evictPage();
    bool writeBackPage(int32_t slot);
    uint8_t* getPagePtr(uint32_t virtualPage);
    void touchPage(int32_t slot);
};

// Global instance (extern declaration)
extern VirtualMemory vmem;

#endif // VIRTUAL_MEMORY

#endif // VIRTUAL_MEMORY_H
