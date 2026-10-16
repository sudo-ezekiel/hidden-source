# How the code was rebuilt

This repository is the C++ source code of Hidden: Source (beta 4b, 2006), the Half-Life 2 mod, rebuilt
from the mod's own shipped binaries after the original source was lost. The code was reconstructed and
recompiled until our builds matched Hidden's binaries function by function. `README.md` says how to
build and run it in one step; this file has the details.

## What is here

- The repository root is the mod source folder (`src_mod`) of Valve's 2004 Source SDK, with Hidden's
  code added and Valve's files changed where Hidden changed them.
- Commit 494f72c is that SDK, unmodified. It is identical to `src_mod` in
  [Source-SDK-Archives/source-sdk-2004](https://github.com/Source-SDK-Archives/source-sdk-2004) at
  commit 962ba835 (all 2282 files have the same git blob hashes). So `git diff 494f72c` shows every
  change Hidden made, as far as it has been rebuilt: 328 files today.
- Hidden also took parts of Valve's mid-2005 SDK update (IGameEventManager2, IPlayerInfo and
  IBotController, the newer navigation mesh, the HLTV director interface and a few more). Those parts
  were brought in to match the binaries.
- Hidden's own code: `dlls/hidden` (server), `game_shared/hidden` (shared by server and client: the
  weapons, game modes, spectator, laser sight) and `cl_dll/hidden` (HUD, menus, effects). Valve's SDK
  template code, as Hidden changed it, is in `dlls/sdk`, `game_shared/sdk` and `cl_dll/sdk`. Hidden's
  additions to Valve classes, such as the player's stamina, aura and stun state, are in Valve's files.
- `rebuild/`: the tools and notes used to rebuild and check the code. The game does not need them.
- `ghsi/`: GHSI's "Enhanced Edition", the community version most players use, on top of the rebuild.
  GHSI changes no code (its DLLs are Hidden's 2006 files), so this is its text files, a manifest of
  its maps, plugins and server browser fix, and `ghsi/ghsi.py` to fetch and install them with the
  rebuilt DLLs. `ghsi/README.md` lists every change.

## How close it is

**Linux server.** Built with GCC 3.4, the compiler of Hidden's Linux server, 18377 of 18413 functions
compile to the same instructions, with the same constants, as Hidden's `server_i486.so` of 18 August 2006
(beta 4b1). Every function in Hidden's file has one of the same name in ours, and ours has none Hidden's
lacks. The 36 that still differ are listed, with every form tried, in `rebuild/SERVER-STATUS.md`: register
choice, instruction scheduling and block order, plus a few functions where the form that matches one of
Hidden's compilers breaks the other. Hidden's exact compiler build (Debian gcc-3.4 3.4.5-2) gives the same
result for every function, so they are not a compiler-version effect.

**Windows server.** Built with Hidden's own compiler (Visual C++ .NET 2003, cl 13.10.3077, run under Wine),
our server.dll has the same size, imports and object counts as Hidden's server.dll of the same day. 30193 of
its 30703 functions are paired with ours: 30079 are the same and 114 differ (38 of them in Microsoft's C
runtime library). The unpaired ones are small copies of the same inline code, CRT pieces and tool limits,
apart from one 12-instruction function that nothing in Hidden's DLL calls. `rebuild/msvc/MSVC.md` and
`rebuild/SERVER-STATUS.md` have the breakdown.

**Client.** Built the same way, our client.dll is paired with Hidden's client.dll of 14 June 2006
(beta 4b): 23922 of its 24320 functions are paired, 23781 of those are the same and 141 differ (54 of them
in the C runtime). `rebuild/CLIENT.md` section 5 has the breakdown and the unpaired functions.

Both references are the official 2006 files: the server from `hidden-beta4b1-server.zip` and the
client from `hsb4b-full.zip`, as published on hidden-source.com (checked by CRC-32 against archived
copies). The code matches those files; the community's later fixes are in `ghsi/`.

"The same" means identical instructions, constants and data: the two builds place code and data at
different addresses, so an address is compared by what it points at (the function or global by name, or
the float, string or table stored there) and not by its value. The relocation tables of both builds say
which numbers are addresses (`rebuild/STRICT-COMPARE.md`). Each compare also has a `--loose` mode that
masks addresses without looking behind them, as the first versions did.

**Names.** Function, class and global variable names in the server are Hidden's own: the Linux server
kept its symbol table. Hidden's own debug file for its May 2006 server (server.pdb, from a 2006 server
patch kept by the Wayback Machine) gave the real names of members, parameters and local variables, the
source file of every function and its original line order; `rebuild/PDB.md` says what it holds and how to
read it. Code written after May has no such record: where we picked a name, a `// REBUILD:` comment says so.

