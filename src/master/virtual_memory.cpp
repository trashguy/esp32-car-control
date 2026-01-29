#include "shared/config.h"

#if VIRTUAL_MEMORY

#include "master/virtual_memory.h"
#include "master/sd_handler.h"
#include <Arduino.h>
#include <esp_heap_caps.h>

// Global instance
VirtualMemory vmem;

// =============================================================================
// Constructor / Destructor
// =============================================================================

VirtualMemory::VirtualMemory()
    : _initialized(false)
    , _totalSize(0)
    , _cacheSize(0)
    , _maxCachePages(0)
    , _totalPages(0)
    , _pageTable(nullptr)
    , _cacheSlots(nullptr)
    , _cacheBuffer(nullptr)
{
    memset(&_stats, 0, sizeof(_stats));
}

VirtualMemory::~VirtualMemory() {
    shutdown();
}

// =============================================================================
// Initialization
// =============================================================================

bool VirtualMemory::init(uint32_t totalSize) {
    if (_initialized) {
        Serial.println("VMEM: Already initialized");
        return true;
    }

    // Check SD card is ready
    if (!sdIsReady()) {
        Serial.println("VMEM: SD card not ready");
        return false;
    }

    // Check PSRAM is available
    size_t psramFree = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    if (psramFree < VMEM_CACHE_SIZE) {
        Serial.printf("VMEM: Insufficient PSRAM (need %lu, have %zu)\n",
                      VMEM_CACHE_SIZE, psramFree);
        return false;
    }

    _totalSize = totalSize;
    _totalPages = totalSize / VMEM_PAGE_SIZE;
    _cacheSize = VMEM_CACHE_SIZE;
    _maxCachePages = _cacheSize / VMEM_PAGE_SIZE;

    Serial.printf("VMEM: Initializing %lu MB virtual memory\n", totalSize / (1024 * 1024));
    Serial.printf("VMEM: Page size: %d bytes, Total pages: %lu, Cache pages: %lu\n",
                  VMEM_PAGE_SIZE, _totalPages, _maxCachePages);

    // Allocate page table (maps virtual page -> cache slot, -1 if not cached)
    _pageTable = (int32_t*)calloc(_totalPages, sizeof(int32_t));
    if (!_pageTable) {
        Serial.println("VMEM: Failed to allocate page table");
        return false;
    }
    // Initialize all entries to -1 (not cached)
    for (uint32_t i = 0; i < _totalPages; i++) {
        _pageTable[i] = -1;
    }

    // Allocate cache slot descriptors
    _cacheSlots = (VMemPage*)calloc(_maxCachePages, sizeof(VMemPage));
    if (!_cacheSlots) {
        Serial.println("VMEM: Failed to allocate cache slots");
        free(_pageTable);
        _pageTable = nullptr;
        return false;
    }

    // Initialize cache slots
    for (uint32_t i = 0; i < _maxCachePages; i++) {
        _cacheSlots[i].virtualPage = 0xFFFFFFFF;  // Unused marker
        _cacheSlots[i].cachePtr = nullptr;
        _cacheSlots[i].lastAccess = 0;
        _cacheSlots[i].dirty = false;
        _cacheSlots[i].valid = false;
    }

    // Allocate PSRAM cache buffer
    _cacheBuffer = (uint8_t*)heap_caps_malloc(_cacheSize, MALLOC_CAP_SPIRAM);
    if (!_cacheBuffer) {
        Serial.println("VMEM: Failed to allocate PSRAM cache");
        free(_cacheSlots);
        free(_pageTable);
        _cacheSlots = nullptr;
        _pageTable = nullptr;
        return false;
    }

    // Assign cache pointers to slots
    for (uint32_t i = 0; i < _maxCachePages; i++) {
        _cacheSlots[i].cachePtr = _cacheBuffer + (i * VMEM_PAGE_SIZE);
    }

    // Create or verify swap file
    int32_t swapSize = sdFileSize(VMEM_SWAP_FILE);
    if (swapSize < 0 || (uint32_t)swapSize < totalSize) {
        Serial.printf("VMEM: Creating swap file (%lu MB)...\n", totalSize / (1024 * 1024));
        if (!sdCreateSparseFile(VMEM_SWAP_FILE, totalSize)) {
            Serial.println("VMEM: Failed to create swap file");
            heap_caps_free(_cacheBuffer);
            free(_cacheSlots);
            free(_pageTable);
            _cacheBuffer = nullptr;
            _cacheSlots = nullptr;
            _pageTable = nullptr;
            return false;
        }
        Serial.println("VMEM: Swap file created");
    } else {
        Serial.printf("VMEM: Using existing swap file (%d MB)\n", swapSize / (1024 * 1024));
    }

    _stats.maxPages = _maxCachePages;
    _initialized = true;

    Serial.printf("VMEM: Ready - %lu MB virtual, %lu MB cache (%lu pages)\n",
                  _totalSize / (1024 * 1024),
                  _cacheSize / (1024 * 1024),
                  _maxCachePages);

    return true;
}

