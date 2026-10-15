# Strict server compare

`rebuild/compare.sh` (through `rebuild/tools/compare_builds.py`) now compares the constants and the
data behind every address, not only the instructions. This file says what it checks, what it found
on main, and what the client side would need for the same.

## 1. What changed in the comparison

The original normalisation (now `--loose`) replaced every hex number above 0xFFFF with `ADDR`. That
hid addresses, as intended, but it also hid:

- large immediates that are not addresses: trace masks (`$0x46004003`), FCAP flags (`0x80000000`),
  floats stored as bit patterns (`$0x42780000` is 62.0f);
- everything an address points at: float and double constants in .rodata, string literals, jump
  tables;
- two things nobody had noticed. A call to an exported function goes through an R_386_PC32
  relocation whose field holds -4, so objdump shows its target inside the calling instruction and
  the loose compare read every such call (58565 in Hidden's .so) as a call to the calling function
  itself. A reference to an exported global goes through R_386_32 with the addend in the field, so
  `g_pGameRules` and `gpGlobals` both read as `0x0`. The callee and the global were never compared.

Both builds are non-PIC shared objects with text relocations (DT_TEXTREL), so the dynamic
relocation table says exactly which operand bytes hold an address. Hidden's .so has 147918
relocations inside .text: 47984 R_386_PC32 and 43241 R_386_32 against symbols it defines, 10581 and
2403 against imported ones, 43709 R_386_RELATIVE (19375 into .bss, 3925 into .data, 18766 into
.rodata, 1643 into .text). The strict mode (`rebuild/tools/elf_refs.py`) turns each relocated
operand into a token and keeps every other number literally, however large:

| What the operand points at | Token | Example |
|---|---|---|
| a function (call, jump, function pointer) | its name | `call <_ZN11CBaseEntity12SetClassnameEPKc>` |
| a jump target inside the function | offset from the function start | `je <+0x64>` |
| a symbol (global, static, vtable, typeinfo) | name plus offset | `$_ZTV11CBasePlayer+0x8`, `g_VProfCurrentProfile+0x1010` |
| a read-only object with a symbol and no relocations inside | name plus a hash of its bytes | `nanmask#fe034214` |
| read-only data without a symbol, x87 operand | the value by operand size | `flds f32:0.5`, `fldl f64:0.01` |
| read-only data without a symbol, sized integer read | the value | `mov w32:0x706f6f6c,%edx` |
| a C string | the string (a `__FILE__` path relative to the tree) | `mov $"player",%ecx` |
| a table of addresses (jump tables) | each entry, as offsets into the function | `jmp *[+0x3e4,+0x3af,...](,%eax,4)` |
| other read-only data | its bytes up to the next symbol or referenced address | `ro:0200...` |
| writable data without a symbol (one past the end of an array) | the symbol before it plus offset | `<array>+<its size>` |

Notes:

- Symbols with several names at one address (aliases) are named the same way on both sides:
  functions and objects before untyped labels, global before weak before local, then the shortest
  name. Anonymous namespaces go through `canon()` as before.
- `__FILE__` strings differ only because of where the compiler ran: Hidden compiled from `dlls/`
  (`../dlls/../tier1/mempool.cpp`), our Docker build from `/src` (`/src/tier1/mempool.cpp`). Both
  are reduced to the tree-relative path (`tier1/mempool.cpp`). This was the one group of tool
  artefacts: without it, SelectWeightedSequence, the two CMemoryPool constructors and
  CWeaponShotgun::PrimaryAttack were "changed". (Building with Hidden's relative paths in
  `build.sh` would make the bytes identical too; that file is outside this lane.)
- The C runtime's PIC helpers (`frame_dummy`, `__do_global_dtors_aux`, ...) address the GOT through
  `%ebx`; in functions that call `__i686.get_pc_thunk.bx` large numbers are still masked.
- The reference's strict disassembly is cached in `~/.cache/hsrebuild-disasm` like before; the
  strict key includes a hash of `compare_builds.py` and `elf_refs.py`, so a tool change never
  reuses a stale cache. The loose key is unchanged (`v2`), its content is the same as before.

Usage:

    sh rebuild/compare.sh <Hidden's server_i486.so> <work dir>            # strict (default)
    sh rebuild/compare.sh --loose <Hidden's server_i486.so> <work dir>    # the original comparison
    python3 -I rebuild/tools/fn_diff.py <ref.so> <ours.so> "<name>" [--loose]

`fn_diff.py` prints the strict tokens with demangled names, so a changed constant reads directly:

    -or       $0x80000010,%eax
    +or       $0x40000010,%eax

A full strict compare takes about 9 seconds with a cold cache, 4.6 seconds with the reference
cached (loose: 1.8 seconds).

## 2. Checks

| Run | same | changed | only-ref | only-cand |
|---|---|---|---|---|
| main (c5bc12d), `--loose` | 18364 | 49 | 0 | 0 |
| main (c5bc12d), strict | 18346 | 67 | 0 | 0 |
| Hidden's 4b1 .so against itself, strict | 18413 | 0 | 0 | 0 |
| May .so (`so_2006-05-07_patch.so`) against 4b1, `--loose` | 17606 | 596 | 211 | 12 |
| May .so against 4b1, strict | 17602 | 600 | 211 | 12 |

- `--loose` on main writes a `cmp.tsv` byte-identical to `~/.cache/hsrebuild-main/cmp_main_eac94d2.tsv`.
- Every one of the 147918 relocations in .text lands in an instruction and is matched to an operand.
- The May build drops by 4, all real: the spectator send table names `m_pLocation` where 4b1 has
  `m_Location`, the game description is "Hidden : Source B3", and the nav mesh generator uses
  `info_player_start` where 4b1 uses `info_marine_spawn` (two functions).
- No function went from changed to same. Of the 18 that went from same to changed, none differs
  in which function it calls or which global it uses: the R_386_PC32 and R_386_32 blind spot hid
  nothing on main.

## 3. Same in loose mode, changed in strict mode (main at c5bc12d)

18 functions. "Hidden" is the value in Hidden's 4b1 .so, "ours" the value in our build. None is
fixed in this lane; the line is where the constant comes from in our tree.

| Group | Functions |
|---|---|
| integer constant | 2 |
| float constant (stored as an immediate bit pattern) | 5 |
| float constant (.rodata) | 4 |
| string literal | 5 |
| symbol referenced (a different entry of a data table) | 3 |
| jump table | 0 |
| other | 0 |

Several rows share one cause: the three `CPlayerState` rows are one missing prediction field, and
the four `RandomFloat( 8, 12 )` rows are one water splash scale.

### Integer constant

| File | Function | Hidden | Ours | Likely source |
|---|---|---|---|---|
| sdk_player_shared.cpp | CSDKPlayer::FireBullet | `0x46004003` (MASK_SHOT) | `0x4600400b` (MASK_SOLID\|CONTENTS_DEBRIS\|CONTENTS_HITBOX) | game_shared/sdk/sdk_player_shared.cpp:78, the first UTIL_TraceLine mask: `MASK_SHOT` |
| hdn_ammocrate.cpp | CHDNAmmoCrate::ObjectCaps | `\| 0x80000010` (FCAP_IMPULSE_USE\|FCAP_DONT_SAVE) | `\| 0x40000010` (FCAP_IMPULSE_USE\|FCAP_WCEDIT_POSITION) | dlls/hidden/hdn_ammocrate.cpp:30: `FCAP_DONT_SAVE` instead of `FCAP_WCEDIT_POSITION` |

### Float constant stored as an immediate

| File | Function | Hidden | Ours | Likely source |
|---|---|---|---|---|
| gamerules.cpp | global constructors keyed to g_Language | `0x42780000` (62.0) | `0x42800000` (64.0) | game_shared/gamerules.cpp:35, g_DefaultViewVectors m_vView: `Vector( 0, 0, 62 )` |
| gamemovement.cpp | CGameMovement::CheckWaterJump | RandomFloat( 8, 10 ) | RandomFloat( 8, 12 ) | game_shared/gamemovement.cpp:875, the watersplash `data.m_flScale` |
| gamemovement.cpp | CGameMovement::CheckFalling | RandomFloat( 8, 10 ) | RandomFloat( 8, 12 ) | game_shared/gamemovement.cpp:3363, the same splash scale |
| baseplayer_shared.cpp | CBasePlayer::UpdateStepSound (two sites) | RandomFloat( 8, 10 ) | RandomFloat( 8, 12 ) | game_shared/baseplayer_shared.cpp:481 and :491 |

The four splash scales are the same `10` where we have `12`; the RandomFloat arguments are passed
as float bit patterns, so only the strict compare sees them.

### Float constant in .rodata

| File | Function | Hidden | Ours | Likely source |
|---|---|---|---|---|
| takedamageinfo.cpp | CalculateMeleeDamageForce | `fmuls 100` | `fmuls 300` | game_shared/takedamageinfo.cpp:283, `ImpulseScale( 75, 4 )` folds to 300 (75 * 4); Hidden's folds to 100 |
| basegrenade_shared.cpp | CBaseGrenade::Detonate | `flds -32` | `flds -16` | game_shared/basegrenade_shared.cpp:261, `vecSpot + Vector( 0, 0, -32 )` |
| weapon_physcannon.cpp | CGrabController::UpdateObject | `fadds 18` | `fadds 24` | dlls/hl2_dll/weapon_physcannon.cpp:945, `float distance = 18 + ( radius * 2.0f )` |
| base_playeranimstate.cpp | CBasePlayerAnimState::ComputePoseParam_BodyYaw | `flds 0.5` | `flds 2` | game_shared/base_playeranimstate.cpp:739, `vel.Length2D() > 0.5` |

### String literal

| File | Function | Hidden | Ours | Likely source |
|---|---|---|---|---|
| game.cpp | global constructors keyed to displaysoundlist | `"30"` | `"10"` | dlls/game.cpp:29, `decalfrequency` default |
| weapon_physcannon.cpp | global constructors keyed to g_debug_physcannon | `"7500"` | `"1000"` | dlls/hl2_dll/weapon_physcannon.cpp:50, `player_throwforce` default |
| gamevars_shared.cpp | global constructors keyed to mp_forcecamera | `"1"` | `"0"` | game_shared/gamevars_shared.cpp:28, `mp_friendlyfire` default |
| gib.cpp | CGib::SpawnStickyGibs | `"models/gibs/hgibs_rib.mdl"` | `"models/stickygib.mdl"` | dlls/gib.cpp:74 |
| props.cpp | CBaseProp::Precache | `"models/error/error.mdl"` | `"models/error.mdl"` | dlls/props.cpp:215 |

### Symbol referenced (a different entry of a data table)

| File | Function | Hidden | Ours | Likely source |
|---|---|---|---|---|
| player.cpp | PredMapInit\<CPlayerState\> | `$predDesc+0x38` (`&predDesc[1]`, one field) | `$predDesc` (empty table) | dlls/player.cpp:324, `BEGIN_PREDICTION_DATA_NO_BASE( CPlayerState )` is empty in ours; Hidden's table has one entry: `v_angle`, FIELD_VECTOR, flags 0x100 (FTYPEDESC_INSENDTABLE), so `DEFINE_PRED_FIELD( v_angle, FIELD_VECTOR, FTYPEDESC_INSENDTABLE )` |
| player.cpp | global constructors keyed to sv_noclipduringpause | same as above (the inlined PredMapInit) | | same line |
| soundscape.cpp | DataMapInit\<CTriggerSoundscape\> | the think function entry is `dataDesc[1]` (+0x3c) | `dataDesc[3]` (+0xac) | dlls/soundscape.cpp:462, field order: Hidden has the 2006 SDK order `DEFINE_THINKFUNC( PlayerUpdateThink )`, then `DEFINE_KEYFIELD( m_SoundscapeName ... )`, then `DEFINE_FIELD( m_hSoundscape ... )`; ours puts the think function last |

Hidden's `predDesc` for CPlayerState is 0x70 bytes in .data (two entries), ours 0x38 bytes in .bss
(the placeholder only), which confirms the missing field. `dump_datamaps.py` does not list think
functions, so `diff_layouts.py` could not see the CTriggerSoundscape order.

## 4. What the strict compare still does not see

- Data a function does not reach through an operand: function-local static tables filled at
  compile time, and the contents of writable globals (`.data`). Named read-only objects are hashed
  only when they hold no relocations, so tables of pointers (vtables, `const char *const` arrays)
  are compared by name only; vtables have `diff_vtables.py`, save tables `dump_datamaps.py`.
- Strings and data reached through a pointer table one level down (a table entry that points at a
  table) are named, not expanded.
- A pointer one past the end of a writable array that happens to equal the start of the next
  symbol is named after that next symbol. If the two builds lay out .bss differently that would
  read as a difference; it did not happen on main.
- Anything outside .text (static initialisers' data, .eh_frame, .gcc_except_table).

## 5. The same for the client (to do after the server-msvc merge)

`rebuild/tools/msvc_compare.py` (owned by the server-msvc lane until it merges) masks every value
inside the image range as `ADDR` and never reads `.rdata`. The client and the MSVC server need:

1. **Which operands are addresses.** client.dll and our MSVC build are not position independent
   but carry PE base relocations (`.reloc`, 0x3e568 bytes in client.dll). Each block is a 4 KB page
   RVA plus 16-bit entries (type in the top 4 bits, offset in the low 12); type 3
   (IMAGE_REL_BASED_HIGHLOW) marks a 32-bit absolute address. Parse it once per image into a set of
   field addresses (`objdump -p` does not list entries; read the PE directly, the data directory
   index 5 gives its RVA and size). An operand whose 4 bytes are covered is an address; every other
   number, inside the image range or not, is a constant and stays literal. Today only constants
   that happen to fall in the image range (0x24000000 to 0x24380000 for client.dll) are masked
   wrongly, so this step matters less than the next one; its main use is telling an address from a
   constant without guessing from the range.
2. **What it points at.** client.dll has no symbols, so the token comes from the mapping
   msvc_compare.py already builds: a mapped function gets our name; a vftable gets the class from
   pe_rtti.py; an import slot keeps `IMP:<name>`. For `.rdata` without a name, use the content as
   the server does: x87 operands by suffix (`flds`, `fldl`, `fildl`), strings up to the NUL, and
   the bytes up to the next relocated or referenced address otherwise. MSVC puts float constants in
   `.rdata` as `__real@<hex>` (427 in our client.map) and strings as `??_C@...` (6886), so our side
   can be checked against the map; Hidden's has only the bytes, which is enough. This is where the
   client's hidden differences are: MSVC loads nearly every float constant from `.rdata`
   (`flds 0x2423f6c8`, `__real@42340000` in our map), and those addresses read as `ADDR` today.
3. **Writable data** (`.data`, `.bss`): the reference has no names, so name it through the mapping
   (the same global used by a mapped pair at the same position) or leave it as `ADDR` for a first
   version; constants and `.rdata` content are where the known differences are.
4. **Jump tables** after the function are already compared as in-function offsets
   (`TABLE` rows); index tables (`BYTES`) already compare their bytes.
5. Keep a loose switch there too, so the current 23263 same / 360 changed stays reproducible.