**Original files.** `game_shared/hidden/hdn_lasersight.cpp` and `hdn_lasersight.h` are the team's
own files, published on hidden-source.com (dated 2005-10-22). The 2006 client.dll has one check the 2005
file lacks (no dot without an owner), so it was added, marked `// Hidden:`. The comments in those files
are Valve's, from the HL2 multiplayer RPG laser dot the team started from. Everything else Hidden wrote
is rebuilt, and no comment written by the Hidden team survives: the binaries and the PDB hold none.

**In the game.** The rebuilt client.dll and server.dll were run together on 16 October 2026 (Steam's
Source SDK Base under Proton, listen server on ovr_docks with bots): the client connects, bots join,
rounds restart, and the team and weapon menus draw the marine model. The first run found one fault, a
receive table declared with the wrong array form (`DT_SDKGameRules`), and it is fixed. Two things seen in
that test still have to be compared against Hidden's original DLLs: the radio and taunt menu sits off
centre at 2560x1440, and the Support class's weapon choices show no tick. The code of both menus is the
same as client.dll's, so the original may well do the same. The Linux server has not been run yet.

## Building on Windows with Visual Studio .NET 2003

This is the toolchain Hidden used. Open `Game_SDK.sln`, pick the "Release SDK" configuration and
build both projects, `client` (cl_dll/client_sdk.vcproj) and `hl` (dlls/hl_sdk.vcproj). They write
`cl_dll\Release_SDK\client.dll` and `dlls\Release_SDK\server.dll`.

Set the environment variable `HIDDEN_DIR` to the mod's folder (the one with gameinfo.txt) and each
build also copies its DLL and PDB into `%HIDDEN_DIR%\bin`. Without it, nothing is copied.

