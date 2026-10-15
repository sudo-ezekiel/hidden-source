# MSVC 7.1 toolchain for client.dll and server.dll

Hidden's client.dll Rich header names cl 13.10.3077 and link 7.10.3077, the compiler of Visual
Studio .NET 2003 RTM. The free way to get exactly that build is the Visual C++ Toolkit 2003,
version 1.01. This folder unpacks it, plus the Win32 headers and import libraries from a Platform
SDK of the same era, into a tree that runs under Wine. Nothing is installed and no installer runs.

## Which compiler

| Release | Date | cl / link | Use |
|---|---|---|---|
| Toolkit 1.0 | 16 April 2004 | 13.10.3052 / 7.10.3052 | No: not Hidden's build |
| Toolkit 1.01 | 6 July 2004 | 13.10.3077 / 7.10.3077 | Yes |
| .NET Framework SDK 1.1 | 2003 | 13.10.3077 | No: the non-optimizing compiler |

Microsoft's notes for 1.01 say it brought the Toolkit's compiler in line with the one in VS .NET
2003 (build 13.10.3077). The banner is the only reliable way to tell the two Toolkit releases
apart; both installers report version 1.00.000 in Setup.ini.

A first archived copy, VCToolkitSetup.exe (32,952,488 bytes, SHA1
956c81c3106b97042c4126b23c81885c4b5211f4, valid Microsoft signature), turned out to be 1.0.

## Downloads

| File | Source | Bytes | SHA1 |
|---|---|---|---|
| VC_Toolkit_2003_Setup.exe | https://archive.org/details/vc-toolkit-2003-setup | 32,948,392 | 50dc6cca1503f08a41d56816f3f1abcaf347b0bd |
| 5.2.3790.1830.15.PlatformSDK_Svr2003SP1_rtm.img | https://archive.org/details/5.2.3790.1830.15.-platform-sdk-svr-2003-sp-1-rtm | 414,361,600 | ee43b337d9739a4da3417b8eaca36cfafaa41de7 |

The Toolkit's Authenticode signature is Microsoft Corporation (Microsoft Code Signing PCA,
Microsoft Root Authority), timestamped 2004-06-29 22:31:54 UTC. The SHA1 it signs,
98f2ebf413504ed1e824203b513bbf5750b05971, equals the file's own Authenticode hash
(`authenticode.py`). The SDK image is an ISO 9660 volume labelled PSDK with files dated
2005-04-15 (MD5 dd26a923e94efd35878d9a725f012660). Which Platform SDK Hidden used is unknown;
the client includes windows.h in only a few files, so header differences should not show.

Microsoft's files are not in this repo. The Toolkit allows redistribution only of the runtime
DLLs listed in its redist.txt, and the SDK image is over GitHub's 100 MB file limit.

## Unpacking

    sh rebuild/msvc/unpack.sh VC_Toolkit_2003_Setup.exe 5.2.3790.1830.15.PlatformSDK_Svr2003SP1_rtm.img \
        ~/.cache/subject51764/hsrebuild/msvc71/tree

Both inputs are checked against the SHA1s above first. Then:

1. 7z splits the setup .exe; its payload `[0]` is an InstallShield stream. `is_stream.py` splits
   it and undoes InstallShield's XOR (key = file name XOR 13 35 86 07, byte =
   ~(key ^ nibble-swapped byte)), giving `Microsoft Visual C++ Toolkit 2003.msi`.
2. 7z splits the MSI into its table streams and `Data1.cab`, whose entries are named by File key.
3. `msi_table.py` reads the File, Component and Directory tables; `msi_layout.py` copies each
   cabinet entry to its install path under INSTALLDIR.
4. For the SDK, only `Setup/PSDK-x86.msi` and the `PSDK-SDK_Core_BLD-common.*` and
   `PSDK-SDK_Core_BLD_X86-common.*` cabinets are extracted and laid out the same way under
   INSTALLLOCATION (the other 13221 files are documentation, samples, tools and 64-bit libraries).

Result:

- `vc71/bin`: cl.exe, c1.dll, c1xx.dll, c2.dll, link.exe, mspdb71.dll, msobj71.dll, msvcr71.dll,
  msvcp71.dll, dbghelp.dll (182 files in the whole vc71 tree).
