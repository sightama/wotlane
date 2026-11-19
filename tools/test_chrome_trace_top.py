#!/usr/bin/env python3
# ABOUTME: Tests for tools/chrome_trace_top.py (self and total time per function on one thread of a Chrome trace's V8 CPU profiles).
# ABOUTME: Run: python tools/test_chrome_trace_top.py
import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import chrome_trace_top as c  # noqa: E402


def node(i, name, parent=None, url=''):
    return {'id': i, 'callFrame': {'functionName': name, 'url': url}, **({'parent': parent} if parent else {})}


# root > top > fa > fb, top > fc, root > (idle), top > _do_futex_wait
NODES = [node(1, '(root)'), node(2, 'top', 1), node(3, 'fa', 2, 'http://x/wotm.wasm'), node(4, 'fb', 3), node(5, 'fc', 2),
         node(6, '(idle)', 1), node(7, '_do_futex_wait', 2, 'http://x/wotm.wasm')]


def trace(threads):
    """threads: [(pid, tid, profile id, thread name, [leaf ids], [time deltas in us])], each profile sent as two chunks (the nodes in the
    first), as Chrome splits them."""
    ev = []
    for pid, tid, pid_, name, leaves, deltas in threads:
        ev.append({'ph': 'M', 'name': 'thread_name', 'pid': pid, 'tid': tid, 'args': {'name': name}})
        ev.append({'ph': 'P', 'name': 'Profile', 'pid': pid, 'tid': tid, 'id': pid_, 'args': {'data': {'startTime': 1000}}})
        half = len(leaves) // 2
        ev.append({'ph': 'P', 'name': 'ProfileChunk', 'pid': pid, 'tid': tid, 'id': pid_,
                   'args': {'data': {'cpuProfile': {'nodes': NODES, 'samples': leaves[:half]}, 'timeDeltas': deltas[:half]}}})
        ev.append({'ph': 'P', 'name': 'ProfileChunk', 'pid': pid, 'tid': tid, 'id': pid_,
                   'args': {'data': {'cpuProfile': {'samples': leaves[half:]}, 'timeDeltas': deltas[half:]}}})
    f = tempfile.NamedTemporaryFile('w', suffix='.json', delete=False)
    json.dump({'traceEvents': ev}, f)
    f.close()
    return f.name


class Tally(unittest.TestCase):
    def test_counts_self_and_total_on_busy_samples_only(self):
        [p] = c.read(trace([(1, 7, '0x1', 'DedicatedWorker thread', [4, 4, 3, 5, 6, 7], [1000] * 6)]))
        kept, idle, self_c, total_c = c.tally(p, p.samples, None)
        self.assertEqual((kept, idle), (4, 2))   # (idle) and the futex wait are not work
        self.assertEqual(self_c, {'fb': 2, 'fa (wotm.wasm)': 1, 'fc': 1})
        self.assertEqual(total_c, {'top': 4, 'fa (wotm.wasm)': 3, 'fb': 2, 'fc': 1})

    def test_sample_times_add_up_across_chunks(self):
        [p] = c.read(trace([(1, 7, '0x1', 'DedicatedWorker thread', [4, 4, 3, 5], [1000, 2000, 1000, 500])]))
        self.assertEqual([t for t, _ in p.samples], [2.0, 4.0, 5.0, 5.5])

    def test_under_keeps_the_samples_inside_a_function(self):
        [p] = c.read(trace([(1, 7, '0x1', 'DedicatedWorker thread', [4, 3, 5, 5], [1000] * 4)]))
        kept, _, _, total_c = c.tally(p, p.samples, 'fa')
        self.assertEqual(kept, 2)
        self.assertNotIn('fc', total_c)

    def test_chunks_from_sampler_threads_out_of_order(self):
        # as Chrome writes them: each profile's chunks come from a sampler thread of their own (tid 90, 91), not the sampled thread;
        # here a chunk also comes before its Profile event and the chunks are listed out of time order
        def chunk(pid_, tid, ts, leaves, deltas, nodes=None):
            prof = {'samples': leaves, **({'nodes': nodes} if nodes else {})}
            return {'ph': 'P', 'name': 'ProfileChunk', 'pid': 1, 'tid': tid, 'id': pid_, 'ts': ts,
                    'args': {'data': {'cpuProfile': prof, 'timeDeltas': deltas}}}
        ev = [chunk('0x1', 90, 30, [5], [1000]),
              chunk('0x1', 90, 20, [4, 3], [1000, 1000], NODES),
              {'ph': 'P', 'name': 'Profile', 'pid': 1, 'tid': 7, 'id': '0x1', 'ts': 10, 'args': {'data': {'startTime': 5000}}},
              {'ph': 'P', 'name': 'Profile', 'pid': 1, 'tid': 8, 'id': '0x2', 'ts': 10, 'args': {'data': {'startTime': 0}}},
              chunk('0x2', 91, 25, [4], [1000], NODES)]
        f = tempfile.NamedTemporaryFile('w', suffix='.json', delete=False)
        json.dump(ev, f)
        f.close()
        ps = {p.tid: p for p in c.read(f.name)}
        self.assertEqual(sorted(ps), [7, 8])
        self.assertEqual(ps[7].samples, [(6.0, 4), (7.0, 3), (8.0, 5)])   # from startTime, in chunk time order
        self.assertEqual(ps[8].samples, [(1.0, 4)])

    def test_a_thread_index_out_of_range_is_a_clear_error(self):
        ps = c.read(trace([(1, 2, '0x2', 'DedicatedWorker thread', [4], [1000])]))
        with self.assertRaises(SystemExit) as e:
            c.pick(ps, '5')
        self.assertIn('--threads', str(e.exception))

    def test_default_thread_is_the_busiest_worker(self):
        ps = c.read(trace([(1, 1, '0x1', 'CrRendererMain', [4] * 9, [1000] * 9),
                           (1, 2, '0x2', 'DedicatedWorker thread', [4] * 2, [1000] * 2),
                           (1, 3, '0x3', 'DedicatedWorker thread', [4] * 3 + [6] * 5, [1000] * 8)]))
        self.assertEqual(c.pick(ps, None).tid, 3)
        self.assertEqual(c.pick(ps, 'CrRendererMain').tid, 1)
        self.assertEqual(c.pick(ps, '1').tid, 2)


if __name__ == '__main__':
    unittest.main()
