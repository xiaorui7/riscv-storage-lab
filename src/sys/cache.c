#include <stdint.h>
#include "cache.h"
#include "error.h"
#include "heap.h"
#include "conf.h"
#include "io.h"
#include "string.h"

// the cache block structure
struct cache_block {
    unsigned long long start_pos;   // the starting position of the data in backing device
    int isempty;                    // if this cache_block is empty, isempty = 1; else, isempty = 0
    // if the data in the cache block has been modified, that is, is different from the data in the backing device,
    // and needs to be written back to the backing device, dirty = 1; else, dirty = 0
    int dirty;                      // dirty bit
    int refcnt;                     // reference count
    char * data;                    // cached data
    uint64_t last_release_time;     // time of last cache_release_block()
};

// the complete cache structure
struct cache {
    struct io * bkgio;                          // pointer to the I/O interface of the backing device
    struct cache_block blocks[CACHE_CAPACITY];  // cache block array
    uint64_t clock;
    struct cache_stats stats;
};

/* Keep dirty data retryable until a complete block reaches the backing I/O. */
static int write_block(struct cache * cache, struct cache_block * block) {
    cache->stats.backing_writes++;
    long n = iowriteat(cache->bkgio, block->start_pos, block->data, CACHE_BLKSZ);
    if (n != CACHE_BLKSZ)
        return n < 0 ? (int)n : -EIO;
    block->dirty = 0;
    return 0;
}


/* int create_cache(struct io * bkgio, struct cache ** cptr)
 * Inputs: struct io * bkgio - pointer to the I/O interface of the backing device
 *         struct cache ** cptr - pointer to created cache
 * Outputs: return 0 if succeeds; else, return an error code
 * Description: This function creates and initializes a cache with the passed backing interface and
 *              makes it available through cptr.
 * Side Effects: New memory is allocated for the cache created.
 */
int create_cache(struct io * bkgio, struct cache ** cptr) {
    // check whether the inputs are valid
    if (bkgio == NULL || cptr == NULL) {
        return -EINVAL;
    }

    // allocate memory for the new cache
    struct cache * cache = kmalloc(sizeof(struct cache));
    // if the allocation fails, return an error code
    if (cache == NULL) {
        return -ENOMEM;
    }
    // initialize newly allocated memory to 0
    memset(cache, 0, sizeof(struct cache));

    // pass the backing interface to this cache
    cache->bkgio = bkgio;

    // initialize every cache block in this cache
    for (int i = 0; i < CACHE_CAPACITY; i++) {
        cache->blocks[i].isempty = 1;
        cache->blocks[i].dirty = 0;
        cache->blocks[i].refcnt = 0;
        cache->blocks[i].last_release_time = 0;

        // dynamically allocate data for each cache block
        cache->blocks[i].data = kmalloc(CACHE_BLKSZ);
        // if the allocation fails, free allocated memory
        if (cache->blocks[i].data == NULL) {
            for (int j = 0; j < i; j++) {
                kfree(cache->blocks[j].data);
            }
            kfree(cache);
            return -ENOMEM;
        }
        // initialize newly allocated memory to 0
        memset(cache->blocks[i].data, 0, CACHE_BLKSZ);
    }

    // return this new cache to cptr
    *cptr = cache;

    // return 0 on success
    return 0;
}


/* int cache_get_block(struct cache * cache, unsigned long long pos, void ** pptr)
 * Inputs: struct cache * cache - the cache from which to fetch the block
 *         unsigned long long pos - the block position on the backing I/O endpoint
 *         void ** pptr - pointer to block data
 * Outputs: return 0 if succeeds; else, return an error code
 * Description: This function reads a CACHE_BLKSZ sized block from the backing interface into the cache.
 * Side Effects: None
 */
