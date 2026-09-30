# Validation and test map

The main suite consists of 20 named cases, each containing multiple assertions.
These are case counts, not assertion counts. Failure invalidates the run, and the host
requires the exact list plus its completion marker; it does not count arbitrary
PASS text as success.

| Case | Behavior exercised |
|---|---|
| cache_hit_miss | Cold read then cached read; backing read count |
| cache_invalid | Null arguments, unaligned position, no backing I/O on rejection |
| cache_shared_dirty | Multiple holders; read newest dirty bytes; write only on final release |
| cache_pinned_full | All slots pinned; refusal to evict; release one slot and reuse it |
| cache_lru | Release-based victim selection and actual eviction counters |
| cache_read_failure | Failed driver read touches destination; old victim survives |
| cache_short_read | Short read rejected for empty and occupied replacement slots |
| cache_short_write | Dirty retained on partial write; flush failure followed by full retry |
| cache_flush_retry | Negative write error, dirty-hit consistency, later retry |
| cache_pinned_flush | Busy result for referenced dirty block; final release flushes it |
| cache_stats_lifetime | Counter reset preserves contents; independent cache stats; destroy busy/error/success |
| io_reference_ownership | Wrapper close releases one reference without closing another owner's endpoint |
| ktfs_create | Empty-root creation; invalid names |
| ktfs_duplicate_open | Duplicate create, missing file, open, already-open rejection |
| ktfs_extend | SETEND to 2 KiB across direct/indirect boundary; shrinking rejected |
| ktfs_cross_block | 1,600 patterned bytes from offset 400, checked byte-for-byte |
| ktfs_eof_zero | EOF clipping, beyond-EOF, zero-length and negative-length I/O |
| ktfs_reopen | Explicit flush; close, reopen, verify contents, close |
| ktfs_delete | Delete, repeated delete, opening deleted file |
| ktfs_empty_recreate | Four create/open/close/delete cycles from an empty directory |

KTFS cases intentionally form one lifecycle on a fresh image and adapt the
existing `ktfs_test.c` scenarios. They are deterministic but not independently
selectable. They use the real VirtIO driver and filesystem. Cache cases use a
controlled fake backing endpoint to test exact failure conditions and counters.

The separately source-built user smoke validates ELF load, user-mode entry,
the print syscall and exit. It does not test fork, arbitrary exec replacement,
malformed ELF rejection or address-space isolation.

Three host test methods exercise complete output acceptance, rejection of
partial/duplicate/failing/nonzero-exit output, and benchmark-counter consistency.
An actual QEMU invocation with a 0.001-second timeout was also observed to fail
with runner status 1. The original halt shim can report an unhelpful QEMU exit
status on kernel failure, so parsing explicit failure/panic and completion
markers is essential even when QEMU returns zero.

## Reproduced failures during development

The audit preceded implementation. The first new runtime suite failed at
`cache_shared_dirty` with the original cache. After the cache fixes it reached
and failed `io_reference_ownership`; after the reference fix it reached and
failed empty-root `ktfs_create`. After those minimal fixes all 20 cases passed.
Logs are preserved in `docs/evidence/`.

The other cache fixes have targeted short-transfer, victim-preservation and
flush tests in the same suite. A passing test is evidence for that scenario,
not proof of full filesystem correctness or exhaustive fault coverage.
