# Resume notes

## Existing course-system functionality

Validated in the supported workflow: RISC-V kernel build and QEMU boot; memory
initialization needed for boot/user execution; existing ELF loader and user-mode
entry; print/exit syscall path; VirtIO block I/O; KTFS mount, create, open,
explicit extension, positional reads/writes, close/reopen and closed-file delete;
the original 64-entry cache's basic hit/eviction/writeback behavior after fixes.

These are existing repository capabilities, not newly authored OS features.
Fork and complete virtual-memory isolation are not validated by this revision.
The provided files do not establish which original code belongs to which team
member. Preserve the course/group attribution and describe your personal role
accurately, including any assisted implementation, when asked.

## New engineering work

- Reproduced clean-build and QEMU startup failures and documented the baseline.
- Restored a source-reproducible Make workflow and generated disk fixtures.
- Fixed dirty-block lookup, incomplete I/O handling and failed replacement
  corruption, plus seek-reference ownership and empty-directory creation.
- Added per-cache statistics and lifecycle handling with regression tests.
- Added deterministic QEMU tests, bounded orchestration, logs, JSON results and
  reproducible cache workloads on real VirtIO-backed I/O.
- Documented ownership, design tradeoffs and remaining limits; supplied optional
  CI configuration (remote execution not yet verified).

Possible resume wording, subject to your actual participation and understanding:

> Extended an ECE391 RISC-V teaching OS in C with storage correctness fixes,
> per-cache instrumentation and 20 automated QEMU regression cases covering
> eviction, reference ownership, I/O failures and KTFS file operations.

> Built a reproducible kernel/user build, generated disk fixtures and a Python
> QEMU runner with timeout and failure detection; measured cache behavior across
> sequential, hot-set and seeded-random read workloads.

For an interview, be able to explain why dirty data must satisfy hits, why a
failed read must not overwrite a valid victim, who owns each I/O reference, and
why cache hit rate is not the same as measured application speedup.

## Measured results

- 20 passing named QEMU storage cases; one separate user-mode smoke case.
- Three passing host runner-check methods, including negative-output scenarios.
- An actual 0.001-second QEMU timeout returned runner status 1.
- Each benchmark workload performed 1,024 reads with a fresh 64-block cache.
- Sequential: 0 hits, 1,024 misses, 960 evictions, 1,024 backing reads, 0 writes.
- Hot set: 1,016 hits, 8 misses, 0 evictions, 8 backing reads, 0 writes (99.22%).
- Seeded random: 497 hits, 527 misses, 463 evictions, 527 backing reads,
  0 writes (48.54%).

Environment: GCC 13.2.0, QEMU 8.2.2, Ubuntu 24.04, 8 MiB guest RAM. Raw results
are in `docs/evidence/`; methodology is in BENCHMARK.md. There is no before/after
speedup measurement, production throughput claim, or fabricated metric.
