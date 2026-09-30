PYTHON ?= python3
.DEFAULT_GOAL := build
.PHONY: build run debug test benchmark clean check-tools

check-tools:
	@command -v riscv64-unknown-elf-gcc >/dev/null || { echo 'Install gcc-riscv64-unknown-elf (see docs/BUILD.md)'; exit 1; }
	@command -v $(PYTHON) >/dev/null || { echo 'Python 3 is required'; exit 1; }

build: check-tools
	$(MAKE) -C src/sys
	$(MAKE) -C src/usr lab-smoke
	$(PYTHON) scripts/mkimage.py build/demo.raw --file build/user/smoke

run: build
	$(PYTHON) scripts/run_tests.py --mode smoke --no-build

debug: build
	qemu-system-riscv64 -machine virt -bios none -nographic -monitor none -m 8M -global virtio-mmio.force-legacy=false -drive file=build/demo.raw,id=blk0,if=none,format=raw -snapshot -device virtio-blk-device,drive=blk0 -kernel build/kernel/kernel.elf -S -s

test: check-tools
	$(PYTHON) scripts/run_tests.py

benchmark: check-tools
	$(PYTHON) scripts/run_tests.py --mode benchmark

clean:
	$(PYTHON) scripts/clean.py
