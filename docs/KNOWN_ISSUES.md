# Remaining known issues and boundaries

The reproducible build, smoke, cache regression suite, core KTFS lifecycle and
benchmark pass. The following limits remain outside this focused upgrade.

| Priority | Issue / limit | Consequence |
|---|---|---|
| P1 | Cache release has a void return type inherited from coursework | A write operation can appear complete while dirty data awaits retry; explicit fsflush/cache_flush is required to check outstanding errors |
| P1 | KTFS metadata updates have no rollback | ENOSPC/I/O failure during create/extension/delete may leave partial allocations or inconsistent metadata |
| P1 | KTFS open error after table insertion does not fully unwind | A later inode-read failure can retain allocations and a table slot; fault injection currently targets cache, not all metadata paths |
| P1 | Full concurrent KTFS behavior is not tested | Existing locking is not a verified transactional/concurrent design; some helper accesses rely on surrounding locks |
| P1 | Delete of an open file and outstanding handles are not covered | Validated deletion closes the file first; do not assume POSIX unlink semantics |
| P1 | Broad memory/process validation is incomplete | Fork, malicious user pointers, malformed ELF, memory exhaustion and isolation are not claimed as verified |
| P2 | Existing heap does not reuse freed allocations | Long-lived repeated allocations may exhaust the small guest; destroy_cache fixes lifecycle ownership but cannot change allocator behavior |
| P2 | Repeated filesystem mount and failed mount cleanup are incomplete | Validated workflow mounts once per fresh QEMU boot |
| P2 | Original halt-failure assembly can fall through to success write | Runner checks explicit failure markers and completion as well as exit code |
| P2 | Legacy user programs and historical test Makefiles are not all runnable | Supported targets use the new source-built smoke and storage suite |
| P2 | Existing pipe error paths, allocation failures and debug prints remain | Pipe behavior is outside this upgrade; only the compile-blocking size definition changed |
| P2 | Fixture generator supports a constrained subset | Flat directory, at most 95 files, 131 blocks per file; not a general replacement mkfs |
| P2 | Double-indirect data paths, directory-capacity boundaries and malformed images not exhaustively tested | Passing small-file tests is not a complete filesystem certification |
| P2 | No crash-consistency/stable-media flush guarantee | Cache flush is not a journal or a hardware durability barrier |
| P3 | Unrelated commented code and duplicate includes remain | Kept to avoid large unrelated changes to course code |

No P0 blocker remains in the documented workflow. CI configuration is provided
but has not been run remotely. The supplied checkout has no Git history; original
personal contribution attribution must come from the project authors.
