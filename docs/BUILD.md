# Build and run

## Tested environment

Ubuntu 24.04 x86-64, GNU Make 4.3, Python 3.12.3,
`riscv64-unknown-elf-gcc` 13.2.0, binutils 2.42, QEMU 8.2.2.
The actual validation host was Windows with Docker Desktop supplying this
isolated Linux environment. These are tested versions, not minimum-version
claims. No third-party Python packages are needed.

```sh
sudo apt-get update
sudo apt-get install -y make gcc-riscv64-unknown-elf binutils-riscv64-unknown-elf qemu-system-misc python3
make build
make run
make test
make benchmark
```

All commands above run from the repository root. On Windows, use Ubuntu in WSL
or an existing Linux development environment. The POSIX Makefile commands are
not intended for native PowerShell execution.

For this specific workspace the already-prepared development container is
`riscv-storage-lab-dev`. From PowerShell, while it is running:

```powershell
docker exec riscv-storage-lab-dev make build
docker exec riscv-storage-lab-dev make run
docker exec riscv-storage-lab-dev make test
docker exec riscv-storage-lab-dev make benchmark
```

That container is an environment convenience, not a project dependency. To
create an equivalent temporary environment on a new Windows host with Docker:

```powershell
docker run --rm -it --mount "type=bind,source=$($PWD.Path),target=/work" -w /work ubuntu:24.04 bash
```

Inside the Linux shell, run the apt install commands above without `sudo`, then
the Make targets. This avoids adding a containerized application to the project.

## Commands and outputs

| Command | Result |
|---|---|
| `make build` | `build/kernel/kernel.elf`, `build/user/smoke`, `build/demo.raw` |
| `make run` | Boot generated image, execute user smoke, exit after validation |
| `make test` | Compile test kernel; run 20 storage cases on a fresh disk |
| `make benchmark` | Compile benchmark kernel; print counters for three workloads |
| `make debug` | Boot paused, GDB server on port 1234 |
| `make clean` | Delete only generated `build/` directory, including its logs |
| `python3 scripts/test_runner.py` | Test marker validation and false-positive rejection |

The kernel keeps its existing Makefile and object list; output paths and
header dependencies were added. Original compiled `.o` files are never used by
the new workflow. Linker-script edits also trigger relinking. The new user demo
is compiled consistently for `rv64imazicsr` / LP64, matching the kernel.

Lower-level commands:

```sh
make -C src/sys
make -C src/usr lab-smoke
python3 scripts/mkimage.py build/demo.raw --file build/user/smoke
python3 scripts/run_tests.py --mode smoke --no-build
python3 scripts/run_tests.py --timeout 60
```

The original `make -C src/usr` still builds coursework `hello`; it is not the
validated user-mode demo. Alternate historical test Makefiles are retained but
not part of the supported workflow. `make -C src/sys run` and `debug` delegate
to the root targets so they use a regenerated demo disk.

## Disk images

`scripts/mkimage.py` constructs KTFS bytes from the existing structs in
`src/sys/ktfs.h`: 512-byte blocks, 16,384 blocks, four bitmap blocks, six inode
blocks, root inode zero, little-endian fields. Allocation bitmap indices are
absolute disk blocks; inode block pointers are relative to the data region.

It supports a flat directory and files up to 131 blocks, using direct and one
indirect block. It is intentionally a small fixture builder. The regression
disk starts with an empty root, testing first-file creation. The demo disk
contains the source-built `smoke` ELF. Generated fixture names are unique ASCII,
1–13 bytes. The supplied `src/sys/ktfs.raw` and binary-only mkfs tools are not
needed by the new workflow and remain untouched in the original local download.
The GitHub source repository excludes these legacy binaries and disk images;
all supported commands regenerate their required artifacts from source.

Every runner invocation creates a temporary disk under `build/`, then removes
it after QEMU exits. Smoke mode copies `build/demo.raw`; storage and benchmark
modes create empty images. The test suite never edits the original disk.

## Debugging

Run `make debug`, then in a separate terminal with `gdb-multiarch` installed:

```sh
gdb-multiarch build/kernel/kernel.elf
```

Inside GDB:

```text
target remote localhost:1234
break main
continue
```

The source includes debug symbols. Interactive debugging intentionally has no
timeout; ordinary tests default to 30 seconds. QEMU uses one `virt` machine
with 8 MiB RAM, UART0 console, modern VirtIO MMIO and a single block device.

## Failure behavior

Missing tools, compilation errors, QEMU failure, timeout, missing tests, duplicate
tests or absent completion markers produce nonzero runner exit status. Raw
output is saved to `build/test.log`, `smoke.log` or `benchmark.log`.
Successful runs also write corresponding JSON. Before a rerun, any old success
JSON for that mode is removed so a failure cannot masquerade as a fresh pass.

An intentionally tiny timeout can exercise the failure path:

```sh
python3 scripts/run_tests.py --no-build --timeout 0.001
```

This was observed to return status 1 with an explicit timeout error. The runner
kills/reaps the QEMU subprocess using Python's bounded subprocess API.
