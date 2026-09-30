# Storage and cache decisions

## Scope

Preserve the course KTFS → cache → I/O → VirtIO path. No new filesystem format,
replacement-policy experiment, background writer, networking or transactions
were introduced. Tests and measurement are the main new capabilities.

## Cache contract

- `create_cache(backing, &cache)` borrows the backing endpoint. The caller keeps
  it alive until destruction and serializes cache operations.
- `cache_get_block(cache, pos, &ptr)` requires a 512-byte-aligned position. A
  successful get must have exactly one matching release. The returned buffer
  remains pinned until released. Do not use it after the last release.
- Both clean and dirty entries satisfy hits. Dirty data may be newer than disk;
  excluding it causes stale reads and duplicate entries for one disk position.
- Empty slots are preferred. Otherwise choose a clean, unreferenced entry by
  oldest final-release timestamp. Full/pinned/dirty-only caches return ENOMEM;
  dirty entries must be flushed or released before they can be replaced.
- A miss reads into a 512-byte temporary buffer. Only a complete read updates
  the victim bytes and metadata. Negative results propagate; short reads are
  EIO. Failed replacement does not count as an eviction.
- The release policy is unchanged: write dirty data when refcount reaches zero.
  A negative or short write leaves dirty set. Because release returns void,
  callers must use `cache_flush` to observe/retry an unresolved I/O failure.
- Flush returns EBUSY if any dirty entry is still referenced. It may flush other
  eligible entries before returning EBUSY; it does not promise an atomic flush.
- `destroy_cache` refuses live references and unresolved writeback errors,
  otherwise frees its allocations. It does not close the borrowed endpoint.
  The inherited heap marks frees but does not reuse their memory; destruction
  expresses correct ownership, not a new reclaiming allocator.

The monotonic release timestamp now belongs to each cache rather than a global.
Resetting statistics does not reset data, timestamps or eviction order.
There is no internal cache lock: KTFS supplies its existing locks and standalone
tests are single-threaded. This revision does not claim to repair all filesystem
concurrency paths.

## Counter definitions

| Counter | Meaning |
|---|---|
| hits | Valid get found an existing clean or dirty entry |
| misses | Valid get did not find an entry, including a full-cache failure |
| evictions | Successfully replaced a previously valid entry |
| backing_reads | Calls to `ioreadat`, including errors and short reads |
| backing_writes | Calls to `iowriteat`, including errors, short writes and retries |

`cache_get_stats` copies a snapshot; `cache_reset_stats` clears only counters.
Invalid API arguments are excluded from access counters. Backing counts are
endpoint calls, not necessarily physical hardware transfers or durable-media
flushes. Counters use native unsigned long (64-bit for this LP64 build).

## KTFS and I/O fixes

The seek wrapper uses `ioaddref` when constructed, and now releases exactly that
one reference when closed. KTFS transfers its creator reference after wrapping.
Tests check an independently held backing endpoint stays alive and a KTFS file
can be closed/reopened, then deleted.

Empty-root creation previously jumped over initialization of directory block
counts and the data-region offset. The jump now happens after those values are
initialized. Fresh empty-image tests and repeated create/delete cycles cover it.

KTFS retains its original semantics: positional I/O clips at EOF, explicit
IOCTL_SETEND extends files, shrinking is unsupported, and opening an already
open file returns EBUSY. Tests do not impose POSIX behavior. Sequential seek-I/O
writes have their own existing extension behavior, which is not the focus of
this suite.

## Deliberate limits

`fsflush()` flushes this software cache. The existing VirtIO driver does not
provide a durability barrier to stable physical storage. No journal, rollback,
crash-consistency or power-failure guarantee is implied. See KNOWN_ISSUES.md.
