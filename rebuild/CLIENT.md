# Client (client.dll) tools

Hidden's client.dll was built with MSVC 7.1 (Visual Studio .NET 2003 RTM, cl.exe 13.10.3077, per
the PE Rich header) from `cl_dll/client_sdk.vcproj`, configuration "Release SDK|Win32". There is no
PDB. The client is checked structurally with GCC: every file must compile, every class's vtable must
match the reference slot for slot, and every RecvTable must match. It is also built with that exact
compiler (Visual C++ Toolkit 2003 1.01) under Wine and compared with client.dll function by
function (section 5; the toolchain is in `msvc/MSVC.md`).

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
  `Release`). This includes a destructor that only overrides a secondary base's (CClientLeafSystem,
  whose primary chain has no virtual destructor). The overrider is recognised by its qualified
  name in a secondary vtable, so a new overload that only shares the name stays (the
  GetParticlePosition( Particle *, Vector & ) of C_ParticleSmokeGrenade);
- MSVC groups the overloads a class introduces at the first one's position, in reverse
  declaration order.

Template classes are matched under the names client.dll's RTTI gives them
(`CInterpolatedVarArray<float, 24>` is `?$CInterpolatedVarArray@M$0BI@`).

Checks per class: slot counts per vftable, and inheritance: where our slot says the function is
inherited from base X, the reference slot must hold the same address as X's (an "inherit" error
means the slots are misaligned from there on). A class is MATCH when every count agrees and there
are no inherit errors; this is the client's main "done" test for a class. Where our base subobject
offsets differ from the reference's (a data layout difference, such as C_BaseEntity's size before
section 4's fixes), the report says so in its layout column.

`names.tsv` names each slot target from the class it belongs to: high (class matches), medium
(before the first misalignment), low (after it), folded (one address, several names), slot (a
class we do not have yet: `CHudHAmmo::vfunc81_at_14` = slot 81 of the vftable at +0x14).

When the original compiler is available, `tools/vtable_names.py --ours-dll <our client.dll>
--ours-map <our client.map>` takes our order and names from our own MSVC build (RTTI plus the
`/MAP` file) instead of the GCC dumps, so the same report and naming work against a real build.
Against the MSVC build of section 5 it reports 703 classes MATCH, 0 MISMATCH and 3 MISSING; the 3
are anonymous-namespace classes (GripPanel, CaptionGripPanel, ScrollBarButton) whose RTTI names
carry a per-build hash, and they are in our build under another hash.

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
  multiple of 8 ends with `REBUILD_MSVC_TAIL_PAD( m_MSVCTailPad )` (`REBUILD_MSVC_TAIL_PAD_BYTES`
  when they end on an odd boundary). A leaf class needs no pad.
  The MSVC build shows what gives C_BaseEntity its 8-byte alignment: BASEPTR and ENTITYFUNCPTR
  name C_BaseEntity before it is complete, so they are 16-byte member function pointers, which
  MSVC 7.1 aligns to 8 (in thinkfunc_t, m_pfnThink moves to +8 behind a 4-byte member).

With these, 51 of the 58 factory sizes match. The other 7 are C_WeaponSDKBase and the weapons
derived from it, all 8 bytes short: client.dll's C_WeaponSDKBase has two CMaterialReference members
at 0x820 and 0x824 (constructor 241ef660) that ours lacks. To look for tail
padding reuse, build with CLASSDUMP=1 and compare `size=` with `base size=` in the .class dumps for
every class that has derived classes: where they differ, GCC places the derived members early.

## 5. MSVC build and function comparison

    JOBS=6 sh rebuild/msvc/build_client.sh ~/.cache/hsrebuild-msvc-<lane> [file.cpp ...]
    sh rebuild/msvc/compare_client.sh ~/.cache/hsrebuild-msvc-<lane>
    NAME=<part of our name, or a reference address; several, comma separated> sh rebuild/msvc/compare_client.sh ~/.cache/hsrebuild-msvc-<lane>
    sh rebuild/msvc/compare_client.sh --loose ~/.cache/hsrebuild-msvc-<lane>    # or LOOSE=1: the original scoring

