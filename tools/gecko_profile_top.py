# ABOUTME: Reads a Firefox (Gecko profiler) profile and prints where one thread's time goes: the busiest functions by self and by
# ABOUTME: total time, over a time range; by default the busiest worker thread (the game's EE thread in the web build).
"""usage: python3 tools/gecko_profile_top.py <profile.json> [--thread NAME|INDEX] [--skip S] [--last S] [--top N] [--under FUNC]
  --thread  thread name (substring) or index from --threads; default: the worker thread with the most busy samples
  --threads list the threads (process, index, name, busy samples) and stop
  --skip    leave out the first S seconds of the thread's samples (default 0)
  --last    keep only the last S seconds (default: everything after --skip)
  --top     rows per table (default 40)
  --under   only samples whose stack contains a function with this substring (e.g. the frame loop), percentages of those
A sample is busy when its thread used CPU since the previous one (threadCPUDelta > 0) or, without CPU data, has any JS or wasm frame.
Profiles come from tools/browser_mp_test/profile_firefox.mjs."""
import json
import sys
from collections import Counter


def threads_of(prof, proc=0, out=None):
    out = [] if out is None else out
    for t in prof.get("threads", []):
        out.append((proc, t))
    for i, sp in enumerate(prof.get("processes", [])):
        threads_of(sp, proc * 10 + i + 1, out)
    return out


def col(table, name):
    return table["schema"][name]


class Thread:
    def __init__(self, t):
        self.t = t
        self.strings = t["stringTable"]
        st, ft = t["stackTable"], t["frameTable"]
        self.stack_prefix = [r[col(st, "prefix")] for r in st["data"]]
        self.stack_frame = [r[col(st, "frame")] for r in st["data"]]
        self.frame_loc = [r[col(ft, "location")] for r in ft["data"]]
        s = t["samples"]
        self.si, self.ti = col(s, "stack"), col(s, "time")
        self.ci = s["schema"].get("threadCPUDelta")
        self.samples = s["data"]

    def name(self, frame):
        loc = self.frame_loc[frame]
        return self.strings[loc] if isinstance(loc, int) else str(loc)

    def frames(self, stack):
        out = []
        while stack is not None:
            out.append(self.stack_frame[stack])
            stack = self.stack_prefix[stack]
        return out   # leaf first

    def busy(self, row):
        if row[self.si] is None:
            return False
        if self.ci is not None and len(row) > self.ci and row[self.ci] is not None:
            return row[self.ci] > 0
        return True


def tally(th, rows, under):
    """Busy samples kept, idle samples, and the self and total sample counts per function name (only samples whose stack has a
    function containing `under`, when given)."""
    self_c, total_c = Counter(), Counter()
    kept = idle = 0
    for r in rows:
        if not th.busy(r):
            idle += r[th.si] is not None
            continue
        names = [th.name(f) for f in th.frames(r[th.si])]
        if under and not any(under in x for x in names):
            continue
        kept += 1
        self_c[names[0]] += 1
        for x in set(names):
            total_c[x] += 1
    return kept, idle, self_c, total_c


def pick(all_threads, sel):
    """Index of the thread `sel` names (an index, or a name substring: the busiest such thread); default the busiest worker."""
    if sel is not None and sel.isdigit():
        if int(sel) >= len(all_threads):
            raise SystemExit(f"no thread {sel}: there are {len(all_threads)} (see --threads)")
        return int(sel)
    want = sel if sel is not None else "Worker"
    cands = [k for k, (_, t) in enumerate(all_threads) if want in str(t.get("name"))]
    if not cands:
        raise SystemExit(f"no thread named like '{want}' (see --threads)")
    def busy(k):
        th = Thread(all_threads[k][1])
        return sum(1 for r in th.samples if th.busy(r))

    return max(cands, key=busy)


def main(argv):
    if not argv or argv[0] in ("-h", "--help"):
        print(__doc__)
        return 0 if argv else 2
    path, opts, flags = argv[0], {}, set()
    i = 1
    while i < len(argv):
        k = argv[i]
        if k == "--threads":
            flags.add(k)
            i += 1
            continue
        if k not in ("--thread", "--skip", "--last", "--top", "--under") or i + 1 >= len(argv):
            print(__doc__, file=sys.stderr)
            return 2
        opts[k] = argv[i + 1]
        i += 2
    prof = json.load(open(path))
    all_threads = threads_of(prof)
    busy = []
    for proc, t in all_threads:
        th = Thread(t)
        busy.append(sum(1 for r in th.samples if th.busy(r)))
    if "--threads" in flags:
        for n, ((proc, t), b) in enumerate(zip(all_threads, busy)):
            print(f"{n:3d}  process {proc:3d}  {t.get('name')!s:20s} busy samples {b}")
        return 0
    n = pick(all_threads, opts.get("--thread"))
    th = Thread(all_threads[n][1])
    rows = [r for r in th.samples if r[th.si] is not None]
    if not rows:
        print("no samples on that thread", file=sys.stderr)
        return 1
    t0, t1 = rows[0][th.ti], rows[-1][th.ti]
    lo = t0 + float(opts.get("--skip", 0)) * 1000
    if "--last" in opts:
        lo = max(lo, t1 - float(opts["--last"]) * 1000)
    rows = [r for r in rows if r[th.ti] >= lo]
    under = opts.get("--under")
    top = int(opts.get("--top", 40))
    kept, idle, self_c, total_c = tally(th, rows, under)
    span = (rows[-1][th.ti] - rows[0][th.ti]) / 1000 if rows else 0
    print(f"thread {n} ({all_threads[n][1].get('name')}), {span:.1f} s: {kept} busy samples kept, {idle} idle"
          + (f", only under '{under}'" if under else ""))
    if not kept:
        return 1
    for title, c in (("self", self_c), ("total", total_c)):
        print(f"\n{title} | % | samples | function")
        for x, v in c.most_common(top):
            print(f"{title} | {100 * v / kept:5.1f} | {v} | {x[:160]}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