void VirtualMemory::shutdown() {
    if (!_initialized) return;

    Serial.println("VMEM: Shutting down...");

    // Flush all dirty pages
    flush();

    // Free resources
    if (_cacheBuffer) {
        heap_caps_free(_cacheBuffer);
        _cacheBuffer = nullptr;
    }
    if (_cacheSlots) {
        free(_cacheSlots);
        _cacheSlots = nullptr;
    }
    if (_pageTable) {
        free(_pageTable);
        _pageTable = nullptr;
    }

    _initialized = false;
    Serial.println("VMEM: Shutdown complete");
}

// =============================================================================
// Memory Operations
// =============================================================================

int32_t VirtualMemory::read(uint32_t vaddr, void* buffer, size_t length) {
    if (!_initialized || buffer == nullptr) return -1;
    if (vaddr + length > _totalSize) return -1;

    uint8_t* dst = (uint8_t*)buffer;
    size_t remaining = length;
    uint32_t currentAddr = vaddr;

    while (remaining > 0) {
        uint32_t pageNum = currentAddr / VMEM_PAGE_SIZE;
        uint32_t pageOffset = currentAddr % VMEM_PAGE_SIZE;
        size_t bytesInPage = VMEM_PAGE_SIZE - pageOffset;
        if (bytesInPage > remaining) bytesInPage = remaining;

        // Get page pointer (loads from SD if needed)
        uint8_t* pagePtr = getPagePtr(pageNum);
        if (!pagePtr) {
            return -1;  // Page fault failed
        }

        // Copy data
        memcpy(dst, pagePtr + pageOffset, bytesInPage);

        dst += bytesInPage;
        currentAddr += bytesInPage;
        remaining -= bytesInPage;
    }

    return (int32_t)length;
}

int32_t VirtualMemory::write(uint32_t vaddr, const void* data, size_t length) {
    if (!_initialized || data == nullptr) return -1;
    if (vaddr + length > _totalSize) return -1;

    const uint8_t* src = (const uint8_t*)data;
    size_t remaining = length;
    uint32_t currentAddr = vaddr;

    while (remaining > 0) {
        uint32_t pageNum = currentAddr / VMEM_PAGE_SIZE;
        uint32_t pageOffset = currentAddr % VMEM_PAGE_SIZE;
        size_t bytesInPage = VMEM_PAGE_SIZE - pageOffset;
        if (bytesInPage > remaining) bytesInPage = remaining;

        // Get page pointer (loads from SD if needed)
        uint8_t* pagePtr = getPagePtr(pageNum);
        if (!pagePtr) {
            return -1;  // Page fault failed
        }

        // Copy data and mark dirty
        memcpy(pagePtr + pageOffset, src, bytesInPage);

        int32_t slot = _pageTable[pageNum];
        if (slot >= 0) {
            _cacheSlots[slot].dirty = true;
        }

        src += bytesInPage;
        currentAddr += bytesInPage;
        remaining -= bytesInPage;
    }

    return (int32_t)length;
}

bool VirtualMemory::zero(uint32_t vaddr, size_t length) {
    if (!_initialized) return false;
    if (vaddr + length > _totalSize) return false;

    size_t remaining = length;
    uint32_t currentAddr = vaddr;

    while (remaining > 0) {
        uint32_t pageNum = currentAddr / VMEM_PAGE_SIZE;
        uint32_t pageOffset = currentAddr % VMEM_PAGE_SIZE;
        size_t bytesInPage = VMEM_PAGE_SIZE - pageOffset;
        if (bytesInPage > remaining) bytesInPage = remaining;

        // Get page pointer
        uint8_t* pagePtr = getPagePtr(pageNum);
        if (!pagePtr) return false;

        // Zero the region
        memset(pagePtr + pageOffset, 0, bytesInPage);

        int32_t slot = _pageTable[pageNum];
        if (slot >= 0) {
            _cacheSlots[slot].dirty = true;
        }

        currentAddr += bytesInPage;
        remaining -= bytesInPage;
    }

    return true;
}

