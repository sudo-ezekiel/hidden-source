"""Parse GCC 3.4 -fdump-class-hierarchy output and derive MSVC 7.1 vftable layouts from it.

GCC (Itanium ABI) and MSVC lay out virtual tables differently:
- GCC gives a virtual destructor two slots (complete and deleting); MSVC one (scalar deleting).
- A GCC primary vtable also gets an entry for every function that overrides a virtual of a
  secondary base; MSVC only has it in that base's vftable.
- MSVC groups the overloads of one name that a class introduces at the first one's position, in
  reverse declaration order; GCC keeps declaration order.
Everything else (declaration order, one vftable per polymorphic base subobject) is shared.

    load_dumps(paths) -> {class name: ClassInfo}
    msvc_vftables(classes, name) -> [(subobject offset, subobject class, [Slot])]

Slot.name is "Class::method" for the implementing function, Slot.kind is one of
"method", "dtor", "pure", "thunk".
"""
import re
import subprocess
from collections import namedtuple

Slot = namedtuple("Slot", "name kind gcc_index")

VT_HEAD = re.compile(r"^Vtable for (.+)$")
VT_COUNT = re.compile(r"^(.+)::_ZTV\S+: (\d+)u entries$")
VT_ENTRY = re.compile(r"^(\d+)\s+(.*)$")
CL_HEAD = re.compile(r"^Class (.+)$")
CL_NODE = re.compile(r"^( *)(\S.*?) \(0x[0-9a-f]+\) (\d+)( .*)?$")
CL_VPTR = re.compile(r"vptr=\(\(&.*?\) \+ (\d+)u\)")
CL_PRIMARY = re.compile(r"primary-for (.+?) \(0x")


