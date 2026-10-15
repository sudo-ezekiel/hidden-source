# Client (client.dll) tools

Hidden's client.dll was built with MSVC 7.1 (Visual Studio .NET 2003 RTM, cl.exe 13.10.3077, per
the PE Rich header) from `cl_dll/client_sdk.vcproj`, configuration "Release SDK|Win32". There is no
PDB and no MSVC 7.1 compiler here, so the client cannot be byte-compared the way the server is.
Instead it is checked structurally: every file must compile, every class's vtable must match the
reference slot for slot, and every RecvTable must match.

Reference files, all under `~/.cache/subject51764/hsrebuild/`:

- `builds/client_2006-06-14_4bfull.dll`: the reference (same md5 as the installed mod's client.dll).
- `decomp/client0614/decomp.c`: Ghidra decompile, about 24300 functions. Ghidra's own analysis
  found only 8600 (`decomp.c.v1`); the rest were reached only through vftables and data, so
  `tools/pe_seeds.py` and `tools/RecoverFunctions.java` seeded functions at every vftable slot,
  every data pointer into code, and every 16-byte boundary after int3 padding.
- `decomp/client0614/decomp_named.c`, `byclass/<Class>.c`, `names.tsv`: the decompile with
  FUN_ names replaced from the vftables (see below).
- `decomp/client0614/vftables.txt`, `rtti.tsv`, `vftable_slots.tsv`: every vftable from MSVC RTTI
  (class, subobject offset, slot targets, base classes).
- `decomp/client0614/vtable_report.tsv`, `vtables_byclass/`: vtable status per class.
- `decomp/client0614/recv/`: the client's RecvTables, the server's SendTables, and their comparison.

## 1. Compile check

    JOBS=6 sh rebuild/client_check.sh ~/.cache/hsrebuild-work-<lane> [file.cpp ...]

Mirrors the tree into the work dir, generates a makefile from `cl_dll/client_sdk.vcproj`
("Release SDK|Win32", the same `vcproj_to_make.py` as the server, with `--objects-only`), and
compiles every source with the GCC 3.4 image, objects only. With file arguments (paths from the
repo root) only those are recompiled and reported. A header change rebuilds everything (about 1.5
minutes at -j6).

Each file is PASS, MSVC or FAIL. MSVC means every error GCC reported matches a pattern in
`rebuild/client_shims/msvc_only.txt`, a construct MSVC 7.1 accepts and GCC cannot (a temporary
bound to `Color&`, the DirectDraw code in imageloader.cpp). Keep that list short; never use it to
hide a real error. Results: `<work>/out/client_check.tsv`; compiler output per file:
`<work>/out/client_obj/<dir>/<name>.o.log`. The objects live in `out/client_obj`, apart from
the server build's `out/obj` (`rebuild/build.sh`), so one work dir can serve both; when both used
`out/obj`, server objects of the shared files counted as client PASSes.

What makes GCC accept the Windows sources, without editing them:

- `rebuild/client_shims/client_shim.h`, force-included: defines the enums MSVC lets the headers
  forward-declare, `CGlobalVarsBase` (used in a template before it is complete), the
  `random`/stdlib clash (`#define random random_valve`, as dlls/enginecallback.h does), `__cdecl`,
  `_snwprintf`, `type_info::raw_name`.
- Stand-in headers in `rebuild/client_shims/`: `windows.h` (only what in_mouse.cpp and
  in_joystick.cpp use), `typeinfo.h`, `crtdbg.h`.
- `-fno-operator-names`: weapon_selection.h has a parameter named `or`.
- Case aliases: Windows-cased includes (`"materialsystem/materialsystemutil.h"`) resolve through
  symlinks that `tools/case_includes.py` creates in `<work>/case`, searched last.

A few Valve headers needed one-line fixes, each marked `REBUILD:`: `##` pastes next to punctuation
(client_class.h, MessageMap.h, hud_macros.h, hud_element_helper.h, initializer.h,
c_te_effect_dispatch.h, c_weapon__stubs.h, particle_prototype.h), `friend class` (Menu.h,
SectionedListPanel.h), a `template<>` (c_point_camera.cpp), and a comment whose trailing backslash
GCC read as a line continuation (c_func_breakablesurf.cpp).

Baseline: the unmodified 2004 client passes 372 of 379 files, 7 MSVC-only, 0 fail.

## 2. vtables and function names

    CLASSDUMP=1 JOBS=6 sh rebuild/client_check.sh ~/.cache/hsrebuild-work-<lane>
    sh rebuild/client_vtables.sh ~/.cache/hsrebuild-work-<lane> C_BasePlayer C_SDKPlayer
    sh rebuild/client_vtables.sh ~/.cache/hsrebuild-work-<lane>

`CLASSDUMP=1` adds `-fdump-class-hierarchy` (dumps land next to the objects as `<file>.cpp.class`;
the first CLASSDUMP run rebuilds everything). `client_vtables.sh` with class names prints each
vftable side by side: slot, reference address, our function, and notes. Without names it writes
`<work>/out/vtables/vtable_report.tsv` for every class.

How our order is computed (`tools/gcc_classdump.py`): GCC and MSVC agree on declaration order and
on one vftable per polymorphic base subobject. The differences converted:

- a virtual destructor has two GCC slots and one MSVC slot (the scalar deleting destructor);
- GCC repeats a function that overrides a secondary base's virtual in the primary vtable, MSVC
  does not (this is why a naive count of C_BaseEntity is one slot too many: IClientEntity's
  `Release`);
- MSVC groups the overloads a class introduces at the first one's position, in reverse
  declaration order.

Checks per class: slot counts per vftable, and inheritance: where our slot says the function is
inherited from base X, the reference slot must hold the same address as X's (an "inherit" error
means the slots are misaligned from there on). A class is MATCH when every count agrees and there
are no inherit errors; this is the client's main "done" test for a class. Where our base subobject
offsets differ from the reference's (a data layout difference, for example C_BaseEntity is 0x46c
bytes in ours and 0x480 in Hidden's), the report says so in its layout column.

