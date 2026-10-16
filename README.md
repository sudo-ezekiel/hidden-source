# Hidden: Source, rebuilt source code

Hidden: Source's C++ source code was lost, so this project rebuilt it from the mod's own 2006 binaries
(beta 4b) to let the game be developed again.

## Running it

You need Steam with Source SDK Base 2006 (app 215, in the library's Tools), and a Linux machine with
Wine, Docker and the two compilers set up as `REBUILD.md` describes. Then, from this folder:

```bash
sh rebuild/install.sh ~/.local/share/Steam/steamapps/sourcemods
```

That builds client.dll, server.dll and the Linux server from this source and installs them in
`sourcemods/hidden`. The first time, it also installs the files of GHSI's Enhanced Edition, the
community version most players use (`ghsi/`). Once, for the in-game server list:

```bash
python3 -I ghsi/ghsi.py sdkbase "/path/to/SteamLibrary/steamapps/common/Source SDK Base"
```

Restart Steam and the game is in the library as "HIDDEN: SOURCE - Enhanced Edition". On Linux it runs
under Proton.

## More

- `REBUILD.md`: how the code was rebuilt, how close it is to Hidden's binaries, and how to build each
  part on its own, on Linux or with Visual Studio .NET 2003 on Windows.
- `ghsi/README.md`: every change GHSI makes, and the tool that installs them.
