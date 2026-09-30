/* Storage regression tests added during the engineering upgrade.
 * KTFS scenarios adapt the original ktfs_test.c create/extend/read/delete flow.
 */
#include "assert.h"
#include "cache.h"
#include "conf.h"
#include "console.h"
#include "device.h"
#include "dev/virtio.h"
#include "error.h"
#include "fs.h"
#include "intr.h"
#include "ioimpl.h"
#include "memory.h"
#include "process.h"
#include "see.h"
#include "string.h"
#include "thread.h"

#define CHECK(expr) do { if (!(expr)) { \
    kprintf("[FAIL] %s line %d: %s\n", __func__, __LINE__, #expr); \
    halt_failure(); for (;;) __asm__ volatile ("wfi"); } } while (0)

#ifndef STORAGE_BENCH
/* Fake endpoint only for precise cache fault injection; KTFS uses real VirtIO. */
static struct {
    struct io io;
    unsigned char data[128 * CACHE_BLKSZ];
    long read_result;
    long write_result;
    unsigned reads;
    unsigned writes;
    unsigned closes;
} mock;

static long mock_read(struct io *io, unsigned long long pos, void *buf, long len) {
    (void)io;
    mock.reads++;
    if (pos > sizeof(mock.data) || len > sizeof(mock.data) - pos) return -EIO;
    if (mock.read_result < 0) {
        memset(buf, 0xEE, len); /* A failing driver is allowed to touch its buffer. */
        return mock.read_result;
    }
    if (len > mock.read_result) len = mock.read_result;
    memcpy(buf, mock.data + pos, len);
    return len;
}

static long mock_write(struct io *io, unsigned long long pos, const void *buf, long len) {
    (void)io;
    mock.writes++;
    if (pos > sizeof(mock.data) || len > sizeof(mock.data) - pos) return -EIO;
    if (mock.write_result < 0) return mock.write_result;
    if (len > mock.write_result) len = mock.write_result;
    memcpy(mock.data + pos, buf, len);
    return len;
}

static int mock_cntl(struct io *io, int cmd, void *arg) {
    (void)io;
    if (cmd == IOCTL_GETBLKSZ) return 1;
    if (cmd == IOCTL_GETEND && arg) {
        *(unsigned long long *)arg = sizeof(mock.data);
        return 0;
    }
    return -ENOTSUP;
}

static void mock_close(struct io *io) { (void)io; mock.closes++; }
static const struct iointf mock_intf = {
    .readat = mock_read, .writeat = mock_write, .cntl = mock_cntl, .close = mock_close
};

static struct cache *active_cache;
static struct cache *fresh_cache(void) {
    struct cache *cache;
    if (active_cache) CHECK(destroy_cache(active_cache) == 0);
    memset(&mock, 0, sizeof(mock));
    ioinit1(&mock.io, &mock_intf);
    mock.read_result = mock.write_result = CACHE_BLKSZ;
    for (unsigned i = 0; i < sizeof(mock.data); i++) mock.data[i] = i / CACHE_BLKSZ;
    CHECK(create_cache(&mock.io, &cache) == 0);
    active_cache = cache;
    return cache;
}

static void touch(struct cache *cache, unsigned block) {
    void *p;
    CHECK(cache_get_block(cache, block * CACHE_BLKSZ, &p) == 0);
    CHECK(*(unsigned char *)p == block);
    cache_release_block(cache, p, CACHE_CLEAN);
}

static void cache_hit_miss(void) {
    struct cache *c = fresh_cache();
    touch(c, 2); touch(c, 2);
    CHECK(mock.reads == 1);
}

static void cache_invalid(void) {
    struct cache *c = fresh_cache(); void *p;
    CHECK(create_cache(NULL, &c) == -EINVAL);
    CHECK(create_cache(&mock.io, NULL) == -EINVAL);
    CHECK(cache_get_block(NULL, 0, &p) == -EINVAL);
    CHECK(cache_get_block(c, 1, &p) == -EINVAL);
    CHECK(cache_get_block(c, 0, NULL) == -EINVAL);
    CHECK(cache_flush(NULL) == -EINVAL);
    cache_release_block(c, NULL, 0);
    CHECK(mock.reads == 0 && mock.writes == 0);
}

