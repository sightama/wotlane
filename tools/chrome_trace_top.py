# ABOUTME: Reads a Chrome trace with V8's CPU profiler samples (tools/browser_mp_test/profile_chrome.mjs) and prints where one thread's
# ABOUTME: time goes: the busiest functions by self and by total time; by default the busiest worker thread (the game's EE thread).
"""usage: python3 tools/chrome_trace_top.py <trace.json> [--thread NAME|INDEX] [--threads] [--skip S] [--last S] [--top N] [--under FUNC]
  --thread  thread name (substring) or index from --threads; default: the worker thread with the most busy samples
  --threads list the profiled threads (index, process, thread, name, busy samples) and stop
  --skip    leave out the first S seconds of the thread's samples (default 0)
  --last    keep only the last S seconds (default: everything after --skip)
  --top     rows per table (default 40)
  --under   only samples whose stack contains a function with this substring, percentages of those
A sample is busy unless its leaf is V8's (idle), (program) or (garbage collector) node, or a wait (_do_futex_wait, Atomics.wait). The same tables as
tools/gecko_profile_top.py, so Chrome and Firefox read alike."""
import json
import sys
from collections import Counter

IDLE = {'(idle)', '(program)', '(root)'}
WAITS = ('_do_futex_wait', 'emscripten_futex_wait', 'Atomics.wait')   # a thread asleep in a wait, not working


class Profile:
    """One V8 CPU profile (one thread): the call tree's nodes and the samples, each a leaf node id with its time in ms."""

    def __init__(self, pid, tid, name):
        self.pid, self.tid, self.name = pid, tid, name
        self.nodes = {}     # id -> (function name, parent id)
        self.samples = []   # (time ms, leaf node id)
        self._t = 0   # the time of the last sample, us

    def add_chunk(self, data):
        """One ProfileChunk's data: more call tree nodes, and samples whose times follow from the previous chunk's last one."""
        prof = data.get('cpuProfile', {})
        for n in prof.get('nodes', []):
            cf = n.get('callFrame', {})
            name = cf.get('functionName') or '(anonymous)'
            url = cf.get('url') or ''
            label = f"{name} ({url.rsplit('/', 1)[-1]})" if url and not name.startswith('(') else name
            self.nodes[n['id']] = (label, n.get('parent'))
        for leaf, delta in zip(prof.get('samples', []), data.get('timeDeltas', [])):
            self._t += delta
            self.samples.append((self._t / 1000.0, leaf))

    def stack(self, leaf):
        """Function names from the leaf up (the root left out)."""
        out = []
        while leaf is not None and leaf in self.nodes:
            name, parent = self.nodes[leaf]
            if name != '(root)':
                out.append(name)
            leaf = parent
        return out

    def busy(self, names):
        return bool(names) and names[0] not in IDLE and names[0] != '(garbage collector)' and not names[0].startswith(WAITS)


def read(path):
    """Profiles of every thread in the trace, in the order they appear."""
    with open(path) as f:
        events = json.load(f)
    if isinstance(events, dict):
        events = events.get('traceEvents', [])
    thread_names = {(e['pid'], e['tid']): e.get('args', {}).get('name', '')
                    for e in events if e.get('ph') == 'M' and e.get('name') == 'thread_name'}
    # a profile is (pid, id); its Profile event names the sampled thread (the chunks come from a sampler thread of their own). Its
    # chunks are read in time order from its start time, whatever order the file lists the events in (a chunk may come first)
    starts, threads, chunks, order = {}, {}, {}, []
    for e in events:
        if e.get('name') not in ('Profile', 'ProfileChunk'):
            continue
        key = (e['pid'], e.get('id'))
        if key not in chunks:
            chunks[key] = []
            order.append(key)
        data = e.get('args', {}).get('data', {})
        if e['name'] == 'Profile':
            starts[key] = data.get('startTime')
            threads[key] = e['tid']
        else:
            chunks[key].append((e.get('ts', 0), len(chunks[key]), data))
    profiles = {}
    for key in order:
        pid, tid = key[0], threads.get(key)
        p = profiles[key] = Profile(pid, tid, thread_names.get((pid, tid), ''))
        ordered = sorted(chunks[key], key=lambda c: c[:2])
        start = starts.get(key)
        p._t = start if start is not None else (ordered[0][0] if ordered else 0)
        for _, _, data in ordered:
            p.add_chunk(data)
    return [profiles[k] for k in order]


def busy_count(p):
    return sum(1 for _, leaf in p.samples if p.busy(p.stack(leaf)))


def pick(profiles, sel):
    if sel is not None and sel.isdigit():
        if int(sel) >= len(profiles):
            raise SystemExit(f'no thread {sel}: there are {len(profiles)} (see --threads)')
        return profiles[int(sel)]
    want = sel if sel is not None else 'Worker'
    cands = [p for p in profiles if want.lower() in p.name.lower()]
    if not cands:
        raise SystemExit(f"no thread named like '{want}' (see --threads)")
    return max(cands, key=busy_count)


def tally(p, rows, under):
    self_c, total_c = Counter(), Counter()
    kept = idle = 0
    for _, leaf in rows:
        names = p.stack(leaf)
        if not p.busy(names):
            idle += 1
            continue
        if under and not any(under in x for x in names):
            continue
        kept += 1
        self_c[names[0]] += 1
        for x in set(names):
            total_c[x] += 1
    return kept, idle, self_c, total_c


def main(argv):
    if not argv or argv[0] in ('-h', '--help'):
        print(__doc__)
        return 0 if argv else 2
    path, opts, flags = argv[0], {}, set()
    i = 1
    while i < len(argv):
        k = argv[i]
        if k == '--threads':
            flags.add(k)
            i += 1
            continue
        if k not in ('--thread', '--skip', '--last', '--top', '--under') or i + 1 >= len(argv):
            print(__doc__, file=sys.stderr)
            return 2
        opts[k] = argv[i + 1]
        i += 2
    profiles = read(path)
    if not profiles:
        print('no V8 CPU profile in the trace (record it with profile_chrome.mjs)', file=sys.stderr)
        return 1
    if '--threads' in flags:
        for n, p in enumerate(profiles):
            print(f'{n:3d}  pid {p.pid}  tid {p.tid}  {p.name or "?":28s} busy samples {busy_count(p)}')
        return 0
    p = pick(profiles, opts.get('--thread'))
    rows = sorted(p.samples)
    if not rows:
        print('no samples on that thread', file=sys.stderr)
        return 1
    lo = rows[0][0] + float(opts.get('--skip', 0)) * 1000
    if '--last' in opts:
        lo = max(lo, rows[-1][0] - float(opts['--last']) * 1000)
    rows = [r for r in rows if r[0] >= lo]
    under = opts.get('--under')
    kept, idle, self_c, total_c = tally(p, rows, under)
    span = (rows[-1][0] - rows[0][0]) / 1000 if rows else 0
    print(f'thread {p.name} (pid {p.pid}, tid {p.tid}), {span:.1f} s: {kept} busy samples kept, {idle} idle'
          + (f", only under '{under}'" if under else ''))
    if not kept:
        return 1
    top = int(opts.get('--top', 40))
    for title, c in (('self', self_c), ('total', total_c)):
        print(f'\n{title} | % | samples | function')
        for x, v in c.most_common(top):
            print(f'{title} | {100 * v / kept:5.1f} | {v} | {x[:160]}')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
