# RISC-V Storage Systems Lab

A C / RISC-V teaching operating system running under QEMU, extended with a
focus on storage correctness, reproducible builds, automated regression tests,
and measurable block-cache behavior. Based on an existing ECE391 group project.

The original repository supplied the kernel, KTFS filesystem, cache, device
drivers, process management and virtual memory. This revision audits and tests
that foundation and adds focused engineering improvements; it does not claim
to have newly implemented the entire OS.

## What works in the validated workflow

- Source-built kernel and user-mode smoke program, with a generated KTFS image.
- **20 passing QEMU storage regression cases**, including injected cache I/O
  failures, pinned blocks, eviction, and KTFS cross-block file operations.
- Per-cache hit/miss/eviction/backing-I/O counters with reset/read APIs.
- Three reproducible read workloads measured through cache → I/O → VirtIO.
- Bounded Python test runner that rejects missing, duplicate or failing markers,
  panics, timeouts and unsuccessful QEMU exits.

See [measured evidence](docs/evidence/) and [ownership / resume notes](docs/RESUME_NOTES.md).

## Build and run

On Ubuntu 24.04 / an Ubuntu WSL environment:

```sh
sudo apt-get update
sudo apt-get install -y make gcc-riscv64-unknown-elf binutils-riscv64-unknown-elf qemu-system-misc python3
make build
make run
make test
make benchmark
```

`make run` mounts the generated image, loads a freshly compiled user program,
prints a success marker through a syscall, and exits. It is a small repeatable
demo, not an interactive shell. [BUILD.md](docs/BUILD.md) covers Windows,
debugging, direct commands, generated artifacts and cleanup.

## Architecture

```text
User program → syscall → KTFS → block cache → I/O abstraction
                                                 ↓
                                        VirtIO block driver
                                                 ↓
                                         QEMU disk image
```

Kernel storage tests invoke KTFS directly. Isolated cache cases substitute a
controlled I/O endpoint to inject errors. The benchmark uses the real VirtIO
driver and its own cold cache, without going through KTFS file lookup.

## Storage and cache design

The existing cache holds 64 blocks of 512 bytes (32 KiB of data). An acquired
block's reference count prevents eviction while a caller uses it. Modified
blocks are marked dirty; the existing policy writes them when the last
reference is released. Eviction chooses the clean, unreferenced block with the
oldest final-release timestamp, an LRU-like policy.

The upgrade preserves that policy. Dirty blocks now satisfy reads, partial
transfers are errors, and a failed replacement read cannot corrupt the victim.
Failed writes keep data dirty for an explicit `cache_flush()` retry. Flush
reports busy if dirty blocks are still referenced. Counters belong to each
cache; callers provide synchronization. See [DESIGN.md](docs/DESIGN.md).

## Tests and benchmark

```sh
make test                         # 20 kernel storage cases
make run                          # 1 user-mode smoke case
python3 scripts/test_runner.py    # 3 host runner checks
make benchmark                    # 3 read workloads
```

Logs and machine-readable results are saved in `build/`. Tests use disposable
images and do not modify the supplied coursework disk. The original tests remain
available for reference; the new KTFS tests adapt their create/extend/read/delete
scenarios using the actual filesystem semantics.

Recorded with GCC 13.2.0, QEMU 8.2.2, one virtual CPU and 8 MiB guest RAM:

| Workload | Reads | Hits | Misses | Hit rate | Backing reads |
|---|---:|---:|---:|---:|---:|
| Sequential, 128 blocks | 1,024 | 0 | 1,024 | 0.00% | 1,024 |
| Hot set, 8 blocks | 1,024 | 1,016 | 8 | 99.22% | 8 |
| Seeded random, 128 blocks | 1,024 | 497 | 527 | 48.54% | 527 |

These are observed workload results, not improvements against a previous
implementation. [BENCHMARK.md](docs/BENCHMARK.md) explains how to reproduce and
interpret them.

## Engineering improvements

- Audited the original repository and reproduced build and boot failures.
- Fixed the freestanding pipe-size definition and unsupported UART startup.
- Added source-generated demo images, an out-of-source build and bounded runner.
- Fixed cache consistency, short-transfer handling and failed victim replacement.
- Fixed seek-wrapper reference ownership and creation in an empty KTFS directory.
- Added focused tests, cache instrumentation, deterministic workloads and docs.
- Added a GitHub Actions workflow using the same local commands; the first
  [remote build, tests and benchmark passed](https://github.com/xiaorui7/riscv-storage-lab/actions/runs/36779180642).

Details: [AUDIT.md](docs/AUDIT.md), [CHANGES.md](docs/CHANGES.md),
[TESTING.md](docs/TESTING.md).

## Limitations and provenance

This remains a teaching system, not a production filesystem. The suite does not
establish crash consistency, concurrent filesystem safety, complete resource
reclamation, or correctness of every process/VM feature. The existing void
cache-release API cannot directly return write failures; explicit flush is
required to check completion. The simple course heap does not reuse freed
allocations. See [KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md).

Existing University of Illinois copyright and NCSA notices are retained.
Original framework, group contributions and this revision's engineering work
must be distinguished when describing the project. No personal authorship of
the original code is inferred from the supplied files.
