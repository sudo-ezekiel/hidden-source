# Hidden: Source, rebuilt source code

This is the C++ source code of Hidden: Source (beta 4b, 2006), the Half-Life 2 mod, rebuilt from
the mod's own shipped binaries after the original source was lost. The code was reconstructed and
recompiled until our builds matched Hidden's binaries function by function.

It is a private copy for the Hidden: Source team. Please do not publish it.

## What is here

- The repository root is the mod source folder (`src_mod`) of Valve's 2004 Source SDK, with Hidden's
  code added and Valve's files changed where Hidden changed them.
- Commit 494f72c is that SDK, unmodified. It is identical to `src_mod` in
  [Source-SDK-Archives/source-sdk-2004](https://github.com/Source-SDK-Archives/source-sdk-2004) at
  commit 962ba835 (all 2282 files have the same git blob hashes). So `git diff 494f72c` shows every
  change Hidden made, as far as it has been rebuilt: 293 files today.
- Hidden also took parts of Valve's mid-2005 SDK update (IGameEventManager2, IPlayerInfo and
  IBotController, the newer navigation mesh, the HLTV director interface and a few more). Those parts
  were brought in to match the binaries.
- Hidden's own code: `dlls/hidden` (server), `game_shared/hidden` (shared by server and client: the
  weapons, game modes, spectator, laser sight) and `cl_dll/hidden` (HUD, menus, effects). Valve's SDK
  template code, as Hidden changed it, is in `dlls/sdk`, `game_shared/sdk` and `cl_dll/sdk`. Hidden's
  additions to Valve classes, such as the player's stamina, aura and stun state, are in Valve's files.
- `rebuild/`: the tools and notes used to rebuild and check the code. The game does not need them.

## How close it is

**Server.** Built with GCC 3.4, the compiler of Hidden's Linux server, 18364 of 18413 functions
compile to the same instructions as Hidden's `server_i486.so` of 18 August 2006 (beta 4b1). The 49
that still differ are listed, with every form tried, in `rebuild/SERVER-STATUS.md`; nearly all come
down to register and ordering choices of the compiler.

**Client.** Built with Hidden's own compiler (Visual C++ .NET 2003, cl 13.10.3077, run under Wine),
our client.dll is paired with Hidden's client.dll of 14 June 2006 (beta 4b): 23623 of its 24320
functions are paired, 23263 of those are the same and 360 differ. `rebuild/CLIENT.md` section 5 has
the breakdown.

"The same" means identical instructions with addresses masked: the two builds place code and data at
different addresses, so calls and pointers may differ in their target address and in nothing else.

**Names.** Function, class and global variable names in the server are Hidden's own: the Linux server
kept its symbol table. Local variables, parameters and some members had no names in the binaries;
where we picked one, a `// REBUILD:` comment says so.

**Original files.** `game_shared/hidden/hdn_lasersight.cpp` and `hdn_lasersight.h` are the team's
own files, published on hidden-source.com (dated 2005-10-22) and copied unchanged. Everything else
Hidden wrote is rebuilt.

**Not done yet.** The rebuilt DLLs have not been run in the game.

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

**Server (GCC 3.4 in Docker).** Make the `hsrebuild-gcc34` image first: `rebuild/docker/README.md`
lists the Debian 4.0 packages and how each was checked. Then:

```bash
JOBS=6 sh rebuild/build.sh ~/.cache/hsrebuild-work
```

The result is `~/.cache/hsrebuild-work/out/server_i486.so`. To compare it with Hidden's:

```bash
sh rebuild/compare.sh /path/to/hidden/bin/server_i486.so ~/.cache/hsrebuild-work
```

**Client (Visual C++ .NET 2003 under Wine).** `rebuild/msvc/MSVC.md` lists the two Microsoft
downloads (Visual C++ Toolkit 2003 1.01 and a Platform SDK image) with their SHA1 hashes.
`rebuild/msvc/unpack.sh` unpacks them without running an installer. Then:

```bash
JOBS=6 sh rebuild/msvc/build_client.sh ~/.cache/hsrebuild-msvc
```

```bash
sh rebuild/msvc/compare_client.sh ~/.cache/hsrebuild-msvc
```

The build reads three environment variables: `MSVC71` (the unpacked tree), `WINEPREFIX`, and
`SDKLIB`, a folder with tier0.lib, vstdlib.lib and nvtc.lib such as the 2006 SDK's `lib-vc7/public`.
tier0.lib and vstdlib.lib are import libraries, so only their names matter, and nvtc.lib is the same
file in both SDKs. The client build makes vgui_controls.lib from `vgui2/controls`.

## Hammer and the other SDK tools

Hammer, the map compilers (vbsp, vvis, vrad), studiomdl and hlmv are in the `bin` folder of the same
[2004 SDK archive](https://github.com/Source-SDK-Archives/source-sdk-2004), also stored with Git
LFS. This repository holds only the code.

## Notes in `rebuild/`

- `SERVER-STATUS.md`: the server, and the functions that still differ.
- `CLIENT.md`: the client, its checks and the comparison with Hidden's client.dll.
- `msvc/MSVC.md`: the 2003 compiler, where it comes from and how it runs under Wine.
- `tools/`: the comparison and analysis scripts the notes refer to.
