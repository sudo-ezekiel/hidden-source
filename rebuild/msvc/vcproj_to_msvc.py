"""Write a makefile that builds a Visual Studio 2003 .vcproj configuration with cl.exe and link.exe 7.1.

The compiles run under Wine from the project's directory, as the IDE ran them. What is read from the
.vcproj (the file rules are vcproj_to_make.py's):
- every <File> ending in .cpp, unless a <FileConfiguration> for the configuration sets
  ExcludedFromBuild="TRUE"; objects are listed in project order, which is the IDE's link order;
- the configuration's VCCLCompilerTool, as cl.exe switches (SWITCHES below; WIN32 and _WIN32 stay
  defined, this is the Windows build);
- per file: UsePrecompiledHeader (0 none, 1 create, 3 use) and ObjectFile
  ($(IntDir)/$(InputName)1.obj for sdk/c_te_firebullets.cpp);
- VCLinkerTool for a DLL, VCLibrarianTool for a static library (link /lib, the Toolkit has no
  lib.exe).

Debug information: the IDE compiled everything into one vc70.pdb (/Zi /Fd$(IntDir)/), which
cl.exe 7.1 cannot share between parallel compiles, and a /Yu compile must name the same PDB as the
/Yc compile that made the precompiled header. So the precompiled header is created once per job
slot (pch<N>.pch with pch<N>.pdb), every file that uses it is assigned a slot, and compiles of one
slot run one at a time under flock. Files without the precompiled header get a PDB of their own.

/FR (browse information) and /FD (IDE dependency tracking) are left out: they write side files only.

Usage:
    python3 -I vcproj_to_msvc.py <vcproj> "<Config|Win32>" <out dir> <out makefile>
        --host-root <source tree> [--kind dll|lib] [--slots N] [--lib <file.lib> ...] [--extra-cl SWITCH ...]

<out dir> receives the objects, logs and the DLL or .lib. --lib replaces the project's own .lib
inputs (by file name) with the given files; any project .lib not replaced is an error. Make variables
MSCL, MSLINK and FLOCK must be set on the make command line (build_client.sh does). They are not
called CL and LINK because make exports command-line variables, and cl.exe and link.exe read
environment variables of those names as extra options.
"""
import argparse
import os
import posixpath
import sys
import xml.etree.ElementTree as ET

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools"))
from vcproj_to_make import fix_case, norm  # noqa: E402

# VCCLCompilerTool attribute -> {value: switch}; VS 2003 enumerations.
SWITCHES = {
    "Optimization": {"0": "/Od", "1": "/O1", "2": "/O2", "3": "/Ox"},
    "GlobalOptimizations": {"TRUE": "/Og"},
    # 0 is the IDE's "Default": no switch, so /Ox's own inlining applies (MSVC.md).
    "InlineFunctionExpansion": {"1": "/Ob1", "2": "/Ob2"},
    "EnableIntrinsicFunctions": {"TRUE": "/Oi"},
    "FavorSizeOrSpeed": {"1": "/Ot", "2": "/Os"},
    "OmitFramePointers": {"TRUE": "/Oy"},
    "OptimizeForProcessor": {"1": "/G5", "2": "/G6", "3": "/G7"},
    "StringPooling": {"TRUE": "/GF"},
    "MinimalRebuild": {},
    "ExceptionHandling": {"TRUE": "/EHsc"},
    "BasicRuntimeChecks": {"3": "/RTC1"},
    "RuntimeLibrary": {"0": "/MT", "1": "/MTd", "2": "/MD", "3": "/MDd", "4": "/ML", "5": "/MLd"},
    "BufferSecurityCheck": {"TRUE": "/GS"},
    "EnableFunctionLevelLinking": {"TRUE": "/Gy"},
    "RuntimeTypeInfo": {"TRUE": "/GR"},
    "WarningLevel": {"0": "/W0", "1": "/W1", "2": "/W2", "3": "/W3", "4": "/W4"},
    "DebugInformationFormat": {"1": "/Z7", "3": "/Zi", "4": "/ZI"},
    "CompileAs": {"1": "/TC", "2": "/TP"},
}
IGNORED = {"Name", "AdditionalIncludeDirectories", "PreprocessorDefinitions", "UsePrecompiledHeader",
           "PrecompiledHeaderThrough", "PrecompiledHeaderFile", "AssemblerListingLocation", "ObjectFile",
           "ProgramDataBaseFileName", "BrowseInformation", "BrowseInformationFile", "SuppressStartupBanner"}

# What the IDE's project defaults add to every link ($(NoInherit) not given). odbc32.lib and
# odbccp32.lib are part of that set but not in the Platform SDK tree; nothing in the client uses
# them, and an import library adds only what is referenced.
DEFAULT_LIBS = ["kernel32.lib", "user32.lib", "gdi32.lib", "winspool.lib", "comdlg32.lib", "advapi32.lib",
                "shell32.lib", "ole32.lib", "oleaut32.lib", "uuid.lib"]