- `vc71/include`: the CRT and standard C++ library headers.
- `vc71/lib`: libc.lib, libcmt.lib, libcp.lib, libcpmt.lib, oldnames.lib, kernel32.lib, mscoree.lib
  and the debug CRT.
- `psdk/Include` (815 headers) and `psdk/Lib` (197 x86 import libraries).

The Toolkit has no lib.exe, nmake or rc.exe. The client project needs none of them: it is only
.cpp files plus four prebuilt libraries.

## Running

    WINEPREFIX=~/.cache/subject51764/hsrebuild/msvc71/wineprefix WINEDEBUG=-all wine <tree>/vc71/bin/cl.exe

prints

    Microsoft (R) 32-bit C/C++ Optimizing Compiler Version 13.10.3077 for 80x86

and link.exe prints `Incremental Linker Version 7.10.3077`. Use a prefix of its own; the first
run creates it.

## Release SDK settings (cl_dll/client_sdk.vcproj)

Compiler, from the configuration's VCCLCompilerTool:

| Setting | Value | Switch |
|---|---|---|
| Optimization | 3 | /Ox |
| GlobalOptimizations | TRUE | /Og |
| EnableIntrinsicFunctions | TRUE | /Oi |
| FavorSizeOrSpeed | 1 | /Ot |
| OptimizeForProcessor | 3 | /G7 |
| InlineFunctionExpansion | 0 | none (the IDE's "Default", so /Ox's inlining applies; checked below) |
| StringPooling | TRUE | /GF |
| EnableFunctionLevelLinking | TRUE | /Gy |
| RuntimeTypeInfo | TRUE | /GR |
| RuntimeLibrary | 4 | /ML (single-threaded static CRT, libc.lib) |
| ExceptionHandling | FALSE | no /EHsc |
| BufferSecurityCheck | FALSE | no /GS (already the 7.1 default) |
| WarningLevel | 4 | /W4 |
| DebugInformationFormat | 3 | /Zi |
| UsePrecompiledHeader | 3, through cbase.h | /Yu"cbase.h" /Fp.\Release_SDK/client.pch |
| BrowseInformation | 1 | /FR |

Include directories and defines are the vcproj's (NDEBUG, SDK_DLL, NO_STRING_T, CLIENT_DLL,
_WINDOWS, VECTOR, the strncpy and _snprintf traps, _WIN32, PROTECTED_THINGS_ENABLE).

Per file: stdafx.cpp creates the precompiled header (/Yc), 43 files compile without it, 75 are
excluded from the build, and sdk/c_te_firebullets.cpp writes c_te_firebullets1.obj because
another file has the same name.

Linker: /DLL /INCREMENTAL:NO /DEBUG /PDB /MAP /SUBSYSTEM:WINDOWS /BASE:0x24000000 /MACHINE:X86
/IMPLIB, /NODEFAULTLIB for libcd.lib, LIBCD, LIBCMT and libcmtd.lib, and the libraries winmm.lib,
lib/public/nvtc.lib, tier0.lib, vgui_controls.lib and vstdlib.lib.

## Checked against the reference

Each answer comes from a full build (below) compared with client.dll by `tools/msvc_compare.py`.

- **InlineFunctionExpansion 0 means no switch.** With nothing passed, 19704 of the client's own
  mapped functions are identical to client.dll's; with /Ob1, 15317; with /Ob0, 8502 (/Ob0 also
  shrinks the DLL from 3,223,552 to 2,797,568 bytes, because every inline accessor becomes a call).
  Both binaries also keep out-of-line copies of inline functions nobody calls (CUtlVector::Size and
  the like), which is what /Ox with /Gy and no /OPT:REF leaves.
- **The IDE linked its default Windows libraries.** client.dll imports SystemParametersInfoA
  (in_mouse.cpp) from USER32.dll, and user32.lib is not in the vcproj, so the IDE's inherited
  kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib
  oleaut32.lib uuid.lib odbc32.lib odbccp32.lib were on the link line (an import library only
  adds what is referenced). Our link gives the same 118 imports from the same five DLLs in the
  same order (WINMM, KERNEL32, USER32, tier0, vstdlib); only Q_strncat sits elsewhere in
  vstdlib's list. odbc32.lib and odbccp32.lib are not in the Platform SDK tree and are left out.
