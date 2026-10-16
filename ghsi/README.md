# GHSI on top of the rebuild

[GHSI](https://github.com/GeeTwentyFive/GHSI) ("Gee's Hidden: Source Installer") installs what it
calls Hidden: Source - Enhanced Edition, the version most players use today. This folder holds all of
it, so the rebuilt code plus this folder gives the Enhanced Edition. It was taken from GHSI V11 (commit
1a51561, 9 March 2026) for the game files and the server browser fix, and from V10 (9 December 2025)
for the dedicated server package, the newest release that has one.

**GHSI changes no code.** The client.dll it ships is beta 4b's of 14 June 2006 and its server.dll and
server_i486.so are beta 4b1's of 18 August 2006, byte for byte: the same files this rebuild matches.
Its game files are `hsb4b-full.zip` (beta 4b, June 2006, without the 2007 `hsb4b-update.zip`) plus the
changes below. Everything GHSI does is in text files, maps, a server browser fix for Source SDK Base,
SourceMod plugins and two launchers.

## The changes

Commit 5c4721f holds the 2006 originals (and Source SDK Base's) of every text file GHSI changes, and
the commit after it holds GHSI's versions, so this shows every text change GHSI made:

```bash
git diff 5c4721f -- ghsi/hidden ghsi/server ghsi/sdkbase
```

| GHSI change | Where |
|---|---|
| Shotgun damage 17 to 11 per pellet (9 pellets) | `hidden/scripts/weapon_shotgun.txt` |
| FN303 clip 15 to 20 | `hidden/scripts/weapon_fn303.txt` |
| Sonic trip alarms 1 to 4 | `hidden/scripts/weapon_sonic.txt` (`default_clip`) |
| Sonic alarm sounds fixed | `hidden/scripts/game_sounds_manifest.txt`: weapon sounds are preloaded instead of precached |
| Client settings: network rates, `cl_pred_optimize 0` (recoil at high ping), `cl_extrapolate 0`, binds, video, mouse and sound settings, `cl_downloadfilter all` | `hidden/cfg/GHS_config.cfg`, run by `hidden/cfg/autoexec.cfg` and by the last line of `hidden/cfg/config_default.cfg` |
| Game name "HIDDEN: SOURCE - Enhanced Edition" (the launcher finds the window by it) | `hidden/gameinfo.txt` |
| Community Discord link in the message of the day | `hidden/motd.txt` |
| Listen server settings: `sv_lan 0`, `sv_master_legacy_mode 0`, 128-tick rates, Hidden's cvars with notes | `hidden/cfg/server.cfg` |
| Dedicated server settings: the same, plus `sv_alltalk`, `mp_chattime` and an rcon note | `server/hidden/cfg/server.cfg` |
| hdn_decay and hdn_origin replaced by fixed builds (`hdn_decay_fixed`, `hdn_origin_fixed`); the dedicated server's map cycle uses them, and ovr_executive in place of ovr_decay, which does not exist | `manifest.tsv`, `server/hidden/mapcycle.txt`, `server/hidden/maplist.txt` |
| 107 community maps, some in several versions | `manifest.tsv` |
| Server list working again | Source SDK Base, see below |
| Server plugins | the dedicated server package, see below |
| Launchers: stuck `hl2.exe`, UPnP port 27015, borderless window | `launchers/` |

## Installing: ghsi.py and manifest.tsv

Maps, the server browser files and the plugins are binary, and the maps alone are about 1 GB, so they
are not in git. `manifest.tsv` lists each file GHSI adds or changes that is not text here, with its
sha256 and size. `ghsi.py` downloads GHSI's three files from GitHub (checked by sha256: 1.0 GB, 4.5 MB
and 1.7 GB), unpacks them without running anything from them, and installs. It needs Python 3.12 or
later, and 7-Zip (`7z`) for the server package.

```bash
python3 -I ghsi/ghsi.py fetch
```

The mod, with the rebuilt DLLs in place of Hidden's (each `--bin` is optional):

```bash
python3 -I ghsi/ghsi.py client ~/.local/share/Steam/steamapps/sourcemods --bin ~/.cache/hsrebuild-msvc/out/msvc/client/client.dll --bin ~/.cache/hsrebuild-msvc-server/out/msvc/server/server.dll
```

The server browser fix, in the Source SDK Base folder (the Steam library that holds app 215):

```bash
python3 -I ghsi/ghsi.py sdkbase "/path/to/SteamLibrary/steamapps/common/Source SDK Base"
```

The Windows dedicated server, in a folder of its own:

```bash
python3 -I ghsi/ghsi.py server ~/hidden-server --bin ~/.cache/hsrebuild-msvc-server/out/msvc/server/server.dll
```

`client` and `server` give exactly GHSI's files apart from the DLLs given with `--bin` and the two
map lists below; this was checked file by file against GHSI's downloads. Editing a file here (a weapon script, say) and running
`client` again installs the edited file. `sdkbase` keeps every file it replaces in
`Source SDK Base/ghsi-backup`, and `sdkbase --restore` puts them back and removes what it added. If
Steam verifies or updates Source SDK Base it puts its own files back: run `--restore`, then `sdkbase`
again.

## The server browser fix

The server list belongs to Source SDK Base, not to the mod, which is why the 2006 game shows no
servers: it asks master servers that were shut down. GHSI's fix has three parts.

1. **engine.dll, 7 bytes.** Two instructions are replaced with NOPs. One is the call that printed
   "A2C_PRINT from %s : %s" for every remote print packet. The other is in the function behind
   `sv_master_legacy_mode` ("Use old (outside-of-Steam) code to communicate with master servers",
   default 1): it is the jump that obeys the cvar, so the engine always registers with Steam's
   master servers. That is what puts a listen server in the list. `ghsi.py` applies the 7 bytes to
   the installed engine.dll (Source SDK Base build 124232, checked by sha256) and checks that the
   result is GHSI's engine.dll.
2. **bin/ServerBrowser.dll.** GHSI replaces Source SDK Base's (February 2009) with one built in July
   2017 from Valve's Episode One code by someone outside Valve (its debug path is
   `c:\Documents and Settings\Administrator\Desktop\ep1final\serverbrowser\...`). It lists servers
   through Steam's matchmaking API (`SteamMatchMakingServers001`). It is not Valve's file and nothing
   in this repository can rebuild it; it comes from GHSI's download, checked against `manifest.tsv`.
