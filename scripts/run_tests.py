"""Build and run bounded QEMU checks, with strict completion and exit checks."""
import argparse
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
CASES = (
    "cache_hit_miss", "cache_invalid", "cache_shared_dirty", "cache_pinned_full",
    "cache_lru", "cache_read_failure", "cache_short_read", "cache_short_write",
    "cache_flush_retry", "cache_pinned_flush", "cache_stats_lifetime",
    "io_reference_ownership", "ktfs_create", "ktfs_duplicate_open",
    "ktfs_extend", "ktfs_cross_block", "ktfs_eof_zero", "ktfs_reopen",
    "ktfs_delete", "ktfs_empty_recreate",
)


def command(argv):
    subprocess.run(argv, cwd=ROOT, check=True)


def validate(output, returncode, mode):
    if mode == "smoke":
        output = re.sub(r"^<main:0> ", "", output, flags=re.MULTILINE)
    if returncode != 0:
        raise ValueError(f"QEMU exited with status {returncode}")
    if "[FAIL]" in output or "PANIC" in output or "ASSERT" in output:
        raise ValueError("Kernel reported a failure")
    passed = re.findall(r"^\[PASS\] (\w+)\s*$", output, re.MULTILINE)
    if mode == "test":
        expected = list(CASES)
    elif mode == "smoke":
        expected = ["user_exec_print"]
    else:
        expected = []
    if passed != expected:
        raise ValueError(f"Unexpected/missing/duplicate PASS markers: {passed}")
    count = 3 if mode == "benchmark" else len(expected)
    if re.findall(r"^\[DONE\] (\w+) (\d+)\s*$", output, re.MULTILINE) != [(mode, str(count))]:
        raise ValueError("Missing or inconsistent completion marker")
    rows = []
    if mode == "benchmark":
        for match in re.finditer(r"^BENCH (\w+) (\d+) (\d+) (\d+) (\d+) (\d+) (\d+)\s*$", output, re.MULTILINE):
            name, *values = match.groups()
            row = dict(zip(("operations", "hits", "misses", "evictions", "backing_reads", "backing_writes"), map(int, values)))
            row["workload"] = name
            if row["operations"] != row["hits"] + row["misses"] or row["backing_reads"] != row["misses"]:
                raise ValueError("Inconsistent benchmark counters")
            row["hit_rate"] = row["hits"] / row["operations"]
            rows.append(row)
        if [r["workload"] for r in rows] != ["sequential", "hot_set", "random"]:
            raise ValueError("Missing/duplicate benchmark workloads")
    return passed, rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=("test", "smoke", "benchmark"), default="test")
    parser.add_argument("--timeout", type=float, default=30)
    parser.add_argument("--no-build", action="store_true")
    args = parser.parse_args()
    if args.timeout <= 0:
        parser.error("timeout must be positive")
    for tool in ("qemu-system-riscv64",) + (() if args.no_build else ("make", "riscv64-unknown-elf-gcc")):
        if not shutil.which(tool):
            raise SystemExit(f"Missing {tool}; see docs/BUILD.md")
    build = ROOT / "build"
    build.mkdir(exist_ok=True)
    mode = args.mode
    # A failed rerun must not leave a stale success report behind.
    (build / f"{mode}.json").unlink(missing_ok=True)
    kernel = {"test": "storage-test", "benchmark": "bench", "smoke": "kernel"}[mode]
    if not args.no_build:
        if mode == "smoke":
            command(["make", "build"])
        else:
            command(["make", "-C", "src/sys", f"../../build/kernel/{kernel}.elf"])
    with tempfile.TemporaryDirectory(prefix="qemu-", dir=build) as tmp:
        disk = Path(tmp) / "disk.raw"
        if mode == "smoke":
            shutil.copyfile(build / "demo.raw", disk)
        else:
            command([sys.executable, "scripts/mkimage.py", str(disk)])
        argv = ["qemu-system-riscv64", "-machine", "virt", "-bios", "none",
                "-nographic", "-monitor", "none", "-m", "8M",
                "-global", "virtio-mmio.force-legacy=false",
                "-drive", f"file={disk.as_posix()},id=blk0,if=none,format=raw",
                "-device", "virtio-blk-device,drive=blk0",
                "-kernel", str(build / "kernel" / f"{kernel}.elf")]
        timed_out = False
        try:
            result = subprocess.run(argv, cwd=ROOT, capture_output=True, text=True, timeout=args.timeout)
            output = result.stdout + result.stderr
            returncode = result.returncode
        except subprocess.TimeoutExpired as exc:
            def decode(value):
                return value.decode(errors="replace") if isinstance(value, bytes) else (value or "")
            output = decode(exc.stdout) + decode(exc.stderr)
            returncode = -1
            timed_out = True
    (build / f"{mode}.log").write_text(output, encoding="utf-8")
    print(output, end="")
    if timed_out:
        raise ValueError(f"QEMU timed out after {args.timeout:g}s; see build/{mode}.log")
    passed, rows = validate(output, returncode, mode)
    report = {"mode": mode, "passed": passed, "benchmark": rows,
              "qemu": subprocess.check_output(["qemu-system-riscv64", "--version"], text=True).splitlines()[0]}
    (build / f"{mode}.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if rows:
        print("Workload       Reads   Hits   Misses   Hit Rate   Backing Reads   Writes")
        for row in rows:
            print(f"{row['workload']:14} {row['operations']:5} {row['hits']:6} {row['misses']:8} {row['hit_rate']:9.2%} {row['backing_reads']:15} {row['backing_writes']:8}")
    else:
        print(f"{len(passed)} passed, 0 failed")


if __name__ == "__main__":
    try:
        main()
    except (OSError, subprocess.CalledProcessError, ValueError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        sys.exit(1)