- **No /OPT: link 7.1's /DEBUG defaults, /OPT:NOREF and /OPT:NOICF.** No folding: 6049 of
  client.dll's functions sit in groups of byte-identical bodies (the largest has 640 copies), 5990
  of ours do (642). No removal: 10065 of client.dll's functions are never called, jumped to or
  pointed at from code or data, 10078 of ours.

Two more things the reference settles that the vcproj does not say:

- **vgui_controls.lib was compiled with /GL.** Hidden linked Valve's prebuilt vgui_controls.lib.
  client.dll's Rich header counts 37 objects from Utc1310_LTCG_CPP (cl 13.10.3077 with /GL), its
  vgui_controls functions come first in .text (link.exe generates LTCG code before the rest), and
  they are inlined across files (Panel::SetBorder reads g_pPanelInterface where the source calls
  vgui::ipanel()). vgui_controls.vcproj's Release configuration has no /GL, so build_client.sh adds
  it. With it, 2965 vgui_controls functions are identical instead of 621, and 19704 of the client
  objects' instead of 19412 (the linker takes inline functions such as the GetMessageMap of
  DECLARE_PANELMESSAGEMAP from the LTCG objects, where the message map dictionary is inlined), and
  our Rich header has the same 37 LTCG objects.
- **ddraw.h** came from the DirectX SDK (public/nvtc.h includes it for imageloader.cpp), which the
  Platform SDK does not have. `include/ddraw.h` is a stand-in with the two structures and the flags
  used, laid out as client.dll's DXT conversion shows.

The Rich headers agree otherwise: 148 C objects and 38 MASM objects (libc.lib), 118 import
objects. client.dll has 426 objects from Utc1310_CPP, ours 425, so Hidden's link had one more
C++ object; which one is not known (no cluster of unmapped functions points at it). The import
library objects differ in origin only (client.dll: 4 from Linker600 8447 and Implib710 2067 and
2179; ours: Implib710 2179, 3077 and 4035), which changes no code.

## Building client.dll

    JOBS=6 sh rebuild/msvc/build_client.sh ~/.cache/hsrebuild-msvc-<name> [file.cpp ...]

The repo is mirrored into `<work>/src`. `vcproj_to_msvc.py` writes a makefile per project:
vgui_controls.vcproj "Release|Win32" (58 files, plus /GL) into a static library through
`link /lib` (the Toolkit has no lib.exe), then client_sdk.vcproj "Release SDK|Win32" with the
switches above and the per-file settings (416 files, 43 without the precompiled header,
c_te_firebullets1.obj), then the link with the vcproj's linker settings and /MAP. Objects are
linked in project order, as the IDE did. Output in `<work>/out/msvc/client`: client.dll,
client.map, client.pdb, a `.obj.log` per file, client.dll.log, and build.tsv (file, OK or FAIL,
first error). With file arguments only those objects are rebuilt. A full build takes about half a
minute at JOBS=6.

Details:

- One vc70.pdb cannot be shared by parallel cl.exe 7.1 processes, and a /Yu compile must name the
  PDB its precompiled header was made with. So stdafx.cpp makes the precompiled header once per job
  slot (pch0 to pch<JOBS-1>, each with its own PDB), every /Yu file is assigned a slot, and compiles
  of one slot take turns under flock. stdafx.obj itself is slot 0's.
- /FR and /FD are left out (they only write side files). vgui_controls.vcproj's /YX (automatic
  precompiled headers) is left out too.
- The repo's lib/public/*.lib are Git LFS pointers. tier0.lib and vstdlib.lib are import libraries,
  so their names are all that matters: SDKLIB defaults to the 2006 SDK's lib-vc7/public
  (`~/.cache/subject51764/hsrebuild/hl2sdk-episode1/lib-vc7/public`), whose nvtc.lib is the same
  file as the 2004 SDK's (same SHA256 as the LFS pointer). vgui_controls.lib is built from our
  source.
