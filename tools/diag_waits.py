# ABOUTME: Summarizes lockstep waits and frame pacing per player from local match diagnostics (the *-match.log files that
# ABOUTME: tools/browser_mp_test/latency.mjs and desync.mjs leave in <out>/diag), to compare runs before and after a pacing change.
"""Usage: python3 tools/diag_waits.py [--skip N] [--min-gfps F] [--windows] <run dir or match.log> [...]

For each run and each player, over the steady windows (the page's 5 s diag windows from seq N+1 on, default N=2, whose game rate is
at least --min-gfps, default 75% of the match rate): windows used, median and mean waits per window, wait share of the time, median gfps and fps, mean
ee_busy_ms, the fr_len buckets summed, raf_p95_ms median, lat_ms median, and the server's behind_avg mean for that player.
--windows also prints every window's line. Exit status 1 when no match.log is found."""
import glob
import os
import statistics
import sys


def fields(line):
    out = {}
    for tok in line.split()[1:]:
        if '=' in tok:
            k, v = tok.split('=', 1)
            out[k] = v
    return out


def num(d, k):
    try:
        return float(d[k])
    except (KeyError, ValueError):
        return None


def match_logs(arg):
    if os.path.isfile(arg):
        return [arg]
    return sorted(glob.glob(os.path.join(arg, '**', '*-match.log'), recursive=True))


def summarize(path, skip, min_gfps, show):
    clients, behind = {}, {}
    with open(path, encoding='utf-8', errors='replace') as f:
        for line in f:
            d = fields(line)
            if d.get('src') == 'client' and d.get('why') == 'periodic':
                clients.setdefault(d.get('player', '?'), []).append(d)
            elif d.get('src') == 'server' and 'behind_avg' in d:
                behind.setdefault(d.get('player', '?'), []).append(num(d, 'behind_avg'))
    print(path)
    for player in sorted(clients):
        floor = lambda d: min_gfps if min_gfps is not None else 0.75 * (num(d, 'hz') or 60)
        rows = [d for d in clients[player] if (num(d, 'seq') or 0) > skip and (num(d, 'gfps') or 0) >= floor(d)]
        if show:
            for d in clients[player]:
                print('   %s seq=%s gfps=%s fps=%s waits=%s wait_ms=%s ee_busy_ms=%s fr_len=%s raf_p95_ms=%s lat_ms=%s' % tuple(
                    [player] + [d.get(k, '-') for k in ('seq', 'gfps', 'fps', 'waits', 'wait_ms', 'ee_busy_ms', 'fr_len', 'raf_p95_ms', 'lat_ms')]))
        if not rows:
            print('  %s: no steady window (of %d)' % (player, len(clients[player])))
            continue
        waits = [num(d, 'waits') or 0 for d in rows]
        wait_ms = sum(num(d, 'wait_ms') or 0 for d in rows)
        fr = [0, 0, 0, 0]
        for d in rows:
            for k, v in enumerate(d.get('fr_len', '').split(',')[:4]):
                fr[k] += int(v) if v.isdigit() else 0
        med = lambda k: statistics.median([v for v in (num(d, k) for d in rows) if v is not None] or [float('nan')])
        mean = lambda k: statistics.mean([v for v in (num(d, k) for d in rows) if v is not None] or [float('nan')])
        b = [v for v in behind.get(player, []) if v is not None]
        print('  %s: %d windows, waits/5s median %.0f mean %.1f, wait share %.1f%%, gfps %.1f, fps %.1f, ee_busy %.1f ms, '
              'fr_len %s, raf_p95 %.1f ms, lat %.1f ms, behind_avg %s' % (
                  player, len(rows), statistics.median(waits), statistics.mean(waits), 100.0 * wait_ms / (5000.0 * len(rows)),
                  med('gfps'), med('fps'), mean('ee_busy_ms'), ','.join(map(str, fr)), med('raf_p95_ms'), med('lat_ms'),
                  '%.2f' % statistics.mean(b) if b else '-'))


def main(argv):
    skip, min_gfps, show, paths = 2, None, False, []
    i = 1
    while i < len(argv):
        a = argv[i]
        if a in ('-h', '--help'):
            print(__doc__)
            return 0
        if a == '--skip':
            i += 1
            skip = int(argv[i])
        elif a == '--min-gfps':
            i += 1
            min_gfps = float(argv[i])
        elif a == '--windows':
            show = True
        else:
            paths.append(a)
        i += 1
    logs = [p for a in paths for p in match_logs(a)]
    if not logs:
        print('no *-match.log under %s\n%s' % (' '.join(paths) or '(nothing given)', __doc__), file=sys.stderr)
        return 1
    for p in logs:
        summarize(p, skip, min_gfps, show)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
