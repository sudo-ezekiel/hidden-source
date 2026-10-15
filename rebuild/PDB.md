# Hidden's server PDB (May 2006)

## What it is

`server.pdb` is the Visual C++ 7.1 program database Hidden's developer built with the Windows server
`server.dll` of 2006-05-06. It came from `hidden-server-patch.zip`, a server patch hidden-source.com
offered in June 2006, recovered from the Wayback Machine's copy. The PDB's GUID and age match the
patch's `server.dll` exactly. Local copies:

- `~/.cache/subject51764/hsrebuild/builds/server_2006-05-06_patch.pdb` (20 MB, MSF 7.00)
- `~/.cache/subject51764/hsrebuild/builds/dll_2006-05-06_patch.dll` (image base 0x22000000)

It is the MAY 2006 server. This tree reproduces the AUGUST 2006 Linux build (4b1). The May Linux
build (`so_2006-05-07_patch.so`) used the same toolchain as August, and 17585 functions compile the
same against both, so most of the code did not change between May and August. Where it did
(player.cpp, gamemovement.cpp, baseplayer_shared.cpp, sdk_gamerules.cpp, sdk_player.cpp,
hltvdirector.cpp above all), the PDB describes the older code.

What it holds: 600 modules in link order (paths `c:\hl2mods\hidden\...`), 30634 procedures with
their type, 55841 named parameters and locals with their types and frame offsets or registers,
file statics and globals, 1695 constants, 2356 typedefs, C11 line numbers for every module, and the
full type information (classes with members, offsets, methods and vtable slots; enums with values).

## Reading it

`rebuild/tools/pdb_read.py` reads it with the Python 3 standard library (llvm-pdbutil rejects this
TPI version and crashes on the symbol streams). The PDB path defaults to the copy above.

    python3 -I rebuild/tools/pdb_read.py modules                 # link order, object, source file, procs
    python3 -I rebuild/tools/pdb_read.py functions               # va, size, module, file, lines, signature
    python3 -I rebuild/tools/pdb_read.py locals CHiddenSelector::SelectHidden
    python3 -I rebuild/tools/pdb_read.py class CBasePlayer       # layout, methods and vtable slots
    python3 -I rebuild/tools/pdb_read.py enum USE_TYPE
    python3 -I rebuild/tools/pdb_read.py globals
    python3 -I rebuild/tools/pdb_read.py constants
    python3 -I rebuild/tools/pdb_read.py lines hdn_hiddenselection

`--pdb` reads another PDB, `--base` sets another image base. A class or enum name that matches
nothing lists the names that contain it (case-insensitive).

Things to know when reading the output:

- Line numbers are those of the May source files. A function's file is the file that holds its first
  instruction, and first/last are the lowest and highest line of that file inside the function, so
  an inline function from a header shows the header and its lines there.
- In `locals`, `frame+N` is an S_BPREL32 offset from the frame base. Most functions omit the frame
  pointer, and there the base is the return address: the first stack parameter is at `frame+4`.
  The first records of a function are its parameters (`this` first), the rest are locals; a release
  build keeps only the variables the optimiser left a home for, and two variables may share a slot.
- S_REGISTER register numbers in this PDB are one above the documented CV_HREG_e values. Checked
  against the code: `this` (passed in ecx) is recorded as 19, SetBit's pBuf and nBit (edx, ecx) as
  20 and 19, write_char's ch (al) as 2, _abstract_cw's cw (bx) as 13. The reader corrects for it.
  S_REGREL32 uses the documented numbers (ebp 22, esp 21).
- `class` gives the vtable offset of every virtual: the PDB stores it only where a method introduces
  the slot, so for an override the reader finds the introducing base (`from CBaseEntity`).

## How the reader was checked

- `functions`: the va, size, G/L and module of all 30634 procedures agree with procs.tsv, written
  earlier by an independent scratch script.
- Vtables: the slot of every virtual method of CBaseEntity (158), CBasePlayer (199),
  CBaseCombatWeapon (136), CBaseAnimating (41) and CSDKPlayer (23) holds that method in Ghidra's
  vtables of the same DLL (with the PDB applied). The one exception per class is the deleting
  destructor, which the PDB calls `__vecDelDtor` and Ghidra `scalar_deleting_destructor`.
- Members: for 90 getters of CBaseEntity, CBasePlayer, CBaseCombatWeapon, CBaseAnimating,
  CBaseCombatCharacter and CSDKPlayer that Ghidra decompiles as `return this->m_x;`, the offset the
  instruction reads in server.dll equals the member offset the reader prints.
- Lines: the functions of hdn_hiddenselection.cpp and weapon_sonic.cpp come out in a plausible
  source order with plausible lengths, and the NetworkVar helpers of CWeapon_Sonic land in
  `game_shared\hidden\weapon_sonic.h` lines 39 to 47, one per CNetworkVar.
