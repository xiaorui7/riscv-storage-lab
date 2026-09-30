# Repository audit — RISC-V Storage Systems Lab

Audit recorded before implementation changes. Original sources were copied to
`/tmp/original-src` in an isolated Ubuntu 24.04 development container. This
checkout arrived without `.git`, README, CI, or root build targets. All paths
below refer to the original repository unless explicitly marked as new work.

## A. Build environment

- Host: Windows with Docker Desktop / Linux x86-64; no native GNU Make,
  RISC-V compiler, or QEMU on PATH. The development container supplies Linux;
  containers are not an OS feature or a resume claim.
- Actually installed and exercised: Ubuntu 24.04 packages
  `make`, `gcc-riscv64-unknown-elf`, `binutils-riscv64-unknown-elf`,
  `qemu-system-misc`, `python3`. GCC reports 13.2.0; QEMU reports 8.2.2.
  These are tested versions, not an inferred minimum-version requirement.
- Kernel: `make -C src/sys`; prefix `riscv64-unknown-elf-`,
  `rv64imazicsr`, LP64, freestanding, linker script `kernel.ld`, 8 MiB RAM.
- Users: `make -C src/usr`; default builds only `hello`, originally
  `rv64g`/LP64D. Assembly implicit rules do not consistently receive ABI flags.
- Original run commands assume POSIX shell, `/dev/urandom`, PTYs, and a
  writable `src/sys/ktfs.raw` (8 MiB). A clean snapshot must be used for tests.
- `src/util/fs/mkfs_ktfs`, `mkfs_ktfs_inorder`, `unmkfs_ktfs` are precompiled
  Linux x86-64 ELF programs, with no accompanying source. Actual mkfs usage:
  `mkfs_ktfs IMAGE SIZE INODE_COUNT FILE...` (SIZE accepts K/M/G).
- `shell.elf` exists as a supplied user binary, but no shell source/build
  target was found. Default `hello.c` expects a kernel I/O pointer even though
  the user build selects user mode. Supplied binaries are not proof of a
  source-reproducible user environment.
- Baseline compilation was forced in the temporary copy with `make -B`.
  The user build's `mkdir bin` fails under `-B` when `bin` already exists;
  rerunning normal `make` linked the freshly rebuilt user objects.
- Preserved logs from actual commands: `docs/evidence/baseline-*.log`.

## B. Existing functionality

PARTIAL means source exists or a prerequisite worked; it is not a passing
functional test. No original runtime tests are credited as passed.

| Component | Relevant files | Status | Evidence |
|---|---|---|---|
| Kernel build | sys/Makefile, io.c | FAIL | `PIPE_BUF` undeclared in pipe_read/pipe_write |
| User build | usr/Makefile | PARTIAL | Fresh objects linked hello on retry; forced build failed on existing bin directory |
| QEMU boot | main.c, uart.c | FAIL | With diagnostic `CPPFLAGS=-DPIPE_BUF=4096`, kernel starts memory initialization then store access fault at MMIO 0x10000101 |
| KTFS mount | ktfs.c, ktfs_test.c | PARTIAL | Original test kernel builds with workaround but hits same UART fault before mounting |
| File create/delete | ktfs.c | PARTIAL | Runtime not reached; empty-directory path skips initialization of locals |
| File read/write | ktfs.c | PARTIAL | Runtime not reached; explicit extension via SETEND, positional writes clip at EOF |
| Block cache | cache.c | PARTIAL | No standalone automated regression suite; risks below found by inspection |
| fork | process.c, memory.c, usr/fork*.c | PARTIAL | Programs present, not executed successfully in baseline |
| exec | process.c, elf.c | PARTIAL | Default shell entry not reached |
| Virtual memory | memory.c | PARTIAL | Initialization prints under workaround; isolation/fault behavior not validated |

Boot command used QEMU `virt`, `-bios none -nographic -monitor none -m 8M`,
modern VirtIO MMIO, a snapshot of the original image and `virtio-blk-device`.
Both original normal and KTFS-test kernels faulted at the absent UART address.

## C. Code ownership boundary

- **Existing repository code:** all original `src/` files, including cache,
  filesystem, process/memory implementations, drivers, tests and binaries.
  No reliable personal/team authorship attribution is possible without history.
- **Clearly course/framework-associated:** many files retain University of
  Illinois 2024–2025 copyright / NCSA notices, plus teaching scaffolding.
  Notices will remain. A notice alone does not identify who wrote every body.
- **New engineering work:** audit/build documentation, reproducible workflows,
  focused regression fixes, instrumentation, test runner, benchmark and CI
  added during this upgrade. Existing OS features must not be presented as
  newly implemented or attributed to one student.

## D. Bugs and risks

| Priority | Finding | Evidence / scope |
|---|---|---|
| P0 | Undefined PIPE_BUF blocks clean kernel build | Reproduced with GCC 13.2 |
| P0 | Unavailable UART MMIO prevents boot | Reproduced store fault at 0x10000101; main also attaches UART0 twice |
| P1 | Dirty cache blocks excluded from lookup | Can produce stale reads/duplicate cache entries while referenced or after failed writes |
| P1 | Cache accepts short reads/writes as full success | Can expose incomplete data or clear dirty state prematurely |
| P1 | Replacement read overwrites victim before success | Failed/partial I/O can corrupt the old cached entry |
| P1 | Flush reports success despite referenced dirty entries | Existing loop simply skips them |
| P1 | Empty directory create skips local initialization | `goto create` crosses declarations of last_block_offset and other state |
| P1 | Seek wrapper decrements backing refcount twice | `seekio_close` manually decrements then calls ioclose; ktfs_open currently compensates by leaking a reference |
| P1 | KTFS open error paths retain published object | Cache-read error after insertion does not unwind allocations/table slot |
| P1 | Allocation failures incompletely handled | I/O constructors dereference allocation results; broader kernel paths also need future review |
| P1 | Metadata updates are not failure-atomic | Extension/create can allocate blocks before later failure; no rollback/crash recovery |
| P2 | Original tests mismatch I/O semantics | iotest expects 512-byte memio blocks and ENOTSUP, implementation returns 1 and EINVAL; contains source/destination length mistakes |
| P2 | No bounded PASS/FAIL runner | Existing tests rely on asserts/prints; returning from kernel main signals failure |
| P2 | Existing images/binaries hide build dependencies | No source image-generation workflow; shell binary has no source |
| P2 | Cache lifetime and synchronization not explicit | No destruction API; caller locking required and some KTFS helpers access cache outside outer lock |
| P2 | Pipe/process error paths and debug prints | Outside storage focus; preserve and document unvalidated behavior |
| P3 | Duplicate includes, old commented code, old ELF copy | Keep unrelated cleanup out of this upgrade |

This is a full-repository inventory and targeted code audit, not a claim that
every instruction or error path has been verified. Later validation and remaining
limits will be reported in BUILD, BENCHMARK and RESUME_NOTES rather than changing
these baseline results into retroactive passes.

Final acceptance: a clean `build/` rebuild, user smoke, all 20 storage cases,
three host runner-check methods and all three benchmark workloads passed on
the tested toolchain. See `docs/evidence/clean-build.log`, `test.log`,
`runner-checks.log`, `smoke.log` and `benchmark.json`. This does not upgrade the
original baseline table or claim fork/VM isolation were fully validated.
