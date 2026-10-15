# MSVC 7.1 toolchain for client.dll

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
| InlineFunctionExpansion | 0 | none (the IDE's "Default"), to confirm |
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

To check against the reference before trusting a full build:

- InlineFunctionExpansion 0: whether the IDE passed nothing (so /Ox's /Ob2 applies) or /Ob0.
- Whether the IDE also linked its default Windows libraries (kernel32.lib, user32.lib and so on);
  the reference's import table answers it.
- No /OPT is set: whether the reference shows unreferenced COMDATs removed or identical
  functions folded.

## Next

Build every source with these switches under Wine, link with /MAP, map our functions to the
reference's (the `--ours-map` mode of the vtable tools in CLIENT.md section 2), and diff function
by function the way the server lanes did.
