"""Write a Linux makefile from a Visual Studio 2003 .vcproj, the way Valve's vprojtomake (vcpm) did.

vcpm needed Xerces, so this re-implements the parts the SDK used (see
utils/vprojtomake/vcprojconvert.cpp and makefilecreator.cpp):
- every <File> whose RelativePath ends in .cpp is compiled unless a <FileConfiguration>
  for the chosen configuration sets ExcludedFromBuild="TRUE";
- defines come from the configuration's VCCLCompilerTool, minus WIN32/_WIN32/_WINDOWS/WINDOWS;
- include dirs come from AdditionalIncludeDirectories, relative to the .vcproj;
- objects are grouped by source directory, and the link line lists the groups in the order
  their first file appears, which is the link order the original binaries show.

Compiler flags are the 2004 SDK linux_sdk/Makefile's (-mtune=i686 -march=pentium -mmmx -O3)
plus BASE_CFLAGS; --arch-cflags overrides the first part. Usage:

    python3 -I vcproj_to_make.py <vcproj> "<Config|Win32>" <source root for make> <out Makefile> \\
        --host-root <source root on disk> [--name server]
"""
import argparse
import os
import posixpath
import xml.etree.ElementTree as ET

WIN_DEFINES = {"WIN32", "_WIN32", "_WINDOWS", "WINDOWS"}

# Hidden: Source used the 2004 SDK's -march=pentium: its server has no cmov or fcomi, which
# the 2006 SDK's -march=pentium3 would emit thousands of.
ARCH_CFLAGS = "-mtune=i686 -march=pentium -mmmx -O3"
BASE_CFLAGS = (
    "-fpermissive -D_LINUX -DNDEBUG -Dstricmp=strcasecmp -D_stricmp=strcasecmp "
    "-D_strnicmp=strncasecmp -Dstrnicmp=strncasecmp -D_snprintf=snprintf "
    "-D_vsnprintf=vsnprintf -D_alloca=alloca -Dstrcmpi=strcasecmp"
)
TAIL_CFLAGS = "-Usprintf=use_Q_snprintf_instead_of_sprintf -Ustrncpy=use_Q_strncpy_instead -UPROTECTED_THINGS_ENABLE"


def norm(path):
    return path.replace("\\", "/")