static void cache_shared_dirty(void) {
    struct cache *c = fresh_cache(); void *a, *b;
    CHECK(cache_get_block(c, 0, &a) == 0);
    CHECK(cache_get_block(c, 0, &b) == 0 && a == b);
    *(char *)a = 42;
    cache_release_block(c, a, CACHE_DIRTY);
    CHECK(mock.writes == 0);
    CHECK(cache_get_block(c, 0, &a) == 0 && a == b && *(char *)a == 42);
    CHECK(mock.reads == 1);
    cache_release_block(c, a, CACHE_CLEAN);
    cache_release_block(c, b, CACHE_CLEAN);
    CHECK(mock.writes == 1 && mock.data[0] == 42);
}

static void cache_pinned_full(void) {
    struct cache *c = fresh_cache(); void *p[CACHE_CAPACITY], *q;
    for (unsigned i = 0; i < CACHE_CAPACITY; i++)
        CHECK(cache_get_block(c, i * CACHE_BLKSZ, &p[i]) == 0);
    CHECK(cache_get_block(c, CACHE_CAPACITY * CACHE_BLKSZ, &q) == -ENOMEM);
    cache_release_block(c, p[7], CACHE_CLEAN);
    CHECK(cache_get_block(c, CACHE_CAPACITY * CACHE_BLKSZ, &q) == 0);
    CHECK(q == p[7]);
    cache_release_block(c, q, CACHE_CLEAN);
    for (unsigned i = 0; i < CACHE_CAPACITY; i++)
        if (i != 7) cache_release_block(c, p[i], CACHE_CLEAN);
}

static void cache_lru(void) {
    struct cache *c = fresh_cache();
    for (unsigned i = 0; i < CACHE_CAPACITY; i++) touch(c, i);
    touch(c, 0); touch(c, CACHE_CAPACITY);
    unsigned before = mock.reads;
    touch(c, 0); CHECK(mock.reads == before);
    touch(c, 1); CHECK(mock.reads == before + 1);
    struct cache_stats stats;
    CHECK(cache_get_stats(c, &stats) == 0);
    CHECK(stats.evictions == 2 && stats.backing_reads == CACHE_CAPACITY + 2);
}

static void cache_read_failure(void) {
    struct cache *c = fresh_cache(); void *p;
    for (unsigned i = 0; i < CACHE_CAPACITY; i++) touch(c, i);
    mock.read_result = -EIO;
    CHECK(cache_get_block(c, CACHE_CAPACITY * CACHE_BLKSZ, &p) == -EIO);
    mock.read_result = CACHE_BLKSZ;
    unsigned before = mock.reads;
    touch(c, 0); CHECK(mock.reads == before);
}

static void cache_short_read(void) {
    struct cache *c = fresh_cache(); void *p;
    mock.read_result = 17;
    CHECK(cache_get_block(c, 0, &p) == -EIO);
    mock.read_result = CACHE_BLKSZ;
    touch(c, 0); CHECK(mock.reads == 2);
    for (unsigned i = 1; i < CACHE_CAPACITY; i++) touch(c, i);
    mock.read_result = 17;
    CHECK(cache_get_block(c, CACHE_CAPACITY * CACHE_BLKSZ, &p) == -EIO);
    mock.read_result = CACHE_BLKSZ;
    unsigned before = mock.reads;
    touch(c, 0); CHECK(mock.reads == before);
}

static void cache_short_write(void) {
    struct cache *c = fresh_cache(); void *p;
    CHECK(cache_get_block(c, 0, &p) == 0);
    memset(p, 42, CACHE_BLKSZ);
    mock.write_result = 17;
    cache_release_block(c, p, CACHE_DIRTY);
    CHECK(cache_flush(c) == -EIO);
    mock.write_result = CACHE_BLKSZ;
    CHECK(cache_flush(c) == 0);
    CHECK(mock.writes == 3 && mock.data[CACHE_BLKSZ - 1] == 42);
    struct cache_stats stats;
    CHECK(cache_get_stats(c, &stats) == 0 && stats.backing_writes == 3);
}

static void cache_flush_retry(void) {
    struct cache *c = fresh_cache(); void *p, *q;
    CHECK(cache_get_block(c, 0, &p) == 0);
    *(char *)p = 42;
    mock.write_result = -EIO;
    cache_release_block(c, p, CACHE_DIRTY);
    CHECK(cache_flush(c) == -EIO);
    CHECK(cache_get_block(c, 0, &q) == 0 && q == p && *(char *)q == 42);
    mock.write_result = CACHE_BLKSZ;
    cache_release_block(c, q, CACHE_CLEAN);
    CHECK(cache_flush(c) == 0 && mock.data[0] == 42);
}

