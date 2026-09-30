// cache.h - Block cache for a storage device
//
// Copyright (c) 2025 University of Illinois
// SPDX-License-identifier: NCSA
//

#ifndef _CACHE_H_
#define _CACHE_H_

#define CACHE_BLKSZ 512UL 

#define CACHE_CLEAN 0
#define CACHE_DIRTY 1

struct io; // extern decl.
struct cache; // opaque decl.

/* Counts calls to backing I/O, including failed/short transfers. Invalid get
 * arguments do not count as misses; a miss without an evictable slot does.
 * Synchronization and backing endpoint lifetime are the caller's responsibility.
 */
struct cache_stats {
    unsigned long hits;
    unsigned long misses;
    unsigned long evictions;
    unsigned long backing_reads;
    unsigned long backing_writes;
};

extern int create_cache(struct io * bkgio, struct cache ** cptr);
extern int cache_get_block(struct cache * cache, unsigned long long pos, void ** pptr);
extern void cache_release_block(struct cache * cache, void * pblk, int dirty);
extern int cache_flush(struct cache * cache);
extern int cache_get_stats(const struct cache * cache, struct cache_stats * stats);
extern int cache_reset_stats(struct cache * cache);
/* Destroy only when all blocks are released and writeback succeeds. */
extern int destroy_cache(struct cache * cache);

#endif // _CACHE_H_