The projects link the prebuilt libraries in `lib/public`. The SDK archive stores them with Git LFS,
so here they are small pointer files and need replacing. The tested Wine build below uses tier0.lib,
vstdlib.lib and nvtc.lib from the 2006 SDK's `lib-vc7/public`
([alliedmodders/hl2sdk](https://github.com/alliedmodders/hl2sdk), branch episode1), and a
vgui_controls.lib compiled from `vgui2/controls/vgui_controls.vcproj` ("Release"). Copy those into
`lib/public`. The 2004 originals can also be fetched from the archive; they have not been tried:

```bash
GIT_LFS_SKIP_SMUDGE=1 git clone https://github.com/Source-SDK-Archives/source-sdk-2004.git
```

```bash
git -C source-sdk-2004 lfs pull --include="src_mod/lib/public/*.lib"
```

This route has not been tried in Visual Studio itself. The Wine build below uses the same project
files and switches and is tested.

## Building on Linux

`rebuild/install.sh` runs the three builds below side by side and installs the results in the mod
folder (`README.md`). Each build on its own:

**Server (GCC 3.4 in Docker).** Make the `hsrebuild-gcc34` image first: `rebuild/docker/README.md`
lists the Debian 4.0 packages and how each was checked, and how to add Hidden's exact
compiler build as a second image (`GCC_IMAGE=hsrebuild-gcc345`). Then:

```bash
JOBS=6 sh rebuild/build.sh ~/.cache/hsrebuild-work
```

The result is `~/.cache/hsrebuild-work/out/server_i486.so`. To compare it with Hidden's:

```bash
sh rebuild/compare.sh /path/to/hidden/bin/server_i486.so ~/.cache/hsrebuild-work
```

**Client and Windows server (Visual C++ .NET 2003 under Wine).** `rebuild/msvc/MSVC.md` lists the two Microsoft
downloads (Visual C++ Toolkit 2003 1.01 and a Platform SDK image) with their SHA1 hashes.
`rebuild/msvc/unpack.sh` unpacks them without running an installer. Then:

```bash
JOBS=6 sh rebuild/msvc/build_client.sh ~/.cache/hsrebuild-msvc
```

```bash
sh rebuild/msvc/compare_client.sh ~/.cache/hsrebuild-msvc
```

server.dll builds and compares the same way, against Hidden's server.dll:

```bash
JOBS=6 sh rebuild/msvc/build_server.sh ~/.cache/hsrebuild-msvc-server
```

```bash
sh rebuild/msvc/compare_server.sh ~/.cache/hsrebuild-msvc-server /path/to/hidden/bin/server.dll
```

The build reads three environment variables: `MSVC71` (the unpacked tree), `WINEPREFIX`, and
`SDKLIB`, a folder with tier0.lib, vstdlib.lib and nvtc.lib such as the 2006 SDK's `lib-vc7/public`.
tier0.lib and vstdlib.lib are import libraries, so only their names matter, and nvtc.lib is the same
file in both SDKs. The client build makes vgui_controls.lib from `vgui2/controls`.

## The Windows installer for players

`installer/build.sh` makes `HiddenSourceInstaller.exe`, one file of about 760 MB that installs the game
on Windows: the mod built from this source with GHSI's Enhanced Edition files (`ghsi/`), and the server
browser fix in Source SDK Base. It needs the builds above and Go (1.22 or later):

```bash
sh installer/build.sh
```

It builds the three binaries with `rebuild/build_all.sh` (`SKIP_BUILD=1` uses the last ones), stages
the mod as `ghsi.py client` installs it, compiles `installer/main.go` for 32-bit Windows and appends
the files to it. On the player's PC the installer finds Steam in the registry, asks Steam to install
Source SDK Base 2006 if it is missing and waits for it, installs the mod in Steam's sourcemods folder,
applies the server browser fix with a backup (the same layout as `ghsi.py`, so either one can undo
it), and offers to restart Steam so the game shows in the library. `HiddenSourceInstaller.exe
-undo-browser-fix` puts Source SDK Base's own files back.

On Linux the same .exe runs under Wine (open it, or `wine HiddenSourceInstaller.exe`). It then installs
for Linux's own Steam, which runs the game with Proton: it finds Steam in the home folder
(`~/.steam/steam`, `~/.local/share/Steam`, Flatpak's or Snap's), reads the Linux paths of its library
folders, and asks Steam for Source SDK Base through `winebrowser`. It does not restart Linux's Steam;
it says to.

Tested under Wine with a stand-in Steam, Windows and Linux layouts both: the installed mod equals the
staged one file for file, the browser fix equals GHSI's, a second run over an installed game works,
and both undos give back the original files.

## Hammer and the other SDK tools

Hammer, the map compilers (vbsp, vvis, vrad), studiomdl and hlmv are in the `bin` folder of the same
[2004 SDK archive](https://github.com/Source-SDK-Archives/source-sdk-2004), also stored with Git
LFS. This repository holds only the code.

## Notes in `rebuild/`

- `SERVER-STATUS.md`: the server, and the functions that still differ.
- `PDB.md`: Hidden's May 2006 server PDB and the reader for it.
- `STRICT-COMPARE.md`: what the comparisons check, on Linux and Windows.
- `CLIENT.md`: the client, its checks and the comparison with Hidden's client.dll.
- `msvc/MSVC.md`: the 2003 compiler, where it comes from and how it runs under Wine.
- `tools/`: the comparison and analysis scripts the notes refer to.