static void cache_pinned_flush(void) {
    struct cache *c = fresh_cache(); void *p, *q;
    CHECK(cache_get_block(c, 0, &p) == 0);
    CHECK(cache_get_block(c, 0, &q) == 0);
    cache_release_block(c, p, CACHE_DIRTY);
    CHECK(cache_flush(c) == -EBUSY);
    CHECK(mock.writes == 0);
    cache_release_block(c, q, CACHE_CLEAN);
    CHECK(cache_flush(c) == 0 && mock.writes == 1);
}

static void cache_stats_lifetime(void) {
    struct cache *c = fresh_cache();
    struct cache_stats stats;
    void *p;
    touch(c, 0); touch(c, 0);
    CHECK(cache_get_stats(c, &stats) == 0);
    CHECK(stats.hits == 1 && stats.misses == 1 && stats.backing_reads == 1);
    CHECK(stats.evictions == 0 && stats.backing_writes == 0);
    CHECK(cache_reset_stats(c) == 0);
    CHECK(cache_get_stats(c, &stats) == 0);
    CHECK(stats.hits == 0 && stats.misses == 0 && stats.backing_reads == 0);
    CHECK(stats.evictions == 0 && stats.backing_writes == 0);
    CHECK(cache_get_stats(NULL, &stats) == -EINVAL);
    CHECK(cache_get_stats(c, NULL) == -EINVAL);
    CHECK(cache_reset_stats(NULL) == -EINVAL);
    CHECK(destroy_cache(NULL) == -EINVAL);
    CHECK(cache_get_block(c, 0, &p) == 0);
    CHECK(cache_get_stats(c, &stats) == 0 && stats.hits == 1 && stats.misses == 0);
    struct cache *other;
    CHECK(create_cache(&mock.io, &other) == 0);
    touch(other, 0);
    CHECK(cache_get_stats(other, &stats) == 0 && stats.hits == 0 && stats.misses == 1);
    CHECK(cache_get_stats(c, &stats) == 0 && stats.hits == 1 && stats.misses == 0);
    CHECK(destroy_cache(other) == 0);
    CHECK(destroy_cache(c) == -EBUSY);
    mock.write_result = -EIO;
    cache_release_block(c, p, CACHE_DIRTY);
    CHECK(destroy_cache(c) == -EIO);
    mock.write_result = CACHE_BLKSZ;
    CHECK(destroy_cache(c) == 0);
    active_cache = NULL;
}

static void io_reference_ownership(void) {
    struct cache *c = fresh_cache();
    CHECK(destroy_cache(c) == 0);
    active_cache = NULL;
    struct io *wrapper = create_seekable_io(&mock.io);
    CHECK(wrapper != NULL && iorefcnt(&mock.io) == 2);
    ioclose(wrapper);
    CHECK(iorefcnt(&mock.io) == 1 && mock.closes == 0);
    ioclose(&mock.io);
    CHECK(mock.closes == 1);
}

static struct io *file;
static unsigned char payload[1600], readback[1600];

static void ktfs_create(void) {
    CHECK(fscreate("") == -EINVAL);
    CHECK(fscreate("012345678901234") == -EINVAL);
    CHECK(fscreate("lab-test") == 0);
}

static void ktfs_duplicate_open(void) {
    struct io *other;
    CHECK(fscreate("lab-test") < 0);
    CHECK(fsopen("missing", &other) == -ENOENT);
    CHECK(fsopen("lab-test", &file) == 0);
    CHECK(fsopen("lab-test", &other) == -EBUSY);
}

static void ktfs_extend(void) {
    unsigned long long end = 0;
    CHECK(ioctl(file, IOCTL_GETEND, &end) == 0 && end == 0);
    end = 2048;
    CHECK(ioctl(file, IOCTL_SETEND, &end) == 0);
    end = 0;
    CHECK(ioctl(file, IOCTL_GETEND, &end) == 0 && end == 2048);
    end = 2047;
    CHECK(ioctl(file, IOCTL_SETEND, &end) == -ENOTSUP);
}

static void ktfs_cross_block(void) {
    for (unsigned i = 0; i < sizeof(payload); i++) payload[i] = (i * 7 + 3) % 251;
    CHECK(iowriteat(file, 400, payload, sizeof(payload)) == sizeof(payload));
    CHECK(ioreadat(file, 400, readback, sizeof(readback)) == sizeof(readback));
    CHECK(memcmp(payload, readback, sizeof(payload)) == 0);
}