The build (`msvc/MSVC.md`, "Building client.dll") compiles vgui_controls and the 416 client files
with cl 13.10.3077 under Wine and links client.dll with /MAP, in about half a minute. The compare
writes `<work>/cmp_client.tsv` and prints the totals and the objects with the most changed code;
with NAME it prints the normalised diff of the matching functions instead.

`tools/msvc_compare.py` does what `compare_builds.py` does for the server, except that client.dll
has no symbols, so its functions are first mapped to ours:

- Functions: ours from the map file; client.dll's from the decompile's function list plus every
  vftable slot, direct call target, import thunk and tail jump, without the decompile's seeds that
  sit where code falls through (a real start never does) or inside a jump table. objdump's linear
  sweep is redone from any start it ran past. After a first mapping, starts the mapping implies
  (a reference body that is ours plus more code after a ret, a branch where ours goes to a function
  start) are added and the mapping runs again.
- Mapping, repeated until nothing changes: vftables of the same class and subobject with the same
  slot count, slot by slot; the export and the entry point; names.tsv names unique among ours; the
  call graph (in a mapped pair whose bodies line up, the call and jump targets and function pointers
  at the same positions); link order (between two mapped functions in the same order on both
  sides, functions with the same body, or in order when the counts agree and the bodies are 60%
  alike); the same string literals; the same body. The method is in the row.
- Normalisation, strict (the default; STRICT-COMPARE.md section 6): the PE base relocations say
  which operands are addresses. Branches inside the function become offsets, branches out of it
  the callee's name (ours; on client.dll's side the name of the function it is mapped to, so a call
  to an unmapped function is never "same"); every relocated operand becomes a token for what it
  points at (a mapped function, a vftable by class, an import slot, a type descriptor, writable
  data by our symbol through a data pairing, .rdata by its content: floats, strings, bytes), and
  every other number is kept. The strict tokens also re-pair functions the loose mapping could not
  tell apart (method `strict`, and `table` for a function only a table of pointers names). Jump and
  index tables after the code are compared as lists of in-function offsets. Trailing int3 and nop
  are dropped.
- Normalisation, --loose: the same, except that every image address other than an import slot
  (IMP:<name>) is ADDR and the mapping is not refined. It reproduces the scoring from before the
  strict mode.

Rows: object (lib:object from the map), status (same, changed, unmapped, or only-ref and only-ours
for a function in a vftable of a class the other build lacks, of which there are none now),
similarity, ref size, our size, our name, demangled name, ref address, our address, method,
names.tsv name. A call graph mapping makes the callee's name agree in the caller by construction;
a wrong pairing then shows up as a changed callee.

Result (class-image e71adaf; main 2f11229, the client-finish merge, had 23764 same and 143 changed
strict, 23534 and 156 loose):

| | strict: functions | bytes | --loose: functions | bytes |
|---|---|---|---|---|
| client.dll functions | 24320 | 2,148,634 | 24320 | 2,148,634 |
| mapped | 23922 (98.4%) | 2,117,964 | 23705 (97.5%) | 2,112,980 |
| same | 23780 | 2,072,512 | 23550 | 2,067,499 |
| changed | 142 | 45,452 | 155 | 45,481 |
| unmapped, client.dll | 398 | 30,670 | 615 | 35,654 |
| unmapped, ours | 316 | 11,479 | 533 | 16,455 |

By origin of our function, strict: client objects 20183 same, 83 changed; vgui_controls 3001
same, 4 changed; libc.lib 462 same, 54 changed; nvtc.lib 19 same, 1 changed; import thunks 115
same. Mapped by vftable 5418, call graph 4553, order 12783, string 788, body 111, strict 268,
table 1. Most unmapped functions are 32 bytes or less (263 of 398), copies of small inline
functions that nothing tells apart.