def win(path):
    """Unix path -> the Windows path Wine gives it (Z: is the Unix root)."""
    return "Z:" + os.path.abspath(path).replace("/", "\\")


def write_if_changed(path, text):
    """Response files are make prerequisites: keep their time stamp when nothing changed."""
    if os.path.exists(path):
        with open(path) as f:
            if f.read() == text:
                return
    with open(path, "w") as f:
        f.write(text)


def q(arg):
    return "'" + arg.replace("'", "'\\''") + "'"


def config_of(root, name):
    for c in root.iter("Configuration"):
        if c.get("Name") == name:
            return c
    raise SystemExit(f"configuration {name!r} not found")


def tool(elem, name):
    return next((t for t in elem.iter("Tool") if t.get("Name") == name), None)


def cl_switches(cl):
    out = []
    for attr, value in cl.attrib.items():
        if attr in IGNORED:
            continue
        if attr not in SWITCHES:
            raise SystemExit(f"VCCLCompilerTool {attr}={value!r}: no switch mapping")
        sw = SWITCHES[attr].get(value.upper() if value.upper() in ("TRUE", "FALSE") else value)
        if sw:
            out.append(sw)
    for i in (cl.get("AdditionalIncludeDirectories") or "").replace(",", ";").split(";"):
        if i:
            out.append(f'/I "{i}"')
    for d in (cl.get("PreprocessorDefinitions") or "").split(";"):
        if d:
            out.append(f'/D "{d}"')
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("vcproj")
    ap.add_argument("config")
    ap.add_argument("outdir")
    ap.add_argument("makefile")
    ap.add_argument("--host-root", required=True)
    ap.add_argument("--kind", choices=("dll", "lib"), default="dll")
    ap.add_argument("--slots", type=int, default=6)
    ap.add_argument("--lib", action="append", default=[], help="use this file for the project .lib of the same name")
    ap.add_argument("--extra-cl", action="append", default=[], help="add this cl.exe switch to every compile")
    args = ap.parse_args()

    with open(args.vcproj, "rb") as f:
        data = f.read().decode("latin-1").replace('encoding="Windows-1252"', "")
    root = ET.fromstring(data)
    config = config_of(root, args.config)
    cl = tool(config, "VCCLCompilerTool")
    base = cl_switches(cl) + args.extra_cl
    pch_mode = cl.get("UsePrecompiledHeader") or "0"
    through = cl.get("PrecompiledHeaderThrough") or "stdafx.h"
    proj_dir = os.path.dirname(os.path.abspath(args.vcproj))
    out = os.path.abspath(args.outdir)
    name = os.path.splitext(os.path.basename(args.vcproj))[0]

    files = []  # (source relative to the project, object name, pch mode)
    proj_libs = []
    for f in root.iter("File"):
        rel = norm(f.get("RelativePath") or "")
        excluded = False
        mode = pch_mode
        obj = None
        for fc in f.iter("FileConfiguration"):
            if fc.get("Name") != args.config:
                continue
            if (fc.get("ExcludedFromBuild") or "").upper() == "TRUE":
                excluded = True
            t = tool(fc, "VCCLCompilerTool")
            if t is not None:
                mode = t.get("UsePrecompiledHeader", mode)
                obj = t.get("ObjectFile", obj)
        if excluded:
            continue
        if rel.lower().endswith(".lib"):
            proj_libs.append(rel)
            continue
        if not rel.lower().endswith(".cpp"):
            continue
        rel = fix_case(proj_dir, posixpath.normpath(rel))
        stem = posixpath.splitext(posixpath.basename(rel))[0]
        if obj:
            obj = posixpath.basename(norm(obj.replace("$(IntDir)", "").replace("$(InputName)", stem)))
        else:
            obj = stem + ".obj"
        files.append((rel, obj, mode))

    objs = [o for _, o, _ in files]
    dup = {o for o in objs if objs.count(o) > 1}
    if dup:
        raise SystemExit(f"object names collide: {sorted(dup)}")
    creators = [f for f in files if f[2] == "1"]
    if pch_mode == "3" and len(creators) != 1:
        raise SystemExit(f"expected one file that creates the precompiled header, found {len(creators)}")

    os.makedirs(out, exist_ok=True)
    rsp = os.path.join(out, "cl.rsp")
    write_if_changed(rsp, "/nologo /c " + " ".join(base) + "\n")

    slots = max(1, args.slots)
    lines = [
        f"# Generated by rebuild/msvc/vcproj_to_msvc.py from {os.path.basename(args.vcproj)}, {args.config}",
        f"PROJ={proj_dir}",
        f"OUT={out}",
        "CC_LOG=> $@.log 2>&1 || { rm -f $@; echo FAIL $<; exit 1; }",
        "",
    ]
    def compile_rule(obj, src, deps, extra, lock=None):
        """make rule for one cl.exe run; Windows arguments are single-quoted for the shell."""
        args = [f"@{win(rsp)}"] + extra + [f"/Fo{win(out)}\\{obj}", src.replace("/", "\\")]
        flock = f"$(FLOCK) $(OUT)/{lock} " if lock else ""
        return (f"$(OUT)/{obj}: $(PROJ)/{src} $(OUT)/cl.rsp{deps}\n"
                f"\t@cd $(PROJ) && {flock}$(MSCL) {' '.join(q(a) for a in args)} $(CC_LOG)\n")

    rules = []
    pch_objs = []
    if pch_mode == "3":
        src, obj, _ = creators[0]
        for s in range(slots):
            o = f"pch{s}.obj" if s else obj
            pch_objs.append(o)
            rules.append(compile_rule(o, src, "", [f"/Yc{through}", f"/Fp{win(out)}\\pch{s}.pch",
                                                   f"/Fd{win(out)}\\pch{s}.pdb"], f"pch{s}.lock"))
    n = 0
    for src, obj, mode in files:
        if mode == "1":
            continue
        if mode == "3":
            s = n % slots
            n += 1
            rules.append(compile_rule(obj, src, f" $(OUT)/{pch_objs[s]}",
                                      [f"/Yu{through}", f"/Fp{win(out)}\\pch{s}.pch",
                                       f"/Fd{win(out)}\\pch{s}.pdb"], f"pch{s}.lock"))
        else:
            rules.append(compile_rule(obj, src, "", [f"/Fd{win(out)}\\{posixpath.splitext(obj)[0]}.pdb"]))

    all_objs = " ".join(f"$(OUT)/{o}" for o in objs)
    lines.append(f"OBJS={all_objs}")
    lines.append("objs: $(OBJS)")

    # Project .lib inputs, replaced by the files given with --lib.
    given = {os.path.basename(p).lower(): p for p in args.lib}
    lib_inputs = []
    for rel in proj_libs:
        b = posixpath.basename(rel).lower()
        if b not in given:
            raise SystemExit(f"project input {rel} has no --lib file")
        lib_inputs.append(win(given[b]))

    link_rsp = os.path.join(out, "link.rsp")
    if args.kind == "lib":
        lt = tool(config, "VCLibrarianTool")
        target = os.path.join(out, posixpath.basename(norm(lt.get("OutputFile") or f"{name}.lib")))
        # /lib must be the first switch on link.exe's own command line, not in the response file.
        link = ["/nologo", f'/OUT:"{win(target)}"']
    else:
        lt = tool(config, "VCLinkerTool")
        target = os.path.join(out, posixpath.basename(norm(lt.get("OutputFile") or f"{name}.dll")))
        stem = os.path.splitext(target)[0]
        link = [f'/OUT:"{win(target)}"', "/NOLOGO", "/DLL"]
        if lt.get("LinkIncremental") == "1":
            link.append("/INCREMENTAL:NO")
        for d in (lt.get("IgnoreDefaultLibraryNames") or "").replace(",", ";").split(";"):
            if d:
                link.append(f'/NODEFAULTLIB:"{d}"')
        if (lt.get("GenerateDebugInformation") or "").upper() == "TRUE":
            link += ["/DEBUG", f'/PDB:"{win(stem)}.pdb"']
        if (lt.get("GenerateMapFile") or "").upper() == "TRUE":
            link.append(f'/MAP:"{win(stem)}.map"')
        link.append({"1": "/SUBSYSTEM:CONSOLE", "2": "/SUBSYSTEM:WINDOWS"}[lt.get("SubSystem", "2")])
        if lt.get("BaseAddress"):
            link.append(f"/BASE:{lt.get('BaseAddress')}")
        link.append(f'/IMPLIB:"{win(stem)}.lib"')
        link.append({"1": "/MACHINE:X86"}[lt.get("TargetMachine", "1")])
        link += (lt.get("AdditionalDependencies") or "").split()
        link += DEFAULT_LIBS
    link += [f'"{win(os.path.join(out, o))}"' for o in objs]
    link += [f'"{p}"' for p in lib_inputs]
    write_if_changed(link_rsp, "\n".join(link) + "\n")
    t = os.path.basename(target)
    lines.append(f".PHONY: objs {t}\n{t}: $(OUT)/{t}\n")
    lines.append(f"$(OUT)/{t}: $(OBJS) $(OUT)/link.rsp {' '.join(given[posixpath.basename(r).lower()] for r in proj_libs)}".rstrip())
    lines.append(f"\t@cd $(OUT) && $(MSLINK) {'/lib ' if args.kind == 'lib' else ''}{q('@' + win(link_rsp))} > $@.log 2>&1 || {{ rm -f $@; echo FAIL link; exit 1; }}\n")
    lines.extend(rules)
    with open(args.makefile, "w") as f:
        f.write("\n".join(lines) + "\n")
    with open(args.makefile + ".files", "w") as f:
        for src, obj, mode in files:
            f.write(f"{src}\t{obj}\t{mode}\n")
    print(f"{len(files)} source files ({sum(m not in ('1', '3') for _, _, m in files)} without the precompiled header) "
          f"-> {args.makefile}, target {os.path.basename(target)}")


if __name__ == "__main__":
    main()