class ClassInfo:
    def __init__(self, name):
        self.name = name
        self.entries = []  # vtable words after the class's own offset-to-top/RTTI pair, all groups
        self.raw = []      # every vtable word, index = word
        self.nodes = []    # (depth, name, offset, vptr word or None, primary_for or None)
        self.source = None

    def primary_base(self):
        for depth, name, off, vptr, pf in self.nodes:
            if pf == self.name and off == 0:
                return name
        return None

    def groups(self):
        """[(offset, class, first word, word count)] for the primary vtable and each secondary vptr."""
        starts = []
        for depth, name, off, vptr, pf in self.nodes:
            if vptr is not None:
                starts.append((vptr // 4, off, name))
        starts.sort()
        out = []
        for i, (w, off, name) in enumerate(starts):
            end = starts[i + 1][0] - 2 if i + 1 < len(starts) else len(self.raw)
            out.append((off, name, w, end - w))
        return out


def load_dumps(paths):
    classes = {}
    conflicts = {}
    for path in paths:
        with open(path, errors="replace") as f:
            lines = f.read().split("\n")
        i = 0
        n = len(lines)
        while i < n:
            line = lines[i]
            m = VT_HEAD.match(line)
            if m:
                name = m.group(1)
                words = []
                i += 2
                while i < n and lines[i]:
                    e = VT_ENTRY.match(lines[i])
                    if e:
                        words.append(e.group(2))
                    i += 1
                ci = classes.get(name)
                if ci is None:
                    ci = classes[name] = ClassInfo(name)
                    ci.source = path
                if not ci.raw:
                    ci.raw = words
                elif len(ci.raw) != len(words):
                    conflicts.setdefault(name, set()).update({len(ci.raw), len(words)})
                continue
            m = CL_HEAD.match(line)
            if m:
                name = m.group(1)
                nodes = []
                i += 3
                while i < n and lines[i]:
                    nm = CL_NODE.match(lines[i])
                    if nm:
                        nodes.append([len(nm.group(1)), nm.group(2), int(nm.group(3)), None, None])
                    else:
                        v = CL_VPTR.search(lines[i])
                        if v and nodes:
                            nodes[-1][3] = int(v.group(1))
                        p = CL_PRIMARY.search(lines[i])
                        if p and nodes:
                            nodes[-1][4] = p.group(1)
                    i += 1
                ci = classes.get(name)
                if ci is None:
                    ci = classes[name] = ClassInfo(name)
                    ci.source = path
                if not ci.nodes:
                    ci.nodes = [tuple(x) for x in nodes]
                continue
            i += 1
    return classes, conflicts


_demangled = {}


def demangle_all(names):
    todo = sorted({n for n in names if n not in _demangled})
    if not todo:
        return
    out = subprocess.run(["c++filt"], input="\n".join(todo), capture_output=True, text=True).stdout.split("\n")
    for a, b in zip(todo, out):
        _demangled[a] = b


THUNK = re.compile(r"^(?:non-virtual|virtual) thunk to (.*?)(\(.*)?$")


def entry_name(word):
    """('Class::method', kind) for one GCC vtable word."""
    if word == "__cxa_pure_virtual":
        return None, "pure"
    if "::_ZT" in word:
        mangled = word[word.index("::_ZT") + 2:]
        d = _demangled.get(mangled, mangled)
        m = THUNK.match(d)
        return (m.group(1) if m else d), "thunk"
    if "::~" in word:
        return word, "dtor"
    return word, "method"


def method_of(qualified):
    """'vgui::Panel::SetBgColor' -> 'SetBgColor' (also for destructors)."""
    if qualified is None:
        return None
    depth = 0
    for i in range(len(qualified) - 1, 0, -1):
        c = qualified[i]
        if c == ">":
            depth += 1
        elif c == "<":
            depth -= 1
        elif c == ":" and qualified[i - 1] == ":" and depth == 0:
            return qualified[i + 1:]
    return qualified


class Layouts:
    """MSVC order of each class's primary vtable, as indices into its GCC primary vtable."""

    def __init__(self, classes):
        self.classes = classes
        words = [w for c in classes.values() for w in c.raw if "::_ZT" in w]
        demangle_all(w[w.index("::_ZT") + 2:] for w in words)
        self._order = {}
        self._pure_names = {}
        self._fill_pure_names()

    def primary_words(self, name):
        c = self.classes.get(name)
        if c is None or not c.raw:
            return None
        g = c.groups()
        if not g:
            return None
        off, cls, start, count = g[0]
        return c.raw[start:start + count]

    def _fill_pure_names(self):
        # A pure virtual's name comes from any class that has a group for that base.
        for c in self.classes.values():
            if not c.raw or not c.nodes:
                continue
            for off, sub, start, count in c.groups():
                words = c.raw[start:start + count]
                for idx, w in enumerate(words):
                    nm, kind = entry_name(w)
                    if nm:
                        self._pure_names.setdefault((sub, idx), method_of(nm))
            # the primary chain shares the primary group
            words = self.primary_words(c.name) or []
            b = c.primary_base()
            seen = set()
            while b and b not in seen:
                seen.add(b)
                bw = self.primary_words(b)
                if bw is None:
                    break
                for idx in range(min(len(bw), len(words))):
                    nm, kind = entry_name(words[idx])
                    if nm:
                        self._pure_names.setdefault((b, idx), method_of(nm))
                cb = self.classes.get(b)
                b = cb.primary_base() if cb else None

    def slot_method(self, cls, idx, word):
        nm, kind = entry_name(word)
        if nm:
            return method_of(nm)
        return self._pure_names.get((cls, idx))

    def secondary_methods(self, name):
        """Method names in the secondary groups of class `name`. A new primary entry with one of
        these names overrides a secondary base's virtual: GCC repeats it in the primary vtable
        (as a thunk target, or as __cxa_pure_virtual when the override is pure), MSVC does not."""
        c = self.classes[name]
        out = set()
        for off, sub, start, count in c.groups()[1:]:
            for idx, w in enumerate(c.raw[start:start + count]):
                m = self.slot_method(sub, idx, w)
                if m:
                    out.add(m)
        return out

    def order(self, name, _stack=()):
        if name in self._order:
            return self._order[name]
        words = self.primary_words(name)
        if words is None:
            return None
        c = self.classes[name]
        b = c.primary_base()
        base_order = []
        base_len = 0
        if b and b not in _stack:
            bo = self.order(b, _stack + (name,))
            bw = self.primary_words(b)
            if bo is not None and bw is not None:
                base_order = list(bo)
                base_len = len(bw)
        extras = self.secondary_methods(name)
        new = []
        i = base_len
        while i < len(words):
            nm, kind = entry_name(words[i])
            if kind == "dtor":
                new.append((i, "~"))
                i += 2  # complete + deleting destructor -> one scalar deleting destructor
                continue
            m = self.slot_method(name, i, words[i])
            if not (kind in ("method", "pure") and m in extras):
                new.append((i, m))
            i += 1
        # MSVC: overloads introduced together are grouped at the first one, in reverse order.
        groups = []
        index = {}
        for i, m in new:
            if m is None or m == "~":
                groups.append([i])
                continue
            if m in index:
                groups[index[m]].append(i)
            else:
                index[m] = len(groups)
                groups.append([i])
        ordered = base_order + [i for g in groups for i in reversed(g)]
        self._order[name] = ordered
        return ordered

    def vftables(self, name):
        """[(offset, subobject class, [Slot])] in MSVC order for class `name`."""
        c = self.classes.get(name)
        if c is None or not c.raw or not c.nodes:
            return None
        out = []
        for gi, (off, sub, start, count) in enumerate(c.groups()):
            words = c.raw[start:start + count]
            if gi == 0:
                order = self.order(name)
                owner = name
            else:
                order = self.order(sub)
                owner = sub
            if order is None:
                order = list(range(len(words)))
            slots = []
            for idx in order:
                if idx >= len(words):
                    continue
                nm, kind = entry_name(words[idx])
                if kind == "pure":
                    m = self._pure_names.get((owner, idx))
                    nm = f"{owner}::{m}" if m else f"{owner}::pure_{idx}"
                slots.append(Slot(nm, kind, idx))
            out.append((off, sub if gi else name, slots))
        return out