`names.tsv` names each slot target from the class it belongs to: high (class matches), medium
(before the first misalignment), low (after it), folded (one address, several names), slot (a
class we do not have yet: `CHudHAmmo::vfunc81_at_14` = slot 81 of the vftable at +0x14).

When the original compiler is available, `tools/vtable_names.py --ours-dll <our client.dll>
--ours-map <our client.map>` takes our order and names from our own MSVC build (RTTI plus the
`/MAP` file) instead of the GCC dumps, so the same report and naming work against a real build.
That mode is untested (no MSVC build exists yet).

## 3. RecvTables

    sh rebuild/client_recv.sh ~/.cache/hsrebuild-work-<lane> DT_BasePlayer DT_SDKPlayer
    sh rebuild/client_recv.sh ~/.cache/hsrebuild-work-<lane>

Three sides: client.dll's RecvTables and Hidden's server SendTables (both read from the decompiles,
`tools/recv_tables.py` explains how), and our client source (BEGIN_RECV_TABLE,
BEGIN_NETWORK_TABLE and IMPLEMENT_CLIENTCLASS_DT blocks, CLIENT_DLL branches only, files in the
build only). Props are compared by name within a table, with types folded to the client's
(EHandle and Bool are Int, Time is Float, Array3 is Array). With table names it prints all three
sides; without, it writes `<work>/out/recv/recv_report.tsv` (client.dll against the server) and
`recv_report_ours.tsv` (our source against client.dll). The source parser is a text parser:
unusual macro forms can show up as false differences, so check the printed tables before acting on
a DIFF.

client.dll against Hidden's server: 164 of 173 tables match. The rest are real differences in the
shipped binaries (the client receives a few props the server does not send, such as DT_PhysBox
m_mass; DT_Local has m_flStepSize on the server and m_vecClientBaseVelocity on the client; the world
table is DT_WORLD on the server and DT_World on the client).

## 4. Data layout (member offsets)

    python3 -I rebuild/tools/recv_offsets.py ~/.cache/hsrebuild-work-<lane> DT_BasePlayer DT_SDKPlayer

Every RecvProp in client.dll carries the offset of the member it writes. The tool compiles one
probe per source file (the file itself plus `(int)&((Class *)0)->member` for each prop, with
`-fno-access-control`) and prints each prop whose offset differs from client.dll's, plus
`<work>/out/layout/recv_offsets.tsv`. Run `client_check.sh` first (it needs the mirror and the
makefile). Read the result as differences: a constant gap over a class's own members means that
class is right and a base class differs. Example, after the C_BasePlayer state commit: every
C_BasePlayer member was 0x4c lower than in client.dll, while C_BaseEntity's were 0x10 lower and
C_BaseCombatCharacter's m_iAmmo 0x20 lower, so the missing bytes were in the bases. Members written
as `m_vecViewOffset[0]` (a Vector element) print `?`: the probe expression is not a constant there.

Two more references for members without a RecvProp, in `decomp/client0614/`:

- `class_sizes.tsv`: sizeof for the 58 classes client.dll's client class factories allocate (the
  constant passed to operator new, FUN_24089b40, just before the constructor call).
- `typedesc_tables.txt`: the 41 prediction/datadesc typedescription tables (0x34-byte entries:
  type, name, offset), which give offsets for predicted members that are not networked.