static void ktfs_eof_zero(void) {
    CHECK(iowriteat(file, 2044, payload, 10) == 4);
    CHECK(ioreadat(file, 2044, readback, 10) == 4);
    CHECK(memcmp(payload, readback, 4) == 0);
    CHECK(ioreadat(file, 2049, readback, 10) == 0);
    CHECK(iowriteat(file, 2049, payload, 10) == 0);
    CHECK(ioreadat(file, 0, readback, 0) == 0);
    CHECK(iowriteat(file, 0, payload, 0) == 0);
    CHECK(ioreadat(file, 0, readback, -1) == -EINVAL);
}

static void ktfs_reopen(void) {
    CHECK(fsflush() == 0);
    ioclose(file);
    CHECK(fsopen("lab-test", &file) == 0);
    CHECK(ioreadat(file, 400, readback, sizeof(readback)) == sizeof(readback));
    CHECK(memcmp(payload, readback, sizeof(payload)) == 0);
    ioclose(file);
}

static void ktfs_delete(void) {
    CHECK(fsdelete("lab-test") == 0);
    CHECK(fsdelete("lab-test") == -ENOENT);
    CHECK(fsopen("lab-test", &file) == -ENOENT);
}

static void ktfs_empty_recreate(void) {
    for (int i = 0; i < 4; i++) {
        CHECK(fscreate("again") == 0);
        CHECK(fsopen("again", &file) == 0);
        ioclose(file);
        CHECK(fsdelete("again") == 0);
    }
    CHECK(fsflush() == 0);
}

#define RUN(name) do { name(); kprintf("[PASS] " #name "\n"); count++; } while (0)
#else
static void benchmark(struct io *disk, const char *name, int workload) {
    struct cache *cache;
    struct cache_stats stats;
    unsigned state = 0x391;
    CHECK(create_cache(disk, &cache) == 0);
    for (unsigned i = 0; i < 1024; i++) {
        unsigned block;
        if (workload == 0) block = i % 128;
        else if (workload == 1) block = i % 8;
        else {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            block = state % 128;
        }
        void *p;
        CHECK(cache_get_block(cache, block * CACHE_BLKSZ, &p) == 0);
        cache_release_block(cache, p, CACHE_CLEAN);
    }
    CHECK(cache_get_stats(cache, &stats) == 0);
    CHECK(stats.hits + stats.misses == 1024);
    kprintf("BENCH %s 1024 %lu %lu %lu %lu %lu\n", name, stats.hits,
        stats.misses, stats.evictions, stats.backing_reads, stats.backing_writes);
    CHECK(destroy_cache(cache) == 0);
}
#endif

void main(void) {
    console_init(); devmgr_init(); intrmgr_init(); thrmgr_init();
    memory_init(); procmgr_init();
    for (int i = 0; i < 8; i++)
        virtio_attach((void *)VIRTIO_MMIO_BASE(i), VIRTIO0_INTR_SRCNO + i);
    enable_interrupts();
    struct io *disk;
    CHECK(open_device("vioblk", 0, &disk) == 0);
    CHECK(fsmount(disk) == 0);
    kprintf("RISC-V Storage Systems Lab\n");
#ifndef STORAGE_BENCH
    int count = 0;
    RUN(cache_hit_miss); RUN(cache_invalid); RUN(cache_shared_dirty);
    RUN(cache_pinned_full); RUN(cache_lru); RUN(cache_read_failure);
    RUN(cache_short_read); RUN(cache_short_write); RUN(cache_flush_retry);
    RUN(cache_pinned_flush); RUN(cache_stats_lifetime); RUN(io_reference_ownership);
    RUN(ktfs_create); RUN(ktfs_duplicate_open); RUN(ktfs_extend);
    RUN(ktfs_cross_block); RUN(ktfs_eof_zero); RUN(ktfs_reopen);
    RUN(ktfs_delete); RUN(ktfs_empty_recreate);
    kprintf("[DONE] test %d\n", count);
#else
    benchmark(disk, "sequential", 0);
    benchmark(disk, "hot_set", 1);
    benchmark(disk, "random", 2);
    kprintf("[DONE] benchmark 3\n");
#endif
    halt_success();
}
