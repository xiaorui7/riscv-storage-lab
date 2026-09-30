"""Host regression checks for rejecting false-positive QEMU output."""
import unittest
from run_tests import CASES, validate


class RunnerChecks(unittest.TestCase):
    def output(self):
        return "\n".join(f"[PASS] {name}" for name in CASES) + f"\n[DONE] test {len(CASES)}\n"

    def test_complete(self):
        self.assertEqual(validate(self.output(), 0, "test")[0], list(CASES))

    def test_reject_partial_duplicate_failure_and_exit(self):
        output = self.output()
        for bad in ("", output.replace("[DONE]", "[STOP]"),
                    output.replace(f"[PASS] {CASES[0]}\n", ""),
                    output + f"[PASS] {CASES[0]}\n", output + "PANIC\n",
                    output + "[FAIL] injected\n"):
            with self.subTest(output=bad):
                with self.assertRaises(ValueError):
                    validate(bad, 0, "test")
        with self.assertRaises(ValueError):
            validate(output, 1, "test")

    def test_benchmark_requires_consistent_counters(self):
        output = "\n".join(f"BENCH {name} 10 9 1 0 1 0" for name in ("sequential", "hot_set", "random"))
        output += "\n[DONE] benchmark 3\n"
        self.assertEqual(len(validate(output, 0, "benchmark")[1]), 3)
        with self.assertRaises(ValueError):
            validate(output.replace("10 9 1", "10 8 1"), 0, "benchmark")


if __name__ == "__main__":
    unittest.main()