// =============================================================================
// Cache Control
// =============================================================================

bool VirtualMemory::flush() {
    if (!_initialized) return false;

    uint32_t flushed = 0;
    for (uint32_t i = 0; i < _maxCachePages; i++) {
        if (_cacheSlots[i].valid && _cacheSlots[i].dirty) {
            if (writeBackPage(i)) {
                flushed++;
            }
        }
    }

    if (flushed > 0) {
        Serial.printf("VMEM: Flushed %lu dirty pages\n", flushed);
    }
    return true;
}

bool VirtualMemory::flushRange(uint32_t vaddr, size_t length) {
    if (!_initialized) return false;

    uint32_t startPage = vaddr / VMEM_PAGE_SIZE;
    uint32_t endPage = (vaddr + length - 1) / VMEM_PAGE_SIZE;

    for (uint32_t pageNum = startPage; pageNum <= endPage && pageNum < _totalPages; pageNum++) {
        int32_t slot = _pageTable[pageNum];
        if (slot >= 0 && _cacheSlots[slot].dirty) {
            writeBackPage(slot);
        }
    }

    return true;
}

void VirtualMemory::prefetch(uint32_t vaddr, size_t length) {
    if (!_initialized) return;

    uint32_t startPage = vaddr / VMEM_PAGE_SIZE;
    uint32_t endPage = (vaddr + length - 1) / VMEM_PAGE_SIZE;

    // Prefetch up to a reasonable number of pages
    uint32_t maxPrefetch = 8;
    uint32_t prefetched = 0;

    for (uint32_t pageNum = startPage;
         pageNum <= endPage && pageNum < _totalPages && prefetched < maxPrefetch;
         pageNum++) {
        if (_pageTable[pageNum] < 0) {
            // Page not in cache, load it
            loadPage(pageNum);
            prefetched++;
        }
    }
}

void VirtualMemory::invalidate() {
    if (!_initialized) return;

    // Mark all cache slots as invalid (discards dirty data!)
    for (uint32_t i = 0; i < _maxCachePages; i++) {
        if (_cacheSlots[i].valid) {
            uint32_t vpage = _cacheSlots[i].virtualPage;
            if (vpage < _totalPages) {
                _pageTable[vpage] = -1;
            }
            _cacheSlots[i].valid = false;
            _cacheSlots[i].dirty = false;
            _cacheSlots[i].virtualPage = 0xFFFFFFFF;
        }
    }
    _stats.pagesLoaded = 0;
}

// =============================================================================
// Statistics
// =============================================================================

void VirtualMemory::resetStats() {
    _stats.hits = 0;
    _stats.misses = 0;
    _stats.evictions = 0;
    _stats.writebacks = 0;
    _stats.bytesRead = 0;
    _stats.bytesWritten = 0;
    // Keep pagesLoaded and maxPages
}

float VirtualMemory::hitRate() const {
    uint32_t total = _stats.hits + _stats.misses;
    if (total == 0) return 1.0f;
    return (float)_stats.hits / (float)total;
}

void VirtualMemory::printStats() {
    Serial.println("=== Virtual Memory Statistics ===");
    Serial.printf("Cache hits:      %lu\n", _stats.hits);
    Serial.printf("Cache misses:    %lu\n", _stats.misses);
    Serial.printf("Hit rate:        %.1f%%\n", hitRate() * 100.0f);
    Serial.printf("Pages loaded:    %lu / %lu\n", _stats.pagesLoaded, _stats.maxPages);
    Serial.printf("Evictions:       %lu\n", _stats.evictions);
    Serial.printf("Write-backs:     %lu\n", _stats.writebacks);
    Serial.printf("SD bytes read:   %lu KB\n", _stats.bytesRead / 1024);
    Serial.printf("SD bytes written:%lu KB\n", _stats.bytesWritten / 1024);
    Serial.println("=================================");
}

// =============================================================================
// Internal Helpers
// =============================================================================

int32_t VirtualMemory::findCacheSlot(uint32_t virtualPage) {
    return _pageTable[virtualPage];
}

