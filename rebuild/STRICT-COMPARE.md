# Strict server compare

`rebuild/compare.sh` (through `rebuild/tools/compare_builds.py`) now compares the constants and the
data behind every address, not only the instructions. This file says what it checks, what it found
on main, and (sections 5 to 7) the same for the MSVC client.dll and server.dll compare.

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

## 5. The same for the client (the plan; done in section 6)

`rebuild/tools/msvc_compare.py` (owned by the server-msvc lane until it merges) masked every value
inside the image range as `ADDR` and never read `.rdata`. The plan for the client and the MSVC
server was:

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

## 6. The MSVC compare (client.dll and server.dll)

`rebuild/tools/msvc_compare.py`, through `rebuild/msvc/compare_client.sh` and
`rebuild/msvc/compare_server.sh`, is strict by default since this lane; `--loose` (or `LOOSE=1`)
is the original scoring:

    sh rebuild/msvc/compare_client.sh <work dir>                    # strict
    sh rebuild/msvc/compare_client.sh --loose <work dir>            # the original scoring
    sh rebuild/msvc/compare_server.sh [--loose] <work dir> ~/.cache/subject51764/hsrebuild/builds/dll_2006-08-18_4b1.dll
    NAME=<our name part or a reference address>[,...] sh rebuild/msvc/compare_client.sh <work dir>

### What is compared