def fix_case(root, rel):
    """Return rel with each component spelled as it is on disk (the projects were written on Windows)."""
    out = []
    cur = root
    for part in rel.split("/"):
        if part in ("", "."):
            continue
        if part == "..":
            out.append(part)
            cur = os.path.dirname(cur)
            continue
        names = os.listdir(cur) if os.path.isdir(cur) else []
        match = next((n for n in names if n == part), None) or next((n for n in names if n.lower() == part.lower()), part)
        out.append(match)
        cur = os.path.join(cur, match)
    return "/".join(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("vcproj")
    ap.add_argument("config")
    ap.add_argument("srcroot", help="path of the source tree as seen by make")
    ap.add_argument("--host-root", required=True, help="path of the source tree on this machine")
    ap.add_argument("out")
    ap.add_argument("--name", default="server")
    ap.add_argument("--extra-cflags", default="")
    ap.add_argument("--arch-cflags", default=ARCH_CFLAGS)
    ap.add_argument("--objects-only", action="store_true",
                    help="no link: 'all' builds the objects, each compile runs in its object's directory "
                         "and logs to <obj>.log (client_check.sh)")
    args = ap.parse_args()

    # VS2003 project files are not always well-formed UTF-8; read leniently.
    with open(args.vcproj, "rb") as f:
        data = f.read().decode("latin-1")
    data = data.replace('encoding="Windows-1252"', "")
    root = ET.fromstring(data)

    config = None
    for c in root.iter("Configuration"):
        if c.get("Name") == args.config:
            config = c
    if config is None:
        raise SystemExit(f"configuration {args.config!r} not found")
    tool = next(t for t in config.iter("Tool") if t.get("Name") == "VCCLCompilerTool")
    defines = [d for d in (tool.get("PreprocessorDefinitions") or "").split(";") if d and d not in WIN_DEFINES]
    includes = [norm(i) for i in (tool.get("AdditionalIncludeDirectories") or "").split(";") if i]

    # Paths in the project are relative to the .vcproj's directory.
    vc_dir = norm(os.path.relpath(os.path.dirname(os.path.abspath(args.vcproj)), os.path.abspath(args.host_root)))

    files = []
    for f in root.iter("File"):
        rel = norm(f.get("RelativePath") or "")
        if not rel.lower().endswith(".cpp"):
            continue
        excluded = False
        for fc in f.iter("FileConfiguration"):
            if fc.get("Name") == args.config and (fc.get("ExcludedFromBuild") or "").upper() == "TRUE":
                excluded = True
        if not excluded:
            files.append(fix_case(args.host_root, posixpath.normpath(posixpath.join(vc_dir, rel))))

    groups = {}
    order = []
    for path in files:
        d = posixpath.dirname(path)
        if d not in groups:
            groups[d] = []
            order.append(d)
        if path not in groups[d]:
            groups[d].append(path)

    src = args.srcroot.rstrip("/")
    inc_paths = []
    for i in includes:
        p = fix_case(args.host_root, posixpath.normpath(posixpath.join(vc_dir, i)))
        if p not in inc_paths:
            inc_paths.append(p)
    inc = " ".join(f"-I{src}/{p}" for p in inc_paths)
    defs = " ".join(f"-D{d}" for d in defines)
    lines = [
        f"# Generated by rebuild/tools/vcproj_to_make.py from {posixpath.join(vc_dir, os.path.basename(args.vcproj))}, {args.config}",
        f"NAME={args.name}",
        "ARCH=i486",
        "CPLUS?=g++-3.4",
        "CLINK?=gcc-3.4",
        "OBJ?=obj",
        f"SRC={src}",
        f"INCLUDES={inc} {defs}",
        f"CFLAGS={args.arch_cflags} {BASE_CFLAGS} {TAIL_CFLAGS} {args.extra_cflags}".rstrip(),
        "DO_CC=$(CPLUS) -w $(INCLUDES) $(CFLAGS) -o $@ -c $<",
        "LDFLAGS?=-lm -ldl $(SRC)/linux_sdk/tier0_i486.so $(SRC)/linux_sdk/vstdlib_i486.so "
        "$(SRC)/linux_sdk/mathlib_i486.a $(SRC)/linux_sdk/choreoobjects_i486.a $(SRC)/linux_sdk/tier1_i486.a",
        "CPP_LIB?=-Wl,-Bstatic -lstdc++ -Wl,-Bdynamic -lgcc_eh",
        "",
    ]
    objs = []
    rules = []
    for i, d in enumerate(order):
        var = f"OBJS_{i}"
        lines.append(f"# {d}")
        lines.append(f"{var}= \\")
        for path in groups[d]:
            o = f"$(OBJ)/{d}/{posixpath.splitext(posixpath.basename(path))[0]}.o"
            lines.append(f"\t{o} \\")
            if args.objects_only:
                rules.append(f"{o}: $(SRC)/{path}\n\t@mkdir -p $(dir $@)\n\t@cd $(dir $@) && $(DO_CC) > $@.log 2>&1 || {{ rm -f $@; echo FAIL {path}; exit 1; }}\n")
            else:
                rules.append(f"{o}: $(SRC)/{path}\n\t@mkdir -p $(dir $@)\n\t$(DO_CC)\n")
        lines.append("")
        objs.append(f"$({var})")
    if args.objects_only:
        lines.append(f"all: {' '.join(objs)}\n")
        # Sidecar list for client_check.sh: source path<TAB>object path (relative to OBJ).
        with open(args.out + ".files", "w") as f:
            for d in order:
                for path in groups[d]:
                    f.write(f"{path}\t{d}/{posixpath.splitext(posixpath.basename(path))[0]}.o\n")
    else:
        lines.append(f"all: $(NAME)_$(ARCH).so\n")
    lines.append(f"$(NAME)_$(ARCH).so: {' '.join(objs)}")
    lines.append(f"\t$(CLINK) -shared -Wl,-Map,$@_map.txt -o $@ {' '.join(objs)} $(LDFLAGS) $(CPP_LIB)\n")
    lines.extend(rules)
    with open(args.out, "w") as f:
        f.write("\n".join(lines) + "\n")
    print(f"{len(files)} source files in {len(order)} directories -> {args.out}")


if __name__ == "__main__":
    main()
