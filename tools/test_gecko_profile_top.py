#!/usr/bin/env python3
# ABOUTME: Tests for tools/gecko_profile_top.py (self and total time per function on one thread of a Firefox profile).
# ABOUTME: Run: python tools/test_gecko_profile_top.py
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gecko_profile_top as g  # noqa: E402


def thread(name, samples):
    """A Gecko thread whose stacks are top > fa > fb (stack 2), top > fa (stack 1) and top > fc (stack 3).
    samples: (stack, time, cpu delta)."""
    return {
        "name": name,
        "stringTable": ["top", "fa", "fb", "fc"],
        "frameTable": {"schema": {"location": 0}, "data": [[0], [1], [2], [3]]},
        "stackTable": {"schema": {"prefix": 0, "frame": 1}, "data": [[None, 0], [0, 1], [1, 2], [0, 3]]},
        "samples": {"schema": {"stack": 0, "time": 1, "threadCPUDelta": 2}, "data": samples},
    }


class Tally(unittest.TestCase):
    def test_counts_self_and_total_on_busy_samples_only(self):
        th = g.Thread(thread("DOM Worker", [[2, 0, 5], [2, 1, 5], [1, 2, 5], [3, 3, 5], [3, 4, 0], [None, 5, 5]]))
        kept, idle, self_c, total_c = g.tally(th, th.samples, None)
        self.assertEqual((kept, idle), (4, 1))
        self.assertEqual(self_c, {"fb": 2, "fa": 1, "fc": 1})
        self.assertEqual(total_c, {"top": 4, "fa": 3, "fb": 2, "fc": 1})

    def test_under_keeps_the_samples_inside_a_function(self):
        th = g.Thread(thread("DOM Worker", [[2, 0, 5], [1, 1, 5], [3, 2, 5]]))
        kept, _, self_c, total_c = g.tally(th, th.samples, "fa")
        self.assertEqual(kept, 2)
        self.assertNotIn("fc", total_c)

    def test_default_thread_is_the_busiest_worker(self):
        prof = {"meta": {}, "threads": [thread("GeckoMain", [[2, 0, 5]] * 9)],
                "processes": [{"meta": {}, "threads": [thread("DOM Worker", [[2, 0, 5]] * 2), thread("DOM Worker", [[2, 0, 5]] * 3)]}]}
        threads = g.threads_of(prof)
        self.assertEqual(g.pick(threads, None), 2)
        self.assertEqual(g.pick(threads, "GeckoMain"), 0)
        self.assertEqual(g.pick(threads, "1"), 1)


if __name__ == "__main__":
    unittest.main()
