# ABOUTME: Compares per-frame state hash files (WOTM_STATEHASH) of a reference run and other runs frame by frame and prints how the
# ABOUTME: difference spreads: differing 128 KB blocks at some frames, and whether it stays in a few blocks or reaches the game's logic.
"""usage: python3 tools/hashspread.py <reference.hash> <run.hash>... [--at f1,f2,...]
  --at   frames to print the differing block count at (default 60,300,600,1200,2400,4800)
For each run: the first differing frame, the differing blocks at those frames, the most blocks seen differing, and the blocks that
ever differ (with tools/statediff.py --blocks <list> to name the symbols in them). A difference that stays in the same few blocks is a
buffer the game rewrites; one that spreads to most blocks within seconds changed what the game does.
Hash file records (native/runtime/wotm_sync.inc, FrameHash): u32 index, u32 g_frame, u64 total, u64 blocks[256]."""
import struct
import sys

BLOCKS = 256
RECORD = struct.Struct("<IIQ" + "Q" * BLOCKS)


def load(path):
    data = open(path, "rb").read()
    out = {}
    for off in range(0, len(data) - RECORD.size + 1, RECORD.size):
        rec = RECORD.unpack_from(data, off)
        out[rec[0]] = rec[3:]
    return out


def spread(ref, run, at):
    """Frame-by-frame comparison of two loaded runs: common frames, first differing frame, the differing block count at the frames of
    `at` both runs have, the most blocks differing at once and every block that ever differed."""
    frames = sorted(set(ref) & set(run))
    first, most, ever, counts = None, 0, set(), {}
    for f in frames:
        diff = [b for b in range(BLOCKS) if ref[f][b] != run[f][b]]
        if diff and first is None:
            first = f
        most = max(most, len(diff))
        ever.update(diff)
        counts[f] = len(diff)
    return {"frames": len(frames), "first": first, "most": most, "ever": sorted(ever), "at": {f: counts[f] for f in at if f in counts}}


def main(argv):
    if len(argv) < 2 or argv[0] in ("-h", "--help"):
        print(__doc__)
        return 0 if argv and argv[0] in ("-h", "--help") else 2
    at = [60, 300, 600, 1200, 2400, 4800]
    paths = []
    i = 0
    while i < len(argv):
        if argv[i] == "--at":
            if i + 1 >= len(argv):
                print(__doc__, file=sys.stderr)
                return 2
            at = [int(x) for x in argv[i + 1].split(",")]
            i += 2
            continue
        paths.append(argv[i])
        i += 1
    ref = load(paths[0])
    for path in paths[1:]:
        s = spread(ref, load(path), at)
        shown = ", ".join(f"{f}: {n}" for f, n in s["at"].items())
        print(f"{path}: {s['frames']} common frames; first differing frame {s['first']}; differing blocks at frame ({shown}); "
              f"most {s['most']}; ever {len(s['ever'])}: {','.join(map(str, s['ever']))[:400]}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
