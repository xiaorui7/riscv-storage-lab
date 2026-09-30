# Execution evidence

These files are copied from actual command output, not illustrative expected
results. Baseline and intermediate failure logs refer to earlier source states;
their line numbers and kernel addresses may differ from the final files.

- `baseline-kernel.log`: original clean compilation fails on undefined PIPE_BUF.
- `baseline-user*.log`: forced-build directory issue and successful fresh link.
- `baseline-boot.log`, `baseline-ktfs-run.log`: original boot failure after a
  temporary compile-flag workaround, using a snapshot of the supplied disk.
- `before-cache-fix.log`: new suite exposes original dirty-cache lookup bug.
- `after-cache-fix.log`: cache tests pass, original reference ownership fails.
- `before-ktfs-fix.log`: original empty-directory creation fails.
- `clean-build.log`: final clean source rebuild and user smoke.
- `test.log/json`, `smoke.log/json`, `benchmark.log/json`: final successful runs.
- `runner-checks.log`: three host runner-check methods pass.

Benchmark rows matched in the initial and clean-rebuild runs. Original disk
SHA256 before/after testing is unchanged:
`fdde92f71ea63bd0307b8d4cea7d3b3db7590883db836c4ffbf42efbfe66be85`.

Actual negative timeout check:

```text
python3 scripts/run_tests.py --no-build --timeout 0.001
ERROR: QEMU timed out after 0.001s; see build/test.log
runner exit status: 1
```

The source suite verifies reopen within one boot; it is not a crash-recovery or
power-failure test. All results are bounded to the environment in BUILD.md.

GitHub Actions independently passed toolchain installation, source build, user
smoke, storage/runner regressions and benchmark on Ubuntu 24.04 for commit
`02730fde422e9da8296f537bcf769a1d6557f981`:
[remote run 36779180642](https://github.com/xiaorui7/riscv-storage-lab/actions/runs/36779180642).