int cache_get_block(struct cache * cache, unsigned long long pos, void ** pptr) {
    // check whether the inputs are valid
    if (cache == NULL || pptr == NULL || pos % CACHE_BLKSZ != 0) {
        return -EINVAL;
    }

    int i;
    struct cache_block * cblk;

    // first check whether the block exists in the cache
    for (i = 0; i < CACHE_CAPACITY; i++) {
        cblk = &cache->blocks[i];

        // Dirty blocks hold the newest bytes and must also satisfy hits.
        if (cblk->isempty == 0 && cblk->start_pos == pos) {
            cache->stats.hits++;
            // if found, pass the data to pptr
            *pptr = cblk->data;

            // increment the reference count
            cblk->refcnt++;

            return 0;
        }
    }

    cache->stats.misses++;
    // if doesn't exist, read the block from the backing device into an available cache
    // first loop to find an empty block to store data
    int avail_idx = -1;
    for (i = 0; i < CACHE_CAPACITY; i++) {
        cblk = &cache->blocks[i];
        
        // if the block is empty
        if (cblk->isempty == 1) {
            avail_idx = i;
            break;
        }
    }
    if (avail_idx == -1) {
        uint64_t oldest_time = UINT64_MAX;
        // loop to find an non-empty but available block to store data
        for (i = 0; i < CACHE_CAPACITY; i++) {
            cblk = &cache->blocks[i];
            
            // evict the least-recently used (LRU) block
            if (cblk->refcnt == 0 && cblk->dirty == 0) {
                if (cblk->last_release_time < oldest_time) {
                    avail_idx = i;
                    oldest_time = cblk->last_release_time;
                }
            }
        }
    }
    // no block available, return an error code
    if (avail_idx == -1) {
        return -ENOMEM;
    }

    cblk = &cache->blocks[avail_idx];   // the available cache block
    // Read transactionally: even a failing device may overwrite its buffer.
    // Preserve the old victim and metadata unless the whole block was read.
    char data[CACHE_BLKSZ];
    cache->stats.backing_reads++;
    long bytes_read = ioreadat(cache->bkgio, pos, data, CACHE_BLKSZ);
    if (bytes_read != CACHE_BLKSZ)
        return bytes_read < 0 ? (int)bytes_read : -EIO;
    if (!cblk->isempty)
        cache->stats.evictions++;
    memcpy(cblk->data, data, CACHE_BLKSZ);
    // update the information of this specified cache block
    cblk->start_pos = pos;
    cblk->isempty = 0;
    cblk->dirty = 0;
    cblk->refcnt++;

    // pass the data to pptr
    *pptr = cblk->data;

    // return 0 on success
    return 0;
}

/* void cache_release_block(struct cache * cache, void * pblk, int dirty)
 * Inputs: struct cache * cache - the cache containing the block
 *         void * pblk - pointer to the block data returned by cache_get_block()
 *         int dirty - dirty bit that need to be passed to cache_block->dirty
 * Outputs: None
 * Description: This function releases a previously acquired cache block and writes data back to
 *              the disk if it has been modified.
 * Side Effects: None
 */
void cache_release_block(struct cache * cache, void * pblk, int dirty) {
    // check whether the inputs are valid
    if (cache == NULL || pblk == NULL) {
        return;
    }

    // loop to find the specified cache block
    for (int i = 0; i < CACHE_CAPACITY; i++) {
        struct cache_block *cblk = &cache->blocks[i];

        // if found
        if (cblk->data == pblk) {
            // mark it dirty if it has been modified
            if (dirty == 1) {
                cblk->dirty = 1;
            }

            // decrement the reference count
            if (cblk->refcnt > 0) {
                cblk->refcnt--;

                // if no one is holding this cache and it is dirty, write back
                if (cblk->refcnt == 0 && cblk->dirty == 1) {
                    // The void release API cannot return an I/O error. Retain
                    // dirty state; cache_flush explicitly reports/retries it.
                    write_block(cache, cblk);
                }
                
                // indicate that this cache block is no longer in use and can be evicted
                if (cblk->refcnt == 0 && cblk->dirty == 0) {
                    cblk->last_release_time = ++cache->clock;
                }
            }           

            return;
        }
    }
}

/* int cache_flush(struct cache * cache)
 * Inputs: struct cache * cache - the cache to write back
 * Outputs: return 0 if succeeds; else, return an error code
 * Description: This function flushes the cache. Any dirty blocks that have not yet been written
 *              to the backing interface must be written to the backing interface.
 * Side Effects: None
 */
int cache_flush(struct cache * cache) {
    // check whether the input is valid
    if (cache == NULL) {
        return -EINVAL;
    }

    int result = 0;
    // loop to find all cache blocks that need to be written back
    for (int i = 0; i < CACHE_CAPACITY; i++) {
        struct cache_block *cblk = &cache->blocks[i];

        if (!cblk->isempty && cblk->dirty && cblk->refcnt != 0) {
            result = -EBUSY;
            continue;
        }
        // check whether the block is dirty and refcnt equals 0
        if (cblk->isempty == 0 && cblk->dirty == 1 && cblk->refcnt == 0) {
            // write to the backing device
            int ret = write_block(cache, cblk);
            if (ret < 0)
                return ret;
            cblk->last_release_time = ++cache->clock;
        }
    }

    // return 0 on success
    return result;
}

int cache_get_stats(const struct cache * cache, struct cache_stats * stats) {
    if (cache == NULL || stats == NULL) return -EINVAL;
    *stats = cache->stats;
    return 0;
}

int cache_reset_stats(struct cache * cache) {
    if (cache == NULL) return -EINVAL;
    memset(&cache->stats, 0, sizeof(cache->stats));
    return 0;
}

int destroy_cache(struct cache * cache) {
    if (cache == NULL) return -EINVAL;
    for (int i = 0; i < CACHE_CAPACITY; i++)
        if (cache->blocks[i].refcnt != 0) return -EBUSY;
    int ret = cache_flush(cache);
    if (ret < 0) return ret;
    for (int i = 0; i < CACHE_CAPACITY; i++)
        kfree(cache->blocks[i].data);
    kfree(cache);
    return 0;
}