3. **platform/.** The resource files that browser needs (333 files), and the three configs in
   `sdkbase/platform/config`: `masterservers.vdf` drops 208.64.200.65:27015, `serverbrowser.vdf`
   adds `"gametype" "notags"` filters and a Custom Games tab, `ingamedialogconfig.vdf` lays out that
   tab's columns.

A dedicated server needs none of this: srcds's engine.dll is Valve's, unpatched, and its `server.cfg`
sets `sv_master_legacy_mode 0`.

## The dedicated server package

GHSI's `HiddenServer.7z` is a Windows dedicated server (srcds for Source SDK Base 2006) with
Metamod:Source and SourceMod 1.11, started by `GHSServerLauncher.exe` or `RUN_AS_CONSOLE.bat`. Most of
it is stock. What GHSI added:

- Hidden plugins (in `manifest.tsv`), the H:SM set by Paegus (built 2009 to 2014, from the AlliedModders and Hidden forums):
  - `hsm_autobot`: adds a bot to an empty or one-player server.
  - `hsm_bhc`: lets an admin check a player for the "black Hidden" exploit.
  - `hsm_blackdeathfix`: stops the black screen after a fall death.
  - `hsm_visibility`: `+visible` lets the Hidden show himself.
  - `hsm_weighter`: prints each player's chance of being the next Hidden at round end.
  - `hsm_pigshove`: the beta 5 pigshove. It ships in `plugins/disabled`, so it is off.
- `server/hidden/addons/sourcemod/plugins/hdn_physcrash.sp`, a fix for the crash when a dead Hidden
  picks up objects. GHSI ships only this source file in the plugins folder, and SourceMod loads only
  compiled `.smx` files, so the fix never runs on GHSI servers.
- `server/hidden`: the map cycle, map vote and rock-the-vote settings, the translations for pigshove
  and the weight reporter, and `metamod.vdf`.

SourceMod runs only on the dedicated server, so on a listen server none of these plugins run. GHSI
packages no Linux server.

## Launchers

`launchers/` has GHSI's two launchers, unchanged (GPL-3.0, see `launchers/LICENSE`), from
[GHSLauncher](https://github.com/GeeTwentyFive/GHSLauncher) and
[GHSServerLauncher](https://github.com/GeeTwentyFive/GHSServerLauncher). Both are Windows only (they
use pywin32 and miniupnpc). GHSLauncher kills a stuck `hl2.exe`, opens port 27015 by UPnP, starts
Source SDK Base with the mod and `-console`, then removes the window's borders. GHSServerLauncher
opens the port and starts srcds with `-tickrate 128`.

## Notes

- **One change that is not GHSI's.** GHSI fixed the map lists only in its dedicated server package.
  Its client package still names hdn_decay, hdn_origin and ovr_decay, which a fresh GHSI install
  does not have, so a listen server's map cycle runs into missing maps. `hidden/mapcycle.txt` and
  `hidden/maplist.txt` here are the dedicated server's lists.

- `hidden-4a`: GHSI also puts 39 beta 4a voice files next to the mod in `sourcemods`. `gameinfo.txt`
  does not mount that folder, so the game does not use them. `ghsi.py client` installs it as GHSI
  does.
- 12 `.ztmp` files in `maps` are leftovers of map downloads; they are installed as GHSI ships them.
