# Engineering change summary

The original course OS and storage architecture are preserved. Changes fall
into the following reviewable groups. The supplied directory had no repository
history; the GitHub repository starts with the validated engineering revision,
not a reconstructed history of individual course contributions.

1. **Baseline and reproducibility:** `docs/AUDIT.md`, root `Makefile`, out-of-tree
   targets in the existing Makefiles, source fixture builder, smoke program.
   Fixed missing freestanding pipe capacity and removed unsupported/duplicate
   UART startup from the active main path. The baseline remains documented as
   failed rather than retroactively reported as working.
2. **Cache correctness and measurement:** `cache.c/.h` dirty hits, exact block
   transfer validation, safe replacement on failed reads, busy flush, per-object
   counters, and explicit destruction. Array layout and replacement/writeback
   policy remain recognizable. No background writer or complex cache framework.
3. **Storage ownership and creation:** `io.c` seek close releases one reference;
   `ktfs.c` transfers the creator reference to the wrapper and initializes
   empty-directory create state before branching.
4. **Verification:** `src/sys/test/storage_test.c` adapts course KTFS scenarios
   and adds precise cache fault injection, 20 cases total, plus three real
   VirtIO-backed read benchmarks. `scripts/run_tests.py` builds, bounds QEMU,
   parses strict markers and saves logs/JSON. Host checks test rejection paths.
5. **Presentation:** README, design/build/testing/benchmark/known-issue/resume
   documents and preserved execution evidence. Optional CI reuses local commands.

Regression mapping: compiler smoke covers the pipe-size/startup changes;
`cache_shared_dirty`, `cache_read_failure`, `cache_short_read`,
`cache_short_write`, `cache_flush_retry`, `cache_pinned_flush` cover cache fixes;
`cache_stats_lifetime` and `cache_lru` cover new APIs/counters;
`io_reference_ownership` and `ktfs_reopen` cover ownership; `ktfs_create` and
`ktfs_empty_recreate` cover empty-root creation.

No unmeasured performance gains, new process subsystem, new filesystem, or
production-level durability are claimed. Existing copyright notices remain.
