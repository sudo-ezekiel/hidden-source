"""Name client.dll's functions from its vftables, and check our class vtables against them slot by slot.

client.dll has no symbols, but MSVC RTTI names every polymorphic class and its vftables
(pe_rtti.py). Our classes' virtual order comes from one of two sources:
- GCC: -fdump-class-hierarchy output of the client check (CLASSDUMP=1 rebuild/client_check.sh),
  converted to MSVC order by gcc_classdump.py;
- MSVC (later, once the original compiler is available): our own client.dll plus its /MAP file,
  --ours-dll/--ours-map; slot targets are then named straight from the map.
Each reference slot gets the name of our function in the same slot.

Checks per class (vtable_report.tsv, vtables_byclass/<Class>.txt):
- slot counts per vftable, ours against the reference;
- inheritance: where our slot says a function is inherited from base X, the reference slot must
  hold the same address as X's reference slot ("inherit" errors mean the slots are misaligned);
  where we override, the reference should differ from the base ("override?" notes, weak: MSVC
  folds identical functions, /OPT:ICF).
A class is MATCH when every vftable has the reference's slot count and there are no inherit errors.

Usage:
    python3 -I vtable_names.py <client.dll> <decomp dir> --dumps <obj dir> [--out <dir>]
        [--class NAME ...] [--no-decomp]
    python3 -I vtable_names.py <client.dll> <decomp dir> --ours-dll <our client.dll> --ours-map <map>

With --class only those classes are reported, to stdout, and nothing is written. Otherwise writes
names.tsv (address, name, confidence, slots naming it; confidence high/medium/low/folded from our
layouts, "slot" = only the class and slot are known: Class::vfuncN), vtable_report.tsv, vtables_byclass/,
and unless --no-decomp, decomp_named.c and byclass/<Class>.c from <decomp dir>/decomp.c.
"""
import argparse
import glob
import os
import re
import sys
from collections import defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pe_rtti  # noqa: E402


def ref_tables(dll):
    pe = pe_rtti.Pe(dll)
    r = pe_rtti.rtti(pe)
    ref = defaultdict(dict)
    bases = {}
    for v in r["vtables"]:
        ref[v["class"]][v["offset"]] = v["slots"]
        bases[v["class"]] = v["bases"]
    return ref, bases


def gcc_source(dumps):
    import gcc_classdump as g
    paths = glob.glob(os.path.join(dumps, "**", "*.class"), recursive=True)
    if not paths:
        raise SystemExit(f"no .class dumps under {dumps}: run CLASSDUMP=1 rebuild/client_check.sh first")
    classes, conflicts = g.load_dumps(paths)
    lay = g.Layouts(classes)
    ours = {}
    primary = {}
    for name, ci in classes.items():
        vt = lay.vftables(name)
        if vt:
            ours[name] = {off: (sub, slots) for off, sub, slots in vt}
            primary[name] = ci.primary_base()
    return ours, primary, conflicts


MAP_LINE = re.compile(r"^\s*[0-9a-f]{4}:[0-9a-f]{8}\s+(\S+)\s+([0-9a-f]{8})\s", re.I)


def msvc_simple_demangle(sym):
    """'?Method@Class@@QAEXXZ' -> 'Class::Method'; constructors/destructors and operators partly."""
    if not sym.startswith("?"):
        return sym
    s = sym[1:]
    special = {"?0": None, "?1": "~", "?_G": "~", "?_E": "~"}
    prefix = None
    for k, v in special.items():
        if s.startswith(k):
            prefix = (k, v)
            s = s[len(k):]
            break
    parts = s.split("@@", 1)[0].split("@")
    if prefix:
        cls = "::".join(reversed(parts))
        last = parts[0]
        return f"{cls}::{(prefix[1] or '') + last}"
    return "::".join(reversed(parts[1:])) + "::" + parts[0] if len(parts) > 1 else parts[0]


