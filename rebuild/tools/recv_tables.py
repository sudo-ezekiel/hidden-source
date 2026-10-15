"""Extract client.dll's RecvTables and Hidden's server SendTables, and compare them table by table.

Both sides build their tables at load time, so neither binary holds them as data. They are read from
the Ghidra decompiles instead:
- client (MSVC, no symbols): every RecvTable is finished by RecvTable::Construct(props, count,
  "DT_x") on a static RecvTable (ECX); its props are written by RecvProp* helper calls
  "FUN_x(&DAT_prop, "m_name", offset, ...)" into the props array just before. The helpers are
  recognised by their bodies (the RecvProp type constant they store), wrappers (RecvPropEHandle,
  RecvPropBool, RecvPropTime...) by the helper they call. Sub-table targets of DataTable props
  are resolved through the RecvTable object each Construct call uses.
- server (GCC, unstripped): SendProp*(ServerClassInit<DT_x::ignored>::g_SendProps + n*0x4c,
  name, ...) calls; string addresses are read from the .so.
- ours (optional, --source <repo>): BEGIN_RECV_TABLE/BEGIN_NETWORK_TABLE blocks of the client side
  of our tree (cl_dll, game_shared; #ifdef CLIENT_DLL branches only), parsed from the source.

Usage:
    python3 -I recv_tables.py --client-decomp <decomp.c> --client-dll <client.dll>
        --server-decomp <decomp.c> --server-so <server.so> [--source <repo>] --out <dir>
        [--table DT_x ...]
writes client_recvtables.txt, server_sendtables.txt, (ours_recvtables.txt), recv_report.tsv
(table, status, details) and recv_byTable/<DT_x>.txt. With --table, prints those tables' comparison.

Comparison is by prop name within a table: missing on the client, extra on the client, and type
differences (SendProp types folded to the client's: Bool/EHandle/ModelIndex -> Int, Time/Angle -> Float,
QAngles -> Vector). SendPropExclude entries are server-only and skipped. Order is shown, not
enforced: the engine matches props by name.
"""
import argparse
import os
import re
import struct
import sys
from collections import defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pe_rtti  # noqa: E402

TYPES = {0: "Int", 1: "Float", 2: "Vector", 3: "String", 4: "Array", 5: "DataTable"}
FUNC = re.compile(r"^//==== FUNC ([0-9a-f]+) (\S+)", re.M)


def split_functions(text):
    out = {}
    heads = list(FUNC.finditer(text))
    for i, m in enumerate(heads):
        end = heads[i + 1].start() if i + 1 < len(heads) else len(text)
        out[m.group(1)] = (m.group(2), text[m.end():end])
    return out


CAST = re.compile(r"^\((?:\w+ )*\w+ \*+\)\s*")
STACK_ASSIGN = re.compile(r"\b\w*Stack_([0-9a-f]+)(?:\[(\d+)\])? = (.*?);")
ANY_CALL = re.compile(r"\b\w+\([^;]*\);")


def stack_args(body, start, explicit):
    """Ghidra sometimes loses track of the stack in long functions and prints a call as "FUN_x();"
    after assignments to its argument slots ("pcStack_58 = "m_name";"). Rebuild the argument list
    from the assignments since the previous call: lowest address first, the return address dropped."""
    prev = 0
    for m in ANY_CALL.finditer(body, max(0, start - 4000), start):
        prev = m.end()
    slots = {}
    for m in STACK_ASSIGN.finditer(body, prev, start):
        nn = int(m.group(1), 16) - 4 * int(m.group(2) or 0)
        slots[nn] = m.group(3).strip()
    vals = [v for nn, v in sorted(slots.items(), reverse=True)]
    if vals and re.match(r"^(?:\(\w+ \*\))?0x240[0-9a-f]{5}$|^(?:\(\w+ \*\))?0x241[0-9a-f]{5}$|^(?:\(\w+ \*\))?0x242[0-3][0-9a-f]{4}$", vals[0]):
        vals = vals[1:]
    # explicit arguments, if any, are the first ones; the slots hold the rest
    return list(explicit) + [CAST.sub("", v) for v in vals]