Which operands are addresses: the PE base relocations (`.reloc`, data directory 5, type 3
HIGHLOW), read once per image by `rebuild/tools/pe_refs.py`. A number whose four bytes a
relocation covers is an address; every other number is a constant and stays literal, inside the
image range or not. The mapping itself (sections of CLIENT.md 5 and MSVC.md "Comparing
server.dll") is computed exactly as before in both modes; the strict tokens are used for scoring
and for the refinement below.

| What the operand points at | Token | Example |
|---|---|---|
| a mapped function (call, jump, function pointer) | our name (the reference's side: the name of the function it is mapped to) | `push $<?PrintFn@CTextStatInt@@CAXPAVIFileSystem@@PAX1@Z>` |
| code that is not a mapped function start | `CODE` on both sides | `push $CODE` |
| a jump or index table inside the function's extent | its number, the table is compared as before | `jmp *TABLE#0(,%eax,4)`; a table past the extent's end (the extent ends at the last instruction) is code that is not a mapped start: `jmp *CODE(,%edi,4)`, `movzbl CODE(%edi),%edi` |
| an import slot | `IMP:<name>` | `call *IMP:?ExitScope@CVProfile@@QAEXXZ` |
| a vftable (RTTI, pe_rtti.py) | class and subobject offset | `movl $vftable:CAmmoDef@0x0,(%ebx)` |
| an RTTI type descriptor | its decorated name | `push $TD:.?AVC_BaseEntity@@` |
| writable data (.data, .bss) | our symbol plus offset through the data pairing, else `ADDR` | `mov data:?gpGlobals@@3PAVCGlobalVarsBase@@A,%eax` |
| .rdata, x87 operand | the value by suffix (`flds`/`fsts`/`fmuls` f32, `fldl` f64, `fildl` i32, `filds` i16, `fldt` f80) | `flds f32:0`, `fcomps f32:0` |
| .rdata, SSE operand | `movss`/`mulss` f32, `movsd`/`mulsd`/`movlpd` f64, packed x128 | `mulss f32:0.63661975,%xmm0` |
| .rdata, sized integer read | the value, or `*<token>` for a relocated pointer | `mov w32:0x65736e75,%ecx` (a string copied inline) |
| .rdata, C string | the text (UTF-16 as `L"..."`, a `__FILE__` path relative to the tree) | `push $"models/error/error.mdl"`, `push $L"EN"`, `push $"game_shared/animation.cpp"` |
| .rdata, a run of relocated words | each entry's token | `movl $[<?length@DNameStatusNode@@UBEHXZ>,...],(%eax)` (a vftable without RTTI) |
| other .rdata | the bytes up to the next referenced address, at most 64, without trailing zeros, pointers among them as tokens | `push $ro:{ffffffff,CODE,CODE}` (an SEH scope table) |

Notes:

- With /GF equal strings are one copy, and MSVC shares one `__real@<hex>` constant between all the
  functions of an image, so .rdata is compared by content, never by address.
- Our map names 7314 `__real@<hex>` and `??_C@` entries in client.dll and 11594 in server.dll;
  every run checks the content reader against them (the bits of `__real@`, narrow or UTF-16 and
  the encoded length of `??_C@`) and prints the count that disagree: 0 for both.
- `__FILE__` paths differ only by where the compiler ran: Hidden's client has
  `\hl2sdk\src\game_shared\animation.cpp`, the server `\HL2Mods\hidden\game_shared\...`, ours
  `..\game_shared\...`. All are cut to start at the tree's top directory (`game_shared/...`).
- Untyped data stops at the next address something points at, the section ends and the tables
  the PE header points at (imports, debug directory, load config): .rdata also holds data no
  operand points at (C++ EH tables, the import descriptors) with image offsets that depend on
  layout and are not relocated.
- `CODE`: without a PDB, the August server.dll's function extents sometimes merge two functions,
  and a pointer to the second one then points inside the first (`PrintFn+0x20`). A pointer to
  code that is not a mapped start compares equal to another such pointer, as in the loose mode;
  a pointer to a mapped function compares by name, and the two kinds never compare equal. Calls
  and jumps keep the loose labels (a call to an unmapped function is never "same").

### Writable data

The reference has no names for .data and .bss. In every mapped pair whose masked bodies line up
(the call graph's alignment), the relocated operands at the same position that point into
writable data on both sides vote for the pairing. Votes rank first by whether the pair's masked
bodies are equal, then by how sure the pair is: by name, vftable or a unique set of strings
first, then the call graph, a table or the strict lines, then position or a shared body; the `$E`
initialisers and destructors always count last, since every object numbers its own. An address
takes the candidate with the most votes of the best rank that has any, when no other candidate
has as many there. Our side is named by the map's nearest symbol plus offset (file statics are
not in the map, so a static reads as the public symbol before it plus an offset, for example
`?s_RagdollLRU@@3VCRagdollLRURetirement@@A+0x48`). An address no pair votes for, on either side,
stays `ADDR`, as in the loose mode: data only unaligned or unmapped code uses is not compared.
Client: 7951 writable addresses paired, server 28800.

### Refining the mapping

The loose normalisation cannot tell apart functions such as GetPanelClassName, GetRecvTable or
GetClientClass of different classes, factories, or the `$E` initialisers and destructors of one
object: their masked bodies are equal, so the mapping pairs them by position or by body, and the
strict tokens show where two were swapped (one returns `"CHudHealth"`, its partner
`"CHudGameMessage"`). In strict mode only, after the mapping, `refine()` does in rounds:

1. names the writable data without the votes of the suspect pairs (equal masked bodies, different
   strict lines) that are not sure (not paired by name, vftable or strings);
2. maps unmapped functions that tables of relocated words point at, entry by entry, where a pair
   with equal masked bodies points at a table on both sides (vftables of classes built without
   RTTI, as in libc's undname);
3. among the suspect pairs and the unmapped functions with the same masked body, pairs again those
   whose strict lines are equal: unique on both sides, or as many on both sides and all mapped
   already (in address order, which scores the same); one left on each side of a group pairs too.

Client: 283 pairs made (282 by strict lines, 1 by a table), 215 more functions mapped than the
loose mode. Server: 32 pairs, 29 more mapped. Rows paired this way say `strict` (or `table`) in
the method column. 20 client functions that were changed in the loose mode are same now: the
loose mapping had paired them the wrong way round (`$E` initialisers and destructors in
C_Func_Monitor, c_func_occluder, the weapons, c_effects, collisionproperty).

### Checks

| Run | same | changed | unmapped (ref) | unmapped (ours) |
|---|---|---|---|---|
| client, `--loose` | 23275 | 348 | 697 | 573 |
| client, strict | 23502 | 336 | 482 | 358 |
| server against 2006-08-18, `--loose` | 30007 | 151 | 543 | 919 |
| server against 2006-08-18, strict | 30026 | 161 | 514 | 890 |
| server, stable May-August subset, `--loose` | 14493 | 37 | 93 | |
| server, stable May-August subset, strict | 14490 | 40 | 93 | |
| server against 2006-08-08 4.1, `--loose` | 30004 | 152 | 542 | 921 |
| server against 2006-08-08 4.1, strict | 30023 | 162 | 513 | 892 |
| server against May 2006, `--loose` | 30000 | 759 | 20 | 216 |
| server against May 2006, strict | 29989 | 771 | 19 | 215 |
| Hidden's client.dll against itself, strict and `--loose` | 24320 | 0 | 0 | 0 |
| 2006-08-18 server.dll against itself, strict and `--loose` | 30701 | 0 | 0 | 0 |
| 2006-08-18 server.dll against the 4.1 DLL, `--loose` | 30680 | 5 | 9 | 13 |
| 2006-08-18 server.dll against the 4.1 DLL, strict | 30680 | 5 | 9 | 13 |

- `--loose` writes a `cmp_client.tsv` and `cmp_server_dll_2006-08-18_4b1.tsv` byte-identical to
  what the tool before this change writes for the same build. Against main's own tables
  (`~/.cache/hsrebuild-main-msvc/cmp_client.tsv`, `~/.cache/hsrebuild-main-msvc-server/...`)
  every status, size and address is the same; only the anonymous namespace hash (`?A0x...`, new
  per compile) and the alias the map lists first at a shared address (`__chkstk` or
  `__alloca_probe`) differ between two builds of the same source.
- The names compare_server.sh carries over from the May DLL now always come from a loose mapping
  (`--loose` on the May-to-August run), so both modes start from the same names
  (`names_dll_2006-08-18_4b1.tsv` is byte-identical to main's). It is rebuilt whenever
  msvc_compare.py is newer, which a checkout of this change is; there is no other cache.
- The self-compares pair every function with itself by name (a procs.tsv of every start,
  `--ref-procs` and `--ours-procs`) and test the tokens alone: all same. The August against 4.1
  run (4.1 as "ours", its starts and May names from a compare against our build) finds nothing
  strict-only; its 5 changed are real differences between the two releases.

Runtime on this machine: client loose about 15 seconds, strict about 45; server loose about 30
(of which 15 the May-to-August mapping), strict about 45 to 60.

### Limits left

- Writable data that no aligned pair uses stays `ADDR` on both sides, so a function that alone
  uses a global (and is unaligned elsewhere) is not checked for which global.
- Pointers to code that is not a mapped start are `CODE`, not compared.
- Data reached through a table entry that points at a table is named, not expanded (one level).
- Untyped .rdata is compared up to 64 bytes.
- Constants in .data (initialised writable globals: ConVar defaults are strings in .rdata, but a
  `static float x = 1.5f` is .data) are not read.
- The refinement only re-pairs functions whose masked bodies are equal; a wrong pair with
  different bodies stays.

## 7. Same in loose mode, changed in strict mode (MSVC, main at f12fee2)

"Hidden" is the value in Hidden's DLL, "ours" in our build. Nothing is fixed in this lane. The GCC
column says how the same function scores in the strict GCC server compare
(`~/.cache/hsrebuild-main/cmp_main_fix11_strict.tsv`), where the code is shared or the server's.
The server column is the compare against the 2006-08-18 DLL, the main target; the 4.1 DLL gives
the same 10 functions, and the May DLL is at the end of this section.

| Group | Client | Server |
|---|---|---|
| integer constant | 0 | 0 |
| float or double constant | 0 | 0 |
| string literal | 5 | 0 |
| symbol referenced | 0 | 0 |
| table | 0 | 0 |
| other: pairing the tool still gets wrong | 4 | 0 |
| other: pointer into an extent that holds two functions (`CODE`) | 0 | 3 |
| other: atexit destructor with no start of its own (`CODE`) | 0 | 4 |
| other: `$E` functions paired by a number that differs | 0 | 3 |

So the loose MSVC compare hid five real string differences in the client and none in the server.
The constant fixes server-msvc made against server.dll had already removed the float and integer
ones; none is left.

### Client: string literal (all client-only code, no GCC counterpart)

| Object | Function | Hidden | Ours | Likely source |
|---|---|---|---|---|
| vgui_controls:URLLabel.obj | vgui::URLLabel::GetDescription | `"%s, string URLText"` | `"%s, string URLText bool Underline"` | vgui2/controls/URLLabel.cpp:169; Hidden's URLLabel is older (no underline setting), as CLIENT.md section 5 already notes for the file |
| c_baseplayer.obj | C_BasePlayer::C_BasePlayer | `m_CurrentLocation = " "` | `""` | cl_dll/c_baseplayer.cpp:364 |
| detailobjectsystem.obj | CDetailObjectSystem::UnserializeModelDict | `"models/error/error.mdl"` | `"models/error.mdl"` | cl_dll/detailobjectsystem.cpp:746, the vertex-lit fallback model (the GCC server had the same fix in CBaseProp::Precache) |
| ClientScoreBoardDialog.obj | CClientScoreBoardDialog::StaticPlayerSortFunc | `GetInt("points")` twice | `GetInt("frags")` | cl_dll/game_controls/ClientScoreBoardDialog.cpp:373 and :374; the other scoreboard functions that set or add the column (:316, :351, :359, :403) are already changed in loose mode and likely use "points" too |
| SpectatorGUI.obj | ForwardSpecCmdToServer | `"%s \"%s\""` | `"%s %s"` | cl_dll/game_controls/SpectatorGUI.cpp:659, the parameter is quoted |

### Client: pairing the tool still gets wrong

| Object | Function (ours) | Hidden | Ours | Why |
|---|---|---|---|---|
| MyPanel.obj | CMyPanel::GetPanelBaseClassName | returns `&C_BaseEntity::m_DataMap` | returns `"vgui::Frame"` | paired by position with a GetDataDescMap-style accessor; its real partner is one of many equal ones |
| weapon_fn303.obj | ClientClassInit\<DT_CFN303Ball\> | constructs the RecvTable paired with our DT_WeaponP90 | an unpaired RecvTable | paired by position in the weapon objects, where Hidden's object order differs |
| c_func_lod.obj | $E5 and $E8 | $E5 uses lod_Enable | $E8 does | the two initialisers are swapped (paired by their strings); the lines differ in more than the swap, so the refinement leaves them |

### Server: `CODE` against a mapped function

`push $CODE` against one of our mapped functions, or the other way round: the function the
pointer goes to is not a mapped start in the reference. All seven are same in the strict GCC
compare (for a `$E`, the file's static initialiser), so none is a source difference.

Three point into an extent the code analysis made of two functions (the 2006-08-18 DLL has no
PDB; against the May DLL, which has one, these three are same):

| Object | Function | Hidden points at | Ours points at | GCC strict |
|---|---|---|---|---|
| textstatsmgr.obj | CTextStatInt::CTextStatInt | 0x20 into the function mapped to CTextStatInt::PrintFn | CTextStatInt::PrintFn | same |
| nav_area.obj | $E19 (passes a command callback) | 0x40 into the function mapped to CommandNavUpdateBlocked | CommandNavUpdateBlocked | same |
| soundenvelope.obj | $E13 (passes a command callback) | 0x20 into the function mapped to report_soundpatch | report_soundpatch | same |

Four involve the destructor MSVC registers with atexit for a static (`mov $obj,%ecx; jmp
<destructor>`, 11 bytes). For a function-local static, as in GetAmmoDef, Hidden's has no start:
the May PDB has no record for it either, so it is `CODE` against the May DLL too. Ours is a
mapped `$E`, paired by its number with a reference `$E` that is another function:

| Object | Function | Hidden points at | Ours points at | GCC strict |
|---|---|---|---|---|
| sdk_gamerules.obj | GetAmmoDef | the destructor of its static CAmmoDef (no start) | `_$E51` | same |
| sdk_gamerules.obj | ServerClassInit\<DT_SDKGameRulesProxy\> | the SendTable's destructor (no start) | `_$E46` | same |
| hdn_overrunrules.obj | ServerClassInit\<DT_OverRunGameRulesProxy\> | the SendTable's destructor (no start) | `_$E30` | same |
| sdk_gamerules.obj | $E47 | the reference function paired with our `_$E51` | an unmapped `$E` of ours | same |

### Server: `$E` functions paired by a number that differs

| Object | Function | Hidden | Ours | GCC strict |
|---|---|---|---|---|
| sdk_gamerules.obj | ServerClassInit\<DT_SDKGameRules\> | registers `_$E38` with atexit | `_$E35` | same |
| weapon_fn303.obj | ServerClassInit\<DT_WeaponFN303\> | `_$E29` | `_$E28` | same |
| sdk_gamerules.obj | $E25 | `"info_marine_spawn"` | `"info_hidden_spawn"` | same (the initialiser) |

MSVC numbers the `$E` initialisers and destructors of an object in its own order, and in these
two objects Hidden's numbers and ours differ (our `_$E35` in sdk_gamerules.obj destroys a SendProp
array; Hidden's calls ServerClassInit). The mapping pairs them by these names (the May PDB's, or
carried over to the August DLL), so a `$E` meets the one next to its real partner, and the
refinement cannot help where no strict partner is unique. In the strict GCC compare the static
initialisers of sdk_gamerules.cpp and weapon_fn303.cpp ("global constructors keyed to
mp_roundtime", "... CFN303Ball::m_DataMap") are same, so the globals are defined in Hidden's
order; what differs is MSVC's count or numbering of `$E` functions in those objects. MSVC-only,
not a source constant, and probably not the initialiser order SERVER-STATUS.md suggests.

### Server against the May DLL

Loose 30000 same / 759 changed, strict 29989 / 771 (mapped 30759 and 30760 of 30779; stable
subset 14585 / 38 loose, 14579 / 44 strict). The 12 functions same only in the loose mode:

| Object | Function | Hidden (May) | Ours | Cause |
|---|---|---|---|---|
| nav_generate.obj | CNavMesh::GetPlayerSpawnName | `"info_player_start"` | `"info_marine_spawn"` | May source, older than ours |
| nav_generate.obj | CNavMesh::BeginGeneration | `"info_player_start"` | `"info_marine_spawn"` | the same, inlined |
| sdk_gamerules.obj | CSDKGameRules::GetGameDescription | `"Hidden : Source B3"` | `"Hidden : Source B4"` | May source |
| hdn_spectator.obj | ServerClassInit\<DT_HDNSpectator\> | `"m_pLocation"` | `"m_Location"` | May source |
| sdk_gamerules.obj | $E17, $E20 | ConVars hdn_deathwait, hdn_survival | hdn_jointime, hdn_limitbombs | `$E` number, May ConVars differ |
| sdk_gamerules.obj | $E25, $E47, ServerClassInit\<DT_SDKGameRules\>, GetAmmoDef, ServerClassInit\<DT_SDKGameRulesProxy\> | | | as against August above |
| weapon_fn303.obj | ServerClassInit\<DT_WeaponFN303\> | `_$E29` | `_$E28` | as against August above |

The four string rows are differences between Hidden's May and August source, where ours follows
August; none is a fix for the August target.