The class image (class-image lane) is in sdk/clientmode_sdk.cpp: ClientModeSDKNormal::PostRenderVGui
draws the 3DModel of the first visible CSClassImagePanel (models/player/iris.mdl in Teammenu.res and
Weaponmenu.res) through UpdateClassImageEntity, Counter-Strike: Source's code with Hidden's
sequences and weapon: a client-only C_BaseAnimatingOverlay on "Idle_lower" with "walk_lower" and
"Idle_Upper_Aug" layers, a bone-merged models/weapons/f2000/w_f2000.mdl, a dynamic light, and a
view of its own (fov 54, 110 units behind the model, which faces yaw 210). All 14 functions of it
(PostRenderVGui, the renderer at 241e6440, WillPanelBeVisible, ShouldRecreateClassImageEntity and
the CHandle and CUtlVector COMDATs) are same in both modes, in client.dll's order, and the two
handles sit in .data where client.dll has them.

Strict against loose: 6 functions are same only in the loose mode, all pairs of functions with the
same body that the strict tokens pair the other way (CMyPanel::GetPanelBaseClassName,
ClientClassInit<DT_CFN303Ball>, the dynamic initializers $E5 and $E8 of c_func_lod and $E7 and $E8
of weapon_pistol); 18 loose "changed" rows were two functions paired the wrong way round and are
same once the strict tokens re-pair them; 218 functions the loose mapping left unmapped are mapped
and same.

The Rich header now agrees on the C++ objects: 426 Utc1310_CPP on both sides. The one ours lacked
was game_shared/death_pose.cpp, which client.dll links between death.obj and decals.obj
(GetRagdollCurSequenceWithDeathPose, which C_AI_BaseNPC::GetRagdollCurSequence calls). The rows that
still differ are the linker and import library builds of the SDK's .lib files, not code.

What is left falls into four groups. The first is mapping limits: client.dll functions that the
decompile's function starts cut short or split, so our row carries code the reference row lacks
(C_BaseAnimating::FireEvent, DrawTeslaSegs, CTempEnts::Sprite_Trail, C_BasePlayer::PhysicsSimulate,
CPrediction::PostEntityPacketReceived, the VPROF exit at the end of VGui_PreRender and others), and
jump tables that fall after such an end (Label::ComputeAlignment). The second is Hidden code where
the instructions are right but the registers or their order are not, because the exact source shape
is not known: CHudRadar::Paint, C_WeaponSDKBase::InitBeam, C_LaserDot::DrawModel,
CHudCrosshair::Paint, HDNTeamMenu::OnCommand, ScrollBar::OnMousePressed,
C_HLTVCamera::CalcChaseCamView, CHudDeathNotice::Paint, C_BaseCombatWeapon::ShadowCastType (where
the class name literal is loaded) and SDK_EjectBrass (a null test client.dll makes around the
shooter argument). The third is shared code that cannot match every binary at once:
C_WeaponShotgun::PrimaryAttack (client.dll's if/else would move the __LINE__ that SHARED_RANDOMINT
uses, which the server builds depend on) and CGameMovement::CheckBack (client.dll and server.dll
want the forward move stored before m_bSpeedCropped, server_i486.so wants the bool stored between
the load and the store; main keeps the GCC form). The fourth is hand-written CRT code in libc.lib
and nvtc.lib, which the mapping cannot pair cleanly and which is left as it is.

Missing code, from the unmapped client.dll functions over 32 bytes (135, of which 97 start at an
address that is not 16-byte aligned and are the tails of reference functions cut short): an
unreferenced C_BasePlayer helper at 24099df0 that hashes a lower-cased file name (CRC32_Init, strlwr), and two TGAWriter
functions from a different SDK drop (24197430 and 241977e0). The rest are CUtlVector and CUtlMemory
helpers, DECLARE map helpers and CRT pieces.

## Original build settings ("Release SDK|Win32", cl_dll/client_sdk.vcproj)

Compiler (VCCLCompilerTool), as attributes and the cl.exe 13.10 switches they stand for:

- Optimization=3 (/Ox), GlobalOptimizations (/Og), InlineFunctionExpansion=0 (no switch, so /Ox's
  own inlining applies; the MSVC build confirms it, see msvc/MSVC.md), EnableIntrinsicFunctions (/Oi),
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