- Make variables are MSCL and MSLINK: make exports command-line variables, and cl.exe and link.exe
  read CL and LINK from the environment as extra switches.

The first build compiled 412 of 415 files. The three MSVC errors were real ones in our source and
are fixed (LoadWeaponSprites took a C_BaseCombatWeapon * where client.dll's takes the file info
handle; vgui::SETUP_PANEL needed its namespace in hdn_hud_name.cpp) or worked around (ddraw.h).
The link had no unresolved externals.

Result: client.dll 3,223,552 bytes (client.dll 3,235,840); .text 0x238f3a (0x23b6ea), .rdata
0x6a72d (0x6a7ad), .data 0x2f000 (0x2f000).

The comparison is in CLIENT.md section 5.

## Building server.dll

    JOBS=6 sh rebuild/msvc/build_server.sh ~/.cache/hsrebuild-msvc-server-<name> [file.cpp ...]

The same pipeline as the client, on dlls/hl_sdk.vcproj "Release SDK|Win32": 441 files, stdafx.cpp
makes the precompiled header through cbase.h, 42 files compile without it. Output in
`<work>/out/msvc/server`: server.dll, server.map, server.pdb, a `.obj.log` per file, server.dll.log
and build.tsv. With file arguments (paths from the repo root) only those objects are rebuilt.

Hidden's server.dll files:

| File | Bytes | Notes |
|---|---|---|
| dll_2006-05-06_patch.dll | 4,341,760 | Has Hidden's own PDB: every function's name and extent (`procs.tsv`), the module list |
| dll_2006-08-08_4.1.dll | 4,366,336 | 4.1 release, no PDB |
| dll_2006-08-18_4b1.dll | 4,366,336 | The 4b1 release, the same build as the 4b1 server_i486.so; no PDB |

All three are under `~/.cache/subject51764/hsrebuild/builds`, and the PDB tables under
`decomp/server_pdb0506`.

Switches, from the configuration, checked against the reference:

- Compiler: /Ox /Og /Oi /Ot /G7 /GF /Gy /GR /ML /W4 /Zi, no /Ob (the client's settings), and the
  defines NDEBUG, BOTS, SDK_DLL, USES_SAVERESTORE, GAME_DLL, _WINDOWS, VECTOR, _WIN32,
  PROTECTED_THINGS_ENABLE and the sprintf, strncpy and _snprintf traps.
- Linker: /DLL /INCREMENTAL:NO /DEBUG /MAP /SUBSYSTEM:WINDOWS /BASE:0x22000000 /MACHINE:X86,
  /NODEFAULTLIB:LIBCD, winmm.lib, the IDE's default Windows libraries, tier0.lib and vstdlib.lib.
  Our imports are those of the 2006-08-18 DLL, name for name and in order: 59 from KERNEL32,
  3 from USER32, 26 from tier0, 25 from vstdlib. As for the client, nothing is folded or removed:
  link 7.1's /DEBUG defaults /OPT:NOREF and /OPT:NOICF.
- Rich header: 452 Utc1310_CPP objects, as in both August DLLs (the May DLL has 446, from an
  older file list), 107 Utc1310_C and 35 Masm710 objects from libc.lib, 6 AliasObj710, 113
  imports. Only the import library objects differ in origin (Hidden: Linker600 8447 and
  Implib710 2067 and 2179; ours: Implib710 2179, 3077 and 4035).

Link order. The IDE links in project order. The PDB's module list (modules.txt, which runs from
the last object linked to the first) has the Hidden filter in alphabetical order, sdk_bot_temp.obj
in the SDK filter, and func_monitor, grenade_tripmine and weapon_physcannon in the Hidden filter;
dlls/hl_sdk.vcproj is in that order now, so project order is Hidden's. `server_link_order.txt` is
the PDB's list, first to last, and build_server.sh also passes it to `vcproj_to_msvc.py
--link-order`; an object the PDB lacks goes after the listed object of its filter whose name
precedes it. With the vcproj as it is, both give the same 441 objects in the same order. Measured
as the share of mapped functions in the same relative order: 99.17% against the May DLL and 99.17%
against the 2006-08-18 DLL, from 98.00% and 97.43% in the project order before the Hidden
filter was sorted. The DLL is 4,366,336 bytes, the size of both August DLLs;
.text 0x2dc13a against 0x2dc1ba in the 2006-08-18 DLL, .rdata 0x0a57ad against 0x0a57dd, .data
the same.

## Comparing server.dll

    sh rebuild/msvc/compare_server.sh [--loose] <work> [Hidden's server.dll]

`msvc_compare.py` maps and compares as for the client, strict by default (constants and the data
behind addresses count, STRICT-COMPARE.md section 6; `--loose` or LOOSE=1 gives the original
scoring), with two more sources of names:

- **The May DLL** (the default) is mapped by name. `--ref-procs procs.tsv` gives every reference
  function's undecorated name, module and extent, so reference starts are taken from the PDB
  alone (a jump or pointer target inside a PDB function is not a start), and functions pair by
  name first: unique names, then by object file, then by identical bodies, then by similarity.
  The rest (inline copies in several objects, LIBC) are mapped the client's way. Writes
  `cmp_server.tsv`.
- **An August DLL** has no PDB. compare_server.sh first maps it to the May DLL (`--ours-procs`,
  the May DLL taking the place of ours) and keeps the pairs that are the same, or half alike and
  64 bytes or more; those reference functions get the May names (`names_<dll>.tsv`, cached; this
  step is always loose, so both modes start from the same names). The
  August DLL is then mapped to ours by vftables and exports first, then by the carried names,
  then the client's way. Small accessors are kept out because a class layout changed between May
  and August: CBasePlayer is 12 bytes shorter in May, so each May accessor body is equal to the
  next August one. Writes `cmp_server_<dll>.tsv`. NONAMES=1 skips the names.

Every result is also scored on the stable subset (`msvc_subset.py`): the functions whose source
did not change between Hidden's May and August .so files (`stable_may_aug.tsv`, GCC names, joined
by name and arguments), written to `cmp_server*_stable.tsv`. `NAME=<part of a name, or a reference
address; several, comma separated>` prints diffs. "Same" was checked on 300 random same pairs: the bytes differ only at base
relocations and at rel32 call and jump targets.

Results (our build against each DLL; the subset has 16111 functions, 14622 of them are functions
in our build, the rest MSVC inlined everywhere):

| Reference | Mode | Mapped | Same | Changed | Stable subset: same / changed / unmapped |
|---|---|---|---|---|---|
| May 2006 (by the PDB) | strict | 30760 of 30779 | 29989 | 771 | 14579 / 44 / 0 |
| | --loose | 30759 of 30779 | 30000 | 759 | 14585 / 38 / 0 |
| 2006-08-18 4b1 | strict | 30187 of 30701 | 30026 | 161 | 14490 / 40 / 93 |
| | --loose | 30158 of 30701 | 30007 | 151 | 14493 / 37 / 93 |
| 2006-08-08 4.1 | strict | 30185 of 30698 | 30023 | 162 | 14490 / 40 / 93 |
| | --loose | 30156 of 30698 | 30004 | 152 | 14493 / 37 / 93 |

(main f12fee2.) The functions same only in the loose mode are listed in STRICT-COMPARE.md section
7: against the August DLLs 10, all same in the strict GCC compare and none a source constant
(pointers the reference's extents or `$E` names pair wrongly); against the May DLL 12, four of
them strings of the older May source. The strict mode also maps 29 more August functions.
The counts in the next paragraph are the loose ones.

Our source is the August 4b1 source, and the May DLL was built from older source than even the
May .so: 30 of the 38 stable functions that differ from the May DLL are the same as the August
one's. Against the 2006-08-18 DLL, 29 of the 37 that differ are the same as the May one's: the
reference has no PDB, so some of its extents are cut at a tail call or run into the next function,
and a few were compiled differently from the same source. Eight differ from both (FireBullets,
CWeaponFN303::Reload, CheckBack, LevelInitPostEntity, ParseEntryExitAnims,
CBludgeonWeaponSDKBase::Hit, SampleStep, SendPropUtlVector). SERVER-STATUS.md lists the causes.