def msvc_source(dll, mapfile):
    from gcc_classdump import Slot
    pe = pe_rtti.Pe(dll)
    r = pe_rtti.rtti(pe)
    names = {}
    with open(mapfile, errors="replace") as f:
        for line in f:
            m = MAP_LINE.match(line)
            if m:
                names.setdefault(int(m.group(2), 16), msvc_simple_demangle(m.group(1)))
    ours = defaultdict(dict)
    for v in r["vtables"]:
        slots = [Slot(names.get(t, f"sub_{t:08x}"), "method", i) for i, t in enumerate(v["slots"])]
        ours[v["class"]][v["offset"]] = (v["class"], slots)
    primary = {}
    for v in r["vtables"]:
        b = v["bases"]
        primary[v["class"]] = b[1][0] if len(b) > 1 and b[1][1] == 0 else None
    return dict(ours), primary, {}


def remap_offsets(ref, bases, ours):
    """Key our secondary vftables by the reference's subobject offset, matching by base class name
    where the offsets differ (a data layout difference, reported separately from the slots)."""
    moved = {}
    for cls, o in ours.items():
        if cls not in ref:
            continue
        r = ref[cls]
        at = defaultdict(set)
        for bname, mdisp in bases.get(cls, []):
            at[mdisp].add(bname)
        new = {}
        for off, (sub, slots) in o.items():
            if off == 0 or (off in r and (sub in at.get(off, ()) or not at)):
                new[off] = (sub, slots)
                continue
            cands = [k for k in r if k and sub in at.get(k, ()) and k not in o]
            if len(cands) == 1:
                new[cands[0]] = (sub, slots)
                moved.setdefault(cls, []).append((sub, off, cands[0]))
            else:
                new[off] = (sub, slots)
        ours[cls] = new
    return moved


def owner_of(name):
    if name is None:
        return None
    depth = 0
    for i in range(len(name) - 1, 0, -1):
        c = name[i]
        if c == ">":
            depth += 1
        elif c == "<":
            depth -= 1
        elif c == ":" and name[i - 1] == ":" and depth == 0:
            return name[:i - 1]
    return None


def check(ref, ours, primary):
    """Per class: status, per-offset counts, per-slot notes."""
    report = {}
    for cls in sorted(ref):
        r = ref[cls]
        o = ours.get(cls)
        if o is None:
            report[cls] = {"status": "MISSING", "tables": [], "notes": {}}
            continue
        tables = []
        notes = {}
        ok = True
        for off in sorted(set(r) | set(o)):
            rs = r.get(off)
            os_ = o.get(off, (None, None))[1]
            tables.append((off, len(os_) if os_ is not None else None, len(rs) if rs is not None else None))
            if rs is None or os_ is None or len(rs) != len(os_):
                ok = False
        # inheritance consistency along the primary chain
        inherit_err = 0
        b = primary.get(cls)
        if b and b in ref and b in ours:
            for off, rs in r.items():
                if off not in o or off not in ref[b] or off not in ours[b]:
                    continue
                ours_c = o[off][1]
                ours_b = ours[b][off][1]
                rb = ref[b][off]
                for j in range(min(len(rs), len(rb), len(ours_c), len(ours_b))):
                    nc = ours_c[j].name
                    nb = ours_b[j].name
                    if nc == nb and owner_of(nc) != cls:
                        if rs[j] != rb[j] and ours_c[j].kind != "pure":
                            notes[(off, j)] = f"inherit: ours {nc} from {b}, reference differs from {b}"
                            inherit_err += 1
                    elif owner_of(nc) == cls and rs[j] == rb[j] and ours_c[j].kind not in ("pure",):
                        notes[(off, j)] = f"override?: ours {nc}, reference keeps {b}'s"
        if inherit_err:
            ok = False
        report[cls] = {"status": "MATCH" if ok else "MISMATCH", "tables": tables, "notes": notes,
                       "inherit_errors": inherit_err}
    return report