def calls(body, name_re, stack_fallback=False):
    """Yield (function name, [args]) for calls matching name_re, args split at top-level commas."""
    for m in name_re.finditer(body):
        i = m.end()
        depth = 1
        args = []
        cur = []
        in_str = False
        while i < len(body) and depth:
            c = body[i]
            if in_str:
                cur.append(c)
                if c == "\\":
                    cur.append(body[i + 1])
                    i += 1
                elif c == '"':
                    in_str = False
            elif c == '"':
                in_str = True
                cur.append(c)
            elif c == "(":
                depth += 1
                cur.append(c)
            elif c == ")":
                depth -= 1
                if depth:
                    cur.append(c)
            elif c == "," and depth == 1:
                args.append("".join(cur).strip())
                cur = []
            else:
                cur.append(c)
            i += 1
        args.append("".join(cur).strip())
        args = [CAST.sub("", re.sub(r"\s+", " ", a)) for a in args]
        if args == [""]:
            args = []
        if stack_fallback and len(args) < 3:
            args = stack_args(body, m.start(), args)
        yield m.group(1), args


def num(s):
    s = s.strip()
    m = re.search(r"(-?0x[0-9a-f]+|-?\d+)$", s)
    if not m:
        return None
    try:
        return int(m.group(1), 0)
    except ValueError:
        return None


def lit(s):
    m = re.match(r'^"(.*)"$', s.strip())
    return m.group(1) if m else None


# ---------------------------------------------------------------- client

