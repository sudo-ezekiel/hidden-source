"""Compare two dump_datamaps.py outputs class by class.

For each class in both, fields are matched by name. A class is reported when a field was added
or removed, or when the offset delta between the two builds changes from one field to the next
(a change point is where something was inserted or removed in the class or a base class).

Usage:
    python3 -I diff_layouts.py <reference fields.tsv> <candidate fields.tsv> [--class NAME]
"""
import argparse
import collections


def load(path):
    classes = collections.OrderedDict()
    for line in open(path):
        cls, base, field, ftype, off, count, flags, ext = line.rstrip("\n").split("\t")
        off = int(off, 16)
        if int(flags, 16) & 0x0008:
            continue  # FTYPEDESC_INPUT: an input handler, not storage
        classes.setdefault(cls, {})[field] = (off, ftype, int(count))
    return classes


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("ref")
    ap.add_argument("cand")
    ap.add_argument("--class", dest="cls")
    ap.add_argument("--all", action="store_true", help="also list classes whose layout matches")
    args = ap.parse_args()
    ref, cand = load(args.ref), load(args.cand)
    only_ref = [c for c in ref if c not in cand]
    only_cand = [c for c in cand if c not in ref]
    same = 0
    shifted = collections.defaultdict(list)
    for cls in sorted(set(ref) & set(cand)):
        if args.cls and cls != args.cls:
            continue
        r, c = ref[cls], cand[cls]
        names = sorted(set(r) | set(c), key=lambda f: (r.get(f) or c.get(f))[0])
        lines = []
        prev = None
        deltas = set()
        added_removed = False
        for f in names:
            a, b = r.get(f), c.get(f)
            if a is None:
                lines.append(f"    - {f} (only in candidate, at {b[0]:#x})")
                added_removed = True
                continue
            if b is None:
                lines.append(f"    + {f} {a[1]} at {a[0]:#x} (only in reference)")
                added_removed = True
                continue
            delta = a[0] - b[0]
            deltas.add(delta)
            if delta != prev:
                lines.append(f"    {f}: {a[0]:#x} vs {b[0]:#x}, delta {delta:+d}")
                prev = delta
        if added_removed or len(deltas) > 1 or args.cls:
            print(cls)
            print("\n".join(lines))
        elif deltas and next(iter(deltas)) != 0:
            shifted[next(iter(deltas))].append(cls)
        else:
            same += 1
    print(f"\n{same} classes with identical saved layouts")
    for delta, classes in sorted(shifted.items()):
        print(f"{len(classes)} classes only shifted by {delta:+d} (a base class grew): {' '.join(classes)}")
    print(f"only in reference ({len(only_ref)}): {' '.join(sorted(only_ref))}")
    print(f"only in candidate ({len(only_cand)}): {' '.join(sorted(only_cand))}")


if __name__ == "__main__":
    main()