def assign_names(ref, ours, report):
    """address -> {name: [(class, offset, slot)]}, using only slots we trust."""
    cand = defaultdict(lambda: defaultdict(list))
    for cls, r in ref.items():
        o = ours.get(cls)
        if o is None:
            continue
        status = report[cls]["status"]
        for off, rs in r.items():
            if off not in o:
                continue
            os_ = o[off][1]
            same_len = len(os_) == len(rs)
            first_bad = min([j for (of, j) in report[cls]["notes"] if of == off and
                             report[cls]["notes"][(of, j)].startswith("inherit")] or [len(rs)])
            for j, t in enumerate(rs):
                if j >= len(os_):
                    break
                s = os_[j]
                if s.kind == "pure":
                    nm = "_purecall"
                else:
                    nm = s.name
                if s.kind == "thunk" and off:
                    nm = f"{nm}`adjustor{{{off}}}'"
                if status == "MATCH" or (same_len and j < first_bad):
                    conf = "high"
                elif j < first_bad:
                    conf = "medium"
                else:
                    conf = "low"
                cand[t][nm].append((cls, off, j, conf))
    return cand


def pick(cand):
    names = {}
    rank = {"high": 0, "medium": 1, "low": 2}
    for addr, byname in cand.items():
        scored = []
        for nm, uses in byname.items():
            best = min(rank[u[3]] for u in uses)
            scored.append((best, -len(uses), nm, uses))
        scored.sort()
        best_rank = scored[0][0]
        top = [s for s in scored if s[0] == best_rank]
        conf = ["high", "medium", "low"][best_rank]
        if len(top) > 1 and top[0][2] != "_purecall":
            conf = "folded" if best_rank == 0 else conf
        names[addr] = (top[0][2], conf, len(top), sum(len(s[3]) for s in scored))
    return names


def safe(name):
    return re.sub(r"[^A-Za-z0-9_]+", "_", name).strip("_")


def write_decomp(decomp_dir, out, names):
    src = os.path.join(decomp_dir, "decomp.c")
    fun = re.compile(r"\bFUN_([0-9a-f]{8})\b")
    head = re.compile(r"^//==== FUNC ([0-9a-f]+) (\S+)")

    def label(addr):
        n = names.get(addr)
        if not n:
            return None
        nm, conf, ntop, _ = n
        if conf == "folded":
            return f"ICF_{nm}_and_{ntop - 1}_more"
        return nm if conf in ("high", "slot") else f"{nm}_{conf}"

    def sub(m):
        lab = label(int(m.group(1), 16))
        return lab if lab else m.group(0)

    byclass = defaultdict(list)
    current = None
    os.makedirs(os.path.join(out, "byclass"), exist_ok=True)
    with open(src, errors="replace") as f, open(os.path.join(out, "decomp_named.c"), "w") as w:
        for line in f:
            h = head.match(line)
            if h:
                addr = int(h.group(1), 16)
                n = names.get(addr)
                current = None
                if n and n[1] in ("high", "medium", "slot"):
                    current = byclass[owner_of(n[0]) or "_global"]
                lab = label(addr)
                if lab:
                    line = line.replace(" " + h.group(2) + " ", f" {lab} ", 1)
            else:
                line = fun.sub(sub, line)
            w.write(line)
            if current is not None:
                current.append(line)
    for cls, lines in byclass.items():
        with open(os.path.join(out, "byclass", safe(cls) + ".c"), "w") as w:
            w.writelines(lines)
    return len(byclass)


