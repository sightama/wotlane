# ABOUTME: Summarizes the cost lines of rollback synctest logs (native stderr or the browser runner's log): per log, the median of the
# ABOUTME: 300-frame window averages and the worst window maximum for save, restore, compare, the redone frame and the present frame.
"""usage: python3 tools/synctest_cost.py <log>... [--skip N]

  <log>    a WOTM_SYNCTEST log (tools/synctest.sh --log, tools/browser_mp_test/synctest.mjs's runtime/browser_synctest/*.log)
  --skip   leading windows to leave out (default 1: the first window includes the fight's start)

Prints one row per log: windows, checks, mismatches, then avg (median of window averages) / max (worst window) in ms for each cost.
The page prefixes its lines with seconds; both forms are read.
"""
import re
import statistics
import sys

COSTS = ["save", "restore", "compare", "resimulated frame", "present frame"]
LINE = re.compile(r"\[synctest\] frame \d+, \d+ frames back, avg/max over \d+ frames: (.*)$")


def parse(path, skip):
    rows, checks, mismatches = [], 0, 0
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            if "[synctest] MISMATCH" in line:
                mismatches += 1
            m = LINE.search(line)
            if not m:
                continue
            row = {}
            for cost in COSTS:
                c = re.search(re.escape(cost) + r" ([0-9.]+)/([0-9.]+) ms", m.group(1))
                if c:
                    row[cost] = (float(c.group(1)), float(c.group(2)))
            k = re.search(r"(\d+) checks", m.group(1))
            checks += int(k.group(1)) if k else 0
            rows.append(row)
    return rows[skip:], checks, mismatches


def main(argv):
    skip, logs = 1, []
    i = 0
    while i < len(argv):
        if argv[i] in ("-h", "--help"):
            print(__doc__)
            return 0
        if argv[i] == "--skip":
            if i + 1 >= len(argv) or not argv[i + 1].isdigit():
                print("error: --skip needs a number of windows\n" + __doc__, file=sys.stderr)
                return 2
            skip = int(argv[i + 1])
            i += 2
            continue
        logs.append(argv[i])
        i += 1
    if not logs:
        print(__doc__, file=sys.stderr)
        return 2
    print("log | windows | checks | mismatches | " + " | ".join(c + " avg/max" for c in COSTS))
    for path in logs:
        rows, checks, mismatches = parse(path, skip)
        cells = []
        for cost in COSTS:
            vals = [r[cost] for r in rows if cost in r]
            cells.append("%.2f/%.2f" % (statistics.median(v[0] for v in vals), max(v[1] for v in vals)) if vals else "-")
        print("%s | %d | %d | %d | %s" % (path, len(rows), checks, mismatches, " | ".join(cells)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