def client_tables(decomp, dll):
    text = open(decomp, errors="replace").read()
    funcs = split_functions(text)
    helpers = {}
    for addr, (name, body) in funcs.items():
        m = re.search(r"local_3c\[1\] = (\d);", body)
        if m and ("iVar1 = 0xf" in body or "* 0x3c)" in body):
            t = TYPES.get(int(m.group(1)), "?")
            if "* 0x3c)" in body:
                t = "InternalArray" if t == "DataTable" else t
            helpers[addr] = t
    # RecvPropUtlVector: allocates (count+1) props and fills a UtlVector extra data block
    for addr, (name, body) in funcs.items():
        if addr not in helpers and "iVar6 = param_7 + 1;" in body and "* 0x3c)" in body:
            helpers[addr] = "UtlVector"
    changed = True
    while changed:
        changed = False
        for addr, (name, body) in funcs.items():
            if addr in helpers or len(body) > 600:
                continue
            m = re.search(r"FUN_([0-9a-f]{8})\(param_1,param_2,param_3", body)
            if m and m.group(1) in helpers:
                helpers[addr] = helpers[m.group(1)]
                changed = True
    pe = pe_rtti.Pe(dll)
    construct = None
    for addr, (name, body) in funcs.items():
        if re.search(r'FUN_[0-9a-f]+\([^;]*"DT_\w+"\);', body):
            m = re.search(r'FUN_([0-9a-f]+)\([^;]*"DT_\w+"\);', body)
            construct = m.group(1)
            break
    # RecvTable object (ECX) for each Construct call site, from the code bytes.
    table_obj = {}
    text_sec = pe.section(".text")
    tstart, traw, trs = text_sec[1], text_sec[3], text_sec[4]
    code = pe.data[traw:traw + trs]
    target = int(construct, 16)
    sites = []
    for m in re.finditer(rb"\xe8", code):
        i = m.start()
        if i + 5 > len(code):
            continue
        rel = struct.unpack_from("<i", code, i + 1)[0]
        if tstart + i + 5 + rel == target:
            sites.append(i)
    objs_in_order = []
    for i in sites:
        obj = None
        for j in range(i - 5, max(i - 64, 0), -1):
            if code[j] == 0xB9:
                obj = struct.unpack_from("<I", code, j + 1)[0]
                break
        objs_in_order.append((tstart + i, obj))
    call_re = re.compile(r"FUN_([0-9a-f]{8})\s*\(")
    tables = {}
    for addr, (name, body) in funcs.items():
        if construct not in body:
            continue
        props = []
        for fn, args in calls(body, call_re, stack_fallback=True):
            if fn in helpers and args and args[0].startswith("&DAT_"):
                props.append((int(args[0][5:], 16), fn, args))
            elif fn == construct and len(args) == 3 and table_name(args[2], pe):
                base = int(args[0][5:], 16) if args[0].startswith("&DAT_") else None
                n = num(args[1])
                tname = table_name(args[2], pe)
                plist = []
                for paddr, pfn, pargs in props:
                    if base is not None and n is not None and base <= paddr < base + n * 0x3c:
                        plist.append(((paddr - base) // 0x3c, pfn, pargs))
                plist.sort()
                tables[tname] = {"func": addr, "props_addr": base, "count": n,
                                 "props": [client_prop(i, helpers[f], a, pe) for i, f, a in plist]}
    # sub-table names: which RecvTable object each Construct call used
    obj_name = {}
    by_func = defaultdict(list)
    for t, info in tables.items():
        by_func[info["func"]].append(t)
    starts = sorted(int(a, 16) for a in funcs)
    import bisect
    for site, obj in objs_in_order:
        k = bisect.bisect_right(starts, site) - 1
        f = f"{starts[k]:08x}"
        names = by_func.get(f, [])
        if len(names) == 1 and obj:
            obj_name[obj] = names[0]
    for t, info in tables.items():
        for p in info["props"]:
            if p.get("table_obj") is not None:
                p["table"] = obj_name.get(p["table_obj"], f"DAT_{p['table_obj']:08x}")
    return tables, helpers, construct


def string_arg(args, pe):
    """The prop name: the first string literal after the return slot, else a &DAT_ string."""
    for a in args[1:]:
        if lit(a) is not None:
            return lit(a)
    m = re.match(r"^(?:&?DAT_|0x)([0-9a-f]{8})$", args[1]) if len(args) > 1 else None
    if m:
        v = pe.cstr(int(m.group(1), 16))
        if v:
            return v
    return args[1] if len(args) > 1 else "?"


def table_name(arg, pe):
    v = lit(arg)
    if v is None:
        m = re.search(r"(?:DAT_|0x)([0-9a-f]{8})\b", arg)
        v = pe.cstr(int(m.group(1), 16)) if m else None
    return v if v and v.startswith("DT_") else None


def client_prop(index, kind, args, pe):
    p = {"index": index, "name": string_arg(args, pe), "type": kind,
         "offset": num(args[2]) if len(args) > 2 and kind != "Array" else None}
    if kind == "Array" and len(args) > 1:
        p["elements"] = num(args[1])
    if kind == "DataTable" and len(args) > 4:
        m = re.match(r"&DAT_([0-9a-f]+)", args[4])
        p["table_obj"] = int(m.group(1), 16) if m else None
    if kind == "InternalArray" and len(args) > 4:
        p["elements"] = num(args[4])
    if kind == "UtlVector" and len(args) > 6:
        p["elements"] = num(args[6])
    return p


# ---------------------------------------------------------------- server

class Elf:
    def __init__(self, path):
        self.data = open(path, "rb").read()
        d = self.data
        shoff = struct.unpack_from("<I", d, 0x20)[0]
        shentsize, shnum = struct.unpack_from("<HH", d, 0x2E)
        self.secs = []
        for i in range(shnum):
            o = shoff + i * shentsize
            name, typ, flags, addr, off, size = struct.unpack_from("<IIIIII", d, o)
            if addr:
                self.secs.append((addr, off, size))
        self.shdrs = [struct.unpack_from("<IIIIIIIIII", d, shoff + i * shentsize) for i in range(shnum)]

    def symbols(self):
        out = []
        for sh in self.shdrs:
            if sh[1] != 2:  # SHT_SYMTAB
                continue
            strtab = self.shdrs[sh[6]]
            for o in range(sh[4], sh[4] + sh[5], 16):
                name, value, size, info, other, shndx = struct.unpack_from("<IIIBBH", self.data, o)
                e = self.data.find(b"\0", strtab[4] + name)
                out.append((self.data[strtab[4] + name:e].decode("latin-1"), value, size))
        return out

    def cstr(self, va):
        for addr, off, size in self.secs:
            if addr <= va < addr + size:
                o = off + va - addr
                e = self.data.find(b"\0", o)
                return self.data[o:e].decode("latin-1")
        return None


SEND_FOLD = {"Bool": "Int", "EHandle": "Int", "ModelIndex": "Int", "IntWithMinusOneFlag": "Int",
             "PredictableId": "Int", "Time": "Float", "Angle": "Float", "QAngles": "Vector",
             "StringT": "String", "": "?"}


def server_tables(decomp, so, base=0x10000):
    text = open(decomp, errors="replace").read()
    elf = Elf(so)
    # g_SendProps arrays by address (Ghidra address = symbol value + base)
    import subprocess
    syms = [(n, v, sz) for n, v, sz in elf.symbols() if "g_SendProps" in n and "ServerClassInit" in n
            and not n.startswith("_ZGV")]
    dem = subprocess.run(["c++filt"], input="\n".join(n for n, v, sz in syms), capture_output=True,
                         text=True).stdout.split("\n")
    ranges = []
    for (n, v, sz), d in zip(syms, dem):
        m = re.search(r"ServerClassInit<(\w+)::ignored>", d)
        if m:
            ranges.append((v + base, v + base + max(sz, 0x4c), m.group(1)))
    call_re = re.compile(r"\b(?:Internal)?SendProp(\w*)\s*\(")
    owner = re.compile(r"ServerClassInit<(\w+)::ignored>.*?g_SendProps(?: \+ (0x[0-9a-f]+|\d+))?")
    tables = defaultdict(dict)
    for kind, args in calls(text, call_re):
        if not args:
            continue
        m = owner.search(args[0])
        if m:
            t = m.group(1)
            off = int(m.group(2), 0) if m.group(2) else 0
        else:
            dm = re.search(r"DAT_([0-9a-f]{8})$", args[0])
            v = int(dm.group(1), 16) if dm else num(args[0])
            hit = [r for r in ranges if v is not None and r[0] <= v < r[1]]
            if not hit:
                continue
            t = hit[0][2]
            off = v - hit[0][0]
        idx = off // 0x4c
        nm = None
        for a in args[1:]:
            q = re.search(r'"((?:[^"\\]|\\.)*)"', a)
            if q:
                nm = q.group(1)
                break
        if nm is None and len(args) > 1:
            v = num(args[1])
            if v is not None:
                nm = elf.cstr(v - base) or elf.cstr(v)
        if kind == "Exclude":
            tables[t][idx] = {"index": idx, "name": f"(exclude {nm})", "type": "Exclude"}
            continue
        p = {"index": idx, "name": nm or args[1], "type": SEND_FOLD.get(kind, kind), "send": kind or "Prop"}
        if kind == "Array" and len(args) > 1:
            p["elements"] = num(args[1])
        tables[t][idx] = p
    return {t: {"props": [p for i, p in sorted(v.items())]} for t, v in tables.items()}


# ---------------------------------------------------------------- our source

BEGIN = re.compile(r"\b(?:BEGIN_(?:RECV|NETWORK)_TABLE(?:_NOBASE)?|IMPLEMENT_CLIENTCLASS(?:_EVENT)?_DT(?:_NOBASE)?)"
                   r"\s*\(\s*(\w+)\s*,\s*(\w+)\s*[,)]")
END = re.compile(r"\bEND_(?:RECV|NETWORK)_TABLE\b")
RPROP = re.compile(r"\bRecvProp(\w*)\s*\(")


def client_only(text):
    """Keep the CLIENT_DLL side of #ifdef CLIENT_DLL / #ifndef CLIENT_DLL / GAME_DLL blocks."""
    out = []
    stack = []
    for line in text.split("\n"):
        s = line.strip()
        m = re.match(r"#\s*(ifdef|ifndef|if|else|endif|elif)\b(.*)", s)
        if m:
            d, rest = m.group(1), m.group(2).strip()
            if d in ("ifdef", "ifndef", "if"):
                val = None
                if "CLIENT_DLL" in rest:
                    val = (d != "ifndef") and not rest.startswith("!")
                elif "GAME_DLL" in rest:
                    val = (d == "ifndef") or rest.startswith("!")
                stack.append(val)
            elif d == "else" and stack:
                stack[-1] = None if stack[-1] is None else not stack[-1]
            elif d == "elif" and stack:
                stack[-1] = None if stack[-1] is None else False
            elif d == "endif" and stack:
                stack.pop()
            out.append("")
            continue
        out.append(line if all(v is not False for v in stack) else "")
    return "\n".join(out)


def recvinfo_name(arg):
    m = re.match(r"RECVINFO_NAME\s*\(\s*[^,]+?\s*,\s*([\w\[\]]+)\s*\)", arg)
    if m:
        return m.group(1)
    m = re.match(r"RECVINFO\s*\(\s*(.+?)\s*\)$", arg)
    if m:
        return m.group(1).replace(" ", "")
    m = re.match(r"RECVINFO_(?:DT|ARRAY|UTLVECTOR|INTERNALARRAY|ARRAY3)\w*\s*\(\s*(\w+)", arg)
    if m:
        return m.group(1)
    return lit(arg) or arg


def source_files(repo, build_files):
    if build_files:
        with open(build_files) as f:
            return [os.path.join(repo, line.split("\t")[0]) for line in f if line.strip()]
    out = []
    for top in ("cl_dll", "game_shared"):
        for dirpath, dirs, files in os.walk(os.path.join(repo, top)):
            out.extend(os.path.join(dirpath, fn) for fn in files if fn.endswith(".cpp"))
    return out


def source_tables(repo, build_files=None):
    tables = {}
    if True:
        for path in source_files(repo, build_files):
            if True:
                text = client_only(open(path, encoding="latin-1").read())
                for m in BEGIN.finditer(text):
                    e = END.search(text, m.end())
                    if not e:
                        continue
                    body = text[m.end():e.start()]
                    props = []
                    if "NOBASE" not in m.group(0):
                        props.append({"index": 0, "name": "baseclass", "type": "DataTable"})
                    for kind, args in calls(body, RPROP):
                        if not args or not args[0]:
                            continue
                        nm = recvinfo_name(args[0])
                        if nm.startswith("RecvProp"):
                            if kind == "Array" and len(args) > 1:
                                nm = args[1]
                            else:
                                continue
                        props.append({"index": len(props), "name": nm,
                                      "type": {"": "?", "EHandle": "Int", "Bool": "Int", "Time": "Float",
                                               "QAngles": "Vector", "IntWithMinusOneFlag": "Int",
                                               "PredictableId": "DataTable", "Array3": "Array"}.get(kind, kind)})
                    t = m.group(2)
                    if t not in tables:
                        tables[t] = {"props": props, "file": os.path.relpath(path, repo)}
    # arrays: keep the array prop, drop its "[0]" element prop (the binaries fold it the same way)
    for t in tables.values():
        arrays = {p["name"] for p in t["props"] if p["type"] in ("Array", "Array3", "InternalArray")}
        t["props"] = [p for p in t["props"] if not (p["name"].endswith("[0]") and p["name"][:-3] in arrays)]
        for i, p in enumerate(t["props"]):
            p["index"] = i
    return tables


# ---------------------------------------------------------------- compare

FOLD_CLIENT = {"InternalArray": "Array", "Array3": "Array", "UtlVector": "UtlVector"}


def comparable(t):
    t = FOLD_CLIENT.get(t, t)
    return t


def significant(props):
    """Props to compare: no excludes, no dummy, and an array's element prop folded into the array."""
    out = []
    for i, p in enumerate(props):
        if p["type"] == "Exclude" or p["name"] == "should_never_see_this":
            continue
        if i + 1 < len(props) and props[i + 1]["type"] == "Array":
            continue
        out.append(p)
    return out


def compare(a_tab, b_tab, a_name, b_name):
    rows = []
    for t in sorted(set(a_tab) | set(b_tab)):
        a = a_tab.get(t)
        b = b_tab.get(t)
        if a is None:
            rows.append((t, f"only-{b_name}", f"{len(b['props'])} props"))
            continue
        if b is None:
            rows.append((t, f"only-{a_name}", f"{len(a['props'])} props"))
            continue
        pa = {p["name"]: p for p in significant(a["props"])}
        pb = {p["name"]: p for p in significant(b["props"])}
        missing = [n for n in pb if n not in pa]
        extra = [n for n in pa if n not in pb]
        types = [f"{n} {comparable(pa[n]['type'])}/{comparable(pb[n]['type'])}" for n in pa
                 if n in pb and comparable(pa[n]["type"]) != comparable(pb[n]["type"])
                 and "?" not in (pa[n]["type"], pb[n]["type"])
                 and {comparable(pa[n]["type"]), comparable(pb[n]["type"])} not in ({"Array", "DataTable"}, {"UtlVector", "DataTable"}, {"Array", "Int"}, {"Array", "Float"})]
        if not missing and not extra and not types:
            rows.append((t, "MATCH", f"{len(pa)} props"))
        else:
            d = []
            if missing:
                d.append(f"not in {a_name}: " + " ".join(missing))
            if extra:
                d.append(f"not in {b_name}: " + " ".join(extra))
            if types:
                d.append("type: " + " ".join(types))
            rows.append((t, "DIFF", "; ".join(d)))
    return rows


def dump(tables, path):
    with (open(path, "w") if path else open(sys.stdout.fileno(), "w", closefd=False)) as f:
        for t in sorted(tables):
            info = tables[t]
            f.write(f"{t}\n")
            for p in info["props"]:
                extra = ""
                if p.get("table"):
                    extra += f" -> {p['table']}"
                if p.get("elements"):
                    extra += f" [{p['elements']}]"
                if p.get("offset") is not None:
                    extra += f" @{p['offset']:#x}"
                if p.get("send"):
                    extra += f" (SendProp{p['send']})"
                f.write(f"  {p['index']:3d} {p['type']:<13} {p['name']}{extra}\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--client-decomp", required=True)
    ap.add_argument("--client-dll", required=True)
    ap.add_argument("--server-decomp", required=True)
    ap.add_argument("--server-so", required=True)
    ap.add_argument("--source")
    ap.add_argument("--build-files", help="client.mk.files from client_check.sh: only scan the files in the build")
    ap.add_argument("--out")
    ap.add_argument("--table", action="append")
    args = ap.parse_args()
    client, helpers, construct = client_tables(args.client_decomp, args.client_dll)
    server = server_tables(args.server_decomp, args.server_so)
    ours = source_tables(args.source, args.build_files) if args.source else None
    rows = compare(client, server, "client", "server")
    rows_ours = compare(ours, client, "ours", "client") if ours is not None else []
    if args.table:
        for t in args.table:
            for label, tabs in (("client.dll", client), ("server .so", server), ("our source", ours or {})):
                if t in tabs:
                    sys.stdout.flush()
                    dump({t: tabs[t]}, None)
                    print(f"  ({label})", flush=True)
            for r in rows + rows_ours:
                if r[0] == t:
                    print("\t".join(r))
        return
    out = args.out
    os.makedirs(out, exist_ok=True)
    dump(client, os.path.join(out, "client_recvtables.txt"))
    dump(server, os.path.join(out, "server_sendtables.txt"))
    with open(os.path.join(out, "recv_report.tsv"), "w") as f:
        f.write("# client.dll RecvTables against Hidden's server SendTables\ntable\tstatus\tdetails\n")
        for r in rows:
            f.write("\t".join(r) + "\n")
    summary = defaultdict(int)
    for r in rows:
        summary[r[1]] += 1
    print(f"client tables {len(client)}, server tables {len(server)}; client vs server: {dict(summary)}")
    print("RecvProp helpers found: " + ", ".join(f"{a}={t}" for a, t in sorted(helpers.items())) +
          f"; RecvTable::Construct {construct}")
    if ours is not None:
        dump(ours, os.path.join(out, "ours_recvtables.txt"))
        with open(os.path.join(out, "recv_report_ours.tsv"), "w") as f:
            f.write("# our client source RecvTables against client.dll's\ntable\tstatus\tdetails\n")
            for r in rows_ours:
                f.write("\t".join(r) + "\n")
        s2 = defaultdict(int)
        for r in rows_ours:
            s2[r[1]] += 1
        print(f"our source tables {len(ours)}; ours vs client.dll: {dict(s2)}")


if __name__ == "__main__":
    main()