def describe(cls, ref, ours, report):
    lines = []
    rep = report[cls]
    lines.append(f"{cls}: {rep['status']}" + (f", {rep.get('inherit_errors', 0)} inherit errors" if rep["tables"] else ""))
    o = ours.get(cls, {})
    for sub, a, b in rep.get("moved", []):
        lines.append(f"  layout: {sub} subobject at +{a:#x} in ours, +{b:#x} in the reference")
    for off, n_ours, n_ref in rep["tables"]:
        lines.append(f"  vftable +{off:#x} ({o.get(off, (None,))[0] or '?'}): ours {n_ours}, reference {n_ref}"
                     + ("" if n_ours == n_ref else "   <-- count differs"))
        rs = ref[cls].get(off, [])
        os_ = o.get(off, (None, []))[1]
        for j in range(max(len(rs), len(os_))):
            t = f"{rs[j]:08x}" if j < len(rs) else "--------"
            nm = os_[j].name if j < len(os_) else "-"
            note = rep["notes"].get((off, j), "")
            lines.append(f"    [{j:3d}] {t}  {nm}" + (f"   ! {note}" if note else ""))
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dll")
    ap.add_argument("decomp_dir")
    ap.add_argument("--dumps")
    ap.add_argument("--ours-dll")
    ap.add_argument("--ours-map")
    ap.add_argument("--out")
    ap.add_argument("--class", dest="classes", action="append")
    ap.add_argument("--no-decomp", action="store_true")
    args = ap.parse_args()
    ref, bases = ref_tables(args.dll)
    if args.ours_dll:
        ours, primary, conflicts = msvc_source(args.ours_dll, args.ours_map)
    elif args.dumps:
        ours, primary, conflicts = gcc_source(args.dumps)
    else:
        raise SystemExit("need --dumps or --ours-dll/--ours-map")
    moved = remap_offsets(ref, bases, ours)
    report = check(ref, ours, primary)
    for cls, m in moved.items():
        report[cls]["moved"] = m
    if args.classes:
        for c in args.classes:
            if c not in report:
                print(f"{c}: not in client.dll's RTTI" + ("" if c in ours else ", and not in our dumps"))
                continue
            print(describe(c, ref, ours, report))
        return
    out = args.out or args.decomp_dir
    cand = assign_names(ref, ours, report)
    names = pick(cand)
    # Slots of classes we have no layout for (Hidden's own, not yet written) still get a position
    # name, taken from the class with the fewest bases that has the address (the introducing one).
    slot_names = {}
    for cls, r in ref.items():
        for off, rs in r.items():
            for j, t in enumerate(rs):
                if t in names:
                    continue
                key = (len(bases.get(cls, [])), cls)
                if t not in slot_names or key < slot_names[t][0]:
                    suffix = f"_at_{off:x}" if off else ""
                    slot_names[t] = (key, f"{cls}::vfunc{j}{suffix}")
    for t, (key, nm) in slot_names.items():
        names[t] = (nm, "slot", 1, 1)
    with open(os.path.join(out, "names.tsv"), "w") as f:
        f.write("address\tname\tconfidence\tslots\n")
        for addr in sorted(names):
            nm, conf, ntop, uses = names[addr]
            f.write(f"{addr:08x}\t{nm}\t{conf}\t{uses}\n")
    os.makedirs(os.path.join(out, "vtables_byclass"), exist_ok=True)
    counts = defaultdict(int)
    with open(os.path.join(out, "vtable_report.tsv"), "w") as f:
        f.write("class\tstatus\tinherit_errors\tvftables (offset:ours/ref)\tlayout\n")
        for cls in sorted(report):
            rep = report[cls]
            counts[rep["status"]] += 1
            t = " ".join(f"{off:#x}:{a}/{b}" for off, a, b in rep["tables"])
            lay = " ".join(f"{sub}@{a:#x}->{b:#x}" for sub, a, b in rep.get("moved", []))
            f.write(f"{cls}\t{rep['status']}\t{rep.get('inherit_errors', '')}\t{t}\t{lay}\n")
            with open(os.path.join(out, "vtables_byclass", safe(cls) + ".txt"), "w") as g:
                g.write(describe(cls, ref, ours, report) + "\n")
    nconf = defaultdict(int)
    for v in names.values():
        nconf[v[1]] += 1
    print(f"classes: {dict(counts)}; named addresses: {len(names)} {dict(nconf)}")
    if conflicts:
        print(f"{len(conflicts)} classes had differing vtable sizes between translation units (first one used)")
    if not args.no_decomp:
        n = write_decomp(args.decomp_dir, out, names)
        print(f"decomp_named.c and {n} byclass files written to {out}")


if __name__ == "__main__":
    main()