MSVC 7.1 lays these classes out differently from GCC 3.4 in three ways, and the compile check makes
GCC follow client.dll in each (all GCC-only, so an MSVC build is unchanged):

- Member function pointers of C_BaseEntity (`m_pfnThink`, `m_pfnTouch`) are 16 bytes in MSVC,
  because BASEPTR names the class before it is defined and MSVC falls back to its general
  representation; GCC uses 8. `c_baseentity.h` adds 8 bytes after each under `__GNUC__`.
- MSVC aligns doubles in structures to 8 bytes: `client_check.sh` passes `-malign-double`
  (CSimplePhysics holds a double).
- MSVC aligns C_BaseEntity, and so every entity class, to 8 bytes: every size in
  `class_sizes.tsv` is a multiple of 8, and entity-type members such as C_FireSmoke's C_FireSprite
  array sit on 8-byte boundaries. `REBUILD_MSVC_ALIGN8` on C_BaseEntity gives GCC the same
  alignment. GCC (Itanium ABI) also reuses a base class's tail padding for the derived class's
  first members, which MSVC never does, so a base class whose members end 4 bytes short of a
  multiple of 8 ends with `REBUILD_MSVC_TAIL_PAD( m_MSVCTailPad )`. A leaf class needs no pad.
  Which C_BaseEntity member gives MSVC the 8-byte alignment is not known yet.

With these, all 58 factory sizes match.

## Original build settings ("Release SDK|Win32", cl_dll/client_sdk.vcproj)

Compiler (VCCLCompilerTool), as attributes and the cl.exe 13.10 switches they stand for:

- Optimization=3 (/Ox), GlobalOptimizations (/Og), InlineFunctionExpansion=0 (/Ob0 in the VS 2003
  enumeration; confirm in the IDE, since /Ox alone implies /Ob2), EnableIntrinsicFunctions (/Oi),
  FavorSizeOrSpeed=1 (/Ot), OptimizeForProcessor=3 (/G7)
- StringPooling (/GF), ExceptionHandling=FALSE (no /EHsc), RuntimeLibrary=4 (/ML, single-threaded
  static CRT), BufferSecurityCheck=FALSE (/GS-), EnableFunctionLevelLinking (/Gy),
  RuntimeTypeInfo (/GR)
- UsePrecompiledHeader=3 through cbase.h (/Yu"cbase.h" /Fp"Release_SDK/client.pch"); 43 files
  override it to 0 (no PCH) and one to 1 (creates it)
- WarningLevel=4 (/W4), DebugInformationFormat=3 (/Zi), BrowseInformation=1 (/FR)
- Defines: NDEBUG SDK_DLL NO_STRING_T CLIENT_DLL _WINDOWS VECTOR
  strncpy=use_Q_strncpy_instead _snprintf=use_Q_snprintf_instead _WIN32 PROTECTED_THINGS_ENABLE
- Includes: ../Public/tier1; ./sdk/vgui; ./sdk; ../game_shared/sdk; .; ../Public;
  ../vgui2/include; ../vgui2/controls; ../game_shared; ./game_controls

Linker (VCLinkerTool): /DLL winmm.lib, /INCREMENTAL:NO (LinkIncremental=1),
/LIBPATH:..\lib\public, /NODEFAULTLIB:libcd.lib,LIBCD,LIBCMT,libcmtd.lib, /DEBUG
(GenerateDebugInformation; note that /DEBUG makes /OPT:NOREF,NOICF the default, which fits the
reference having almost no folded functions), /PDB:Release_SDK/client.pdb,
/MAP:Release_SDK/client.map, /SUBSYSTEM:WINDOWS, /BASE:0x24000000, /IMPLIB:Release_SDK/client.lib,
/MACHINE:X86. The reference's PDB path is `d:\hl2sdk\src\cl_dll\Release_SDK\client.pdb`.

## Re-running the decompile

One Ghidra process at a time on `~/tools/ghidra-proj-hsrebuild` (program client0614, already
seeded). To redo it from scratch:

    python3 -I rebuild/tools/pe_seeds.py <client.dll> <scratch>/seeds
    analyzeHeadless ~/tools/ghidra-proj-hsrebuild client0614 -process client_2006-06-14_4bfull.dll \
        -noanalysis -scriptPath <dir with RecoverFunctions.java and DumpAll.java> \
        -postScript RecoverFunctions.java <scratch>/seeds/seeds.txt <scratch>/seeds/labels.txt gaps \
        -postScript DumpAll.java <out dir>
    python3 -I rebuild/tools/pe_rtti.py <client.dll> <out dir>
    python3 -I rebuild/tools/vtable_names.py <client.dll> <out dir> --dumps <work>/out/client_obj