int32_t VirtualMemory::loadPage(uint32_t virtualPage) {
    if (virtualPage >= _totalPages) return -1;

    // Find a free cache slot
    int32_t slot = -1;
    for (uint32_t i = 0; i < _maxCachePages; i++) {
        if (!_cacheSlots[i].valid) {
            slot = i;
            break;
        }
    }

    // No free slot, need to evict
    if (slot < 0) {
        slot = evictPage();
        if (slot < 0) {
            Serial.println("VMEM: Failed to evict page");
            return -1;
        }
    }

    // Read page from SD card
    uint32_t fileOffset = virtualPage * VMEM_PAGE_SIZE;
    int32_t bytesRead = sdReadFileAt(VMEM_SWAP_FILE, fileOffset,
                                      _cacheSlots[slot].cachePtr, VMEM_PAGE_SIZE);
    if (bytesRead < 0) {
        Serial.printf("VMEM: Failed to read page %lu from SD\n", virtualPage);
        return -1;
    }

    // Update slot
    _cacheSlots[slot].virtualPage = virtualPage;
    _cacheSlots[slot].valid = true;
    _cacheSlots[slot].dirty = false;
    _cacheSlots[slot].lastAccess = millis();

    // Update page table
    _pageTable[virtualPage] = slot;

    // Update stats
    _stats.misses++;
    _stats.pagesLoaded++;
    _stats.bytesRead += VMEM_PAGE_SIZE;

    return slot;
}

int32_t VirtualMemory::evictPage() {
    // Find LRU (least recently used) page
    int32_t lruSlot = -1;
    uint32_t oldestTime = 0xFFFFFFFF;

    for (uint32_t i = 0; i < _maxCachePages; i++) {
        if (_cacheSlots[i].valid && _cacheSlots[i].lastAccess < oldestTime) {
            oldestTime = _cacheSlots[i].lastAccess;
            lruSlot = i;
        }
    }

    if (lruSlot < 0) {
        return -1;  // No valid pages to evict (shouldn't happen)
    }

    // Write back if dirty
    if (_cacheSlots[lruSlot].dirty) {
        if (!writeBackPage(lruSlot)) {
            Serial.println("VMEM: Write-back failed during eviction");
            // Continue anyway - data loss, but don't deadlock
        }
    }

    // Update page table to mark page as not cached
    uint32_t evictedPage = _cacheSlots[lruSlot].virtualPage;
    if (evictedPage < _totalPages) {
        _pageTable[evictedPage] = -1;
    }

    // Mark slot as free
    _cacheSlots[lruSlot].valid = false;
    _cacheSlots[lruSlot].virtualPage = 0xFFFFFFFF;

    _stats.evictions++;
    _stats.pagesLoaded--;

    return lruSlot;
}

bool VirtualMemory::writeBackPage(int32_t slot) {
    if (slot < 0 || (uint32_t)slot >= _maxCachePages) return false;
    if (!_cacheSlots[slot].valid || !_cacheSlots[slot].dirty) return true;  // Nothing to do

    uint32_t virtualPage = _cacheSlots[slot].virtualPage;
    uint32_t fileOffset = virtualPage * VMEM_PAGE_SIZE;

    int32_t bytesWritten = sdWriteFileAt(VMEM_SWAP_FILE, fileOffset,
                                          _cacheSlots[slot].cachePtr, VMEM_PAGE_SIZE);
    if (bytesWritten != VMEM_PAGE_SIZE) {
        Serial.printf("VMEM: Write-back failed for page %lu\n", virtualPage);
        return false;
    }

    _cacheSlots[slot].dirty = false;
    _stats.writebacks++;
    _stats.bytesWritten += VMEM_PAGE_SIZE;

    return true;
}

uint8_t* VirtualMemory::getPagePtr(uint32_t virtualPage) {
    if (virtualPage >= _totalPages) return nullptr;

    int32_t slot = _pageTable[virtualPage];

    if (slot >= 0) {
        // Cache hit
        _stats.hits++;
        touchPage(slot);
        return _cacheSlots[slot].cachePtr;
    }

    // Cache miss - load page
    slot = loadPage(virtualPage);
    if (slot < 0) return nullptr;

    return _cacheSlots[slot].cachePtr;
}

void VirtualMemory::touchPage(int32_t slot) {
    if (slot >= 0 && (uint32_t)slot < _maxCachePages) {
        _cacheSlots[slot].lastAccess = millis();
    }
}

#endif // VIRTUAL_MEMORY
