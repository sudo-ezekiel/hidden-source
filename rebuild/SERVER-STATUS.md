# Server (server_i486.so) status

## Summary

The rebuilt Linux server is compared function by function with Hidden's shipped, unstripped
`server_i486.so` (build of 2006-08-18). Of 18413 functions:

| same | changed | only in Hidden's build | only in ours |
|---|---|---|---|
| 18366 | 47 | 0 | 0 |

"Same" means the function compiles to exactly the same instructions as Hidden's, once addresses are
normalised (call and jump targets become symbol names or offsets inside the function, absolute
addresses become a placeholder). The build uses GCC 3.4 for i386 with Hidden's flags
(`-mtune=i686 -march=pentium -mmmx -O3`). Nothing is missing: every function in Hidden's binary
exists in the rebuild, and the rebuild has no extra functions.

These counts are from the original (now `--loose`) comparison, which hid every number above 0xFFFF
and the data behind addresses. The strict comparison, now the default of `compare.sh`, also checks
large constants, float constants, strings and the symbol behind every address. On main after the
server-msvc and strict-compare merges it gives 18355 same and 58 changed: 11 functions differ only in a
constant (a ConVar default, a float, a model name, a prediction table entry); `STRICT-COMPARE.md`
lists them with Hidden's value, ours and the source line.

None of the 47 "changed" functions is known to behave differently from Hidden's. Each one has the
same calls, the same constants and the same control flow as far as we could check. What differs is
how GCC compiled it: which register it picked, the order of independent instructions, or the order
it laid out blocks. GCC 3.4 makes some of these choices from state that carries over from one
function to the next inside a source file (for example, which free register the peephole pass
tries first), so a function can come out different even when its own source is right. The
similarity column is the share of matching instructions (Python difflib ratio); one register
renamed throughout a short function can pull it down to 0.6.

## How to rebuild and compare

Needs Docker and the `hsrebuild-gcc34` image (see `rebuild/docker/README.md`). From the repo root:

    JOBS=6 sh rebuild/build.sh ~/.cache/hsrebuild-work
    sh rebuild/compare.sh <path to Hidden's server_i486.so> ~/.cache/hsrebuild-work

`build.sh` mirrors the tree into the work dir, makes a makefile from `dlls/hl_sdk.vcproj`
("Release SDK|Win32") and builds `out/server_i486.so`. A full build takes 1 to 2 minutes; a header
change rebuilds everything. `compare.sh` writes `cmp.tsv` in the work dir, one row per function:
source file, status, similarity, sizes, mangled and readable name. It compares strictly (see
`STRICT-COMPARE.md`); `sh rebuild/compare.sh --loose ...` gives the original comparison, for
comparing with older tables. Totals:

    awk -F'\t' '{n[$2]++} END {for (s in n) print s, n[s]}' ~/.cache/hsrebuild-work/cmp.tsv

To see how one function differs:

    python3 -I rebuild/tools/fn_diff.py <Hidden's .so> ~/.cache/hsrebuild-work/out/server_i486.so "CBasePlayer::Spawn"

## The 47 remaining functions

"Masked" below means the comparison with registers and stack slot offsets replaced by placeholders.
"Tried" lists the source forms that were built and measured without making the function same.

### Registers or stack slots only (identical once masked)

| File | Function | Similarity | Notes |
|---|---|---|---|
| baseentity.cpp | CBaseEntity::IsInWorld | 0.989 | The edict pointer is in esi in Hidden's build, ecx in ours. The source matches both Valve SDKs. |
| gamemovement.cpp | CGameMovement::CheckPounceButton | 0.980 | One edx/ecx swap. |
| hdn_bot.cpp | CHDNBot::~CHDNBot (two of the three) | 0.632, 0.947 | Which scratch register loads the vtable constants and which one the epilogue `pop` uses. |
| hdn_bot.cpp | CUtlLinkedList<CNavArea*,int>::Purge | 0.969 | Only the throw-away register of the epilogue `pop` (eax against ecx). The same template is same elsewhere. |
| hdn_evacuationrules.cpp | CEvacuationGameRulesProxy::~CEvacuationGameRulesProxy | 0.962 | Epilogue scratch register. |
| physics_prop_ragdoll.cpp | CRagdollProp::RecheckCollisionFilter | 0.929 | Register choice. |
| player.cpp | CBasePlayer::SetupVisibility | 0.627 | Register choice. |
| player.cpp | CBasePlayer::UpdateTransmitState, SetStamina, CSprayCan::~CSprayCan | 0.833, 0.962, 0.962 | Epilogue scratch register, or the register used to pass a zero. |
| player.cpp | CBasePlayer::PlayerDeathThink, SetCurrentLocation | 0.901, 0.949 | Register choice. |

### Same instructions in a different order (instruction scheduling, often with register changes)

| File | Function | Similarity | Notes |
|---|---|---|---|
| baseentity.cpp | CBaseEntity::GetVectors | 0.922 | Became different when CBaseEntity::Reset was made same: any named local in Reset changes GetVectors, and without one Reset differs. Reset was kept, since it is Hidden's own code. |
| effects.cpp | CEnvSplash::InputSplash | 0.997 | Two zero stores from the inlined CEffectData constructor come out in a different order. |
| grenade_tripmine.cpp | CTripmineGrenade::OnTakeDamage_Alive | 0.996 | The same CEffectData store is scheduled earlier. Reordering the constructor stores (game_shared/effect_dispatch_data.h) changed 25 other functions from same to changed, so it was reverted. |
| hdn_evacuationrules.cpp, hdn_overrunrules.cpp | RespawnHiddens | 0.852 each | Register choice and scheduling. |
| hdn_hiddenselection.cpp | CHiddenSelector::SelectHidden | 0.915 | Register choice and scheduling. |
| physics_prop_ragdoll.cpp | CRagdollProp::SetDebrisThink | 0.781 | Register choice and scheduling. |
| player.cpp | CBasePlayer::CBasePlayer (both) | 0.994 | The loads of the DevMsg arguments are in a different order. |
| sdk_client.cpp | InstallGameRules | 0.896 | Register choice and scheduling. |
| sdk_gamerules.cpp | CSDKGameRules::IRISWins, ClientCommand | 0.812, 0.960 | Register choice and scheduling. |

### Small differences in register allocation, spills or block layout

| File | Function | Similarity | Notes |
|---|---|---|---|
| gamemovement.cpp | CGameMovement::FullWalkMove | 0.757 | Register allocation and code alignment. |
| gamemovement.cpp | CGameMovement::AirMove | 0.840 | Stack layout of the locals; one instruction differs once masked. |
| hdn_bot.cpp | FindMinimumCostArea<ShortestPathCost> (dlls/nav_pathfind.h) | 0.832 | The block layout now matches (the degenerate case is tested first, so the "random cheap area" return is last). Left: the -1 and count-1 arguments sit in two registers in Hidden's build and one in ours, so GCC shares one more instruction between the two returns. Tried: the 2006 form, `if (count == 0) {...} return ...`, and dropping the numAreas local. |
| hltvdirector.cpp | CHLTVDirector::CreateShotFromEvent | 0.616 | The event pointer is spilled to the stack differently. Three source forms tried. |
| nav_edit.cpp | CNavMesh::CommandNavWarpToMark | 0.644 | Block layout only: GCC moves the EmitSound call out of line and Hidden's falls through to it. GCC predicts a guarded call as unlikely, and no source form changed that. Tried: early returns, nested ifs, one `&&` condition, a bool local, `goto`, `do {} while (0)`, a dead `if (area)` branch, `switch`. Hidden's version reads no marked area, so it is a cut-down form of Valve's 2006 command. |
| nav_edit.cpp | CNavMesh::GetActiveNavArea | 0.780 | Register allocation and scheduling (925 against 928 instructions). No structural difference. |
| nav_edit.cpp | MakeSniperSpots | 0.857 | x87 register stack: ours keeps a second copy of the 25.0 step constant. Seven forms of round() and of the body tried. |
| physics.cpp | CPhysicsHook::LevelInitPreEntity | 0.791 | Same calls in the same order; registers and scheduling differ, and ours is one instruction shorter. |
| physics_prop_ragdoll.cpp | CreateServerCorpseRagdoll | 0.938 | Register allocation (three extra spill instructions in ours). |
| player.cpp | CBasePlayer::OnTakeDamage_Alive | 0.624 | Calls, jumps and compares all match; ours has 7 extra spill reloads. |
| player.cpp | CBasePlayer::SetFOV | 0.815 | Scheduling; two ternary rewrites were worse. |
| player.cpp | CBasePlayer::Spawn | 0.886 | Register allocation (one extra spill). |
| sdk_player.cpp | CSDKPlayer::Spawn | 0.875 | Register allocation. |
| util_shared.cpp | PassServerEntityFilter | 0.954 | The final marine-clip test: Hidden computes `!(a & b)` with `test` and `sete`, ours rewrites it as `!a | !b` with `or`. Nine forms tried. `return ((a) & (b)) == 0;` and two bool locals both give Hidden's `sete`/`setne`/`test`/`sete` shape but widen the operands to int (one extra `xor`, a 32-bit `test`); `bool b = a; b &= c; return !b;` is further off. |
| weapon_fn2000.cpp, weapon_p90.cpp | PrimaryAttack | 0.868, 0.949 | Register allocation and tail sharing in the recoil `? :` with SHARED_RANDOMINT; the FN2000 also swaps two spill slots. |
| weapon_fn303.cpp | CFN303Ball::BoltTouch | 0.921 | Register choice (ebx/esi/edi) in the stun and weighting block, and tail sharing of a DevMsg. |
| weapon_sdkbase.cpp | CWeaponSDKBase::CreateLaserPointer | 0.809 | Scheduling, and a tail that Hidden's build duplicates. |

### Larger layout differences, still with the same calls and compares

| File | Function | Similarity | Notes |
|---|---|---|---|
| player_command.cpp | CPlayerMove::RunCommand | 0.270 | Calls, compares and their order match; the rarely used blocks are laid out in a different order, which moves almost every jump. |
| player_lagcompensation.cpp | CLagCompensationManager::BacktrackPlayer | 0.792 | Hidden's build has 16 bytes more stack, from extra temporaries. Of the 47 this is the one most likely to hide a real source difference that has not been found. |
| sdk_gamerules.cpp | CSDKGameRules::RestartRound | 0.729 | The inlined CountHiddens and TotalPlayers loops have a different loop shape (rotated against jump-to-condition). Each of those functions is same on its own. |
| sdk_gamerules.cpp | CSDKGameRules::IsRoundOver | 0.825 | Hidden's build has a duplicated loop-entry block that we could not reproduce; a different loop form in the source is possible. |

## Function order and the May PDB's locals

Function order inside a source file counts. When the definitions of Hidden's own .cpp files were put
in the order of their first line in the May PDB, CWeaponPistol::ItemPostFrame and
CWeaponSDKBase::Holster became same (the 49 became 47), and moving CSDKGameRules::GoToIntermission
to its May place (after ClientConnected) turned DeathNotice and MarineDisconnected from same to
changed, so it stays before DeathNotice. Functions that pass `__LINE__` to SharedRandomInt (the
PrimaryAttack of the FN2000 and P90) keep their line numbers, since the line is an argument in the
code: moving the P90's PrimaryAttack changed its copy in client.dll.

The May PDB (rebuild/PDB.md) records the named locals that kept a stack slot or register in May's
build. For each of the 49 functions its locals were compared with ours, and where they differ the
PDB's form was built and compared. The PDB does not record block scopes in this build (every local
is at depth 0) and its record order is not the declaration order, so only the set of locals is
evidence.

| Function | What the PDB has | Tried | Result |
|---|---|---|---|
| CLagCompensationManager::BacktrackPlayer | A float `dist` (sharing frac's slot) that ours lacks; the rest as ours | `float dist = orgdiff.Length();` before the sv_unlag_maxdistance test, used in the test and in the DevMsg | 0.792 to 0.441, reverted |
| | | `dist` inside `if ( sv_unlag_maxdistance.GetFloat() > 0 )`, with the distance test nested | 0.801, still 0x13c of stack against Hidden's 0x14c and 758 instructions against 785; reverted (only a same function is kept) |
| CSDKGameRules::IsRoundOver, IRISWins | Not in the May PDB (added after May) | Nothing to try | |
| CBasePlayer::OnTakeDamage_Alive | An int `damage` where ours reads info.GetDamage() into a float for the event; the rest of May's body differs (no weighting) | `int damage = info.GetDamage();` before UTIL_LogPrintf, passed for the `(int)` argument | 0.624, unchanged; reverted |
| CGameMovement::CheckPounceButton | Only vecForward; no float for the pounce speed | `vecForward *= 615.0f;` without flPounceSpeed | 0.980 to 0.900, reverted |
| CBasePlayer::SetStamina | No locals; ours has flStamina | The test written on `m_flStamina + newStamina` | 0.962, unchanged; reverted |
| CGameMovement::AirMove | vStart, vForward, vEnd and tr beside the Valve locals | vStart, vForward and vEnd declared at the top with the others | 0.840 to 0.725, reverted |
| CTripmineGrenade::OnTakeDamage_Alive | vecGibVelocity, vecGibAVelocity and data, as ours | The gib vectors declared before the CEffectData | 0.996 to 0.688, reverted |
| CSDKPlayer::Spawn | `m_ParticleTrail` for the trail info | Renamed (ours was `info`); a name does not change GCC's code | 0.875, unchanged |
| CBasePlayer::PlayerDeathThink, SetupVisibility, Spawn; CEnvSplash::InputSplash; CFN303Ball::BoltTouch; CHLTVDirector::CreateShotFromEvent; CNavMesh::GetActiveNavArea; CPhysicsHook::LevelInitPreEntity; CPlayerMove::RunCommand; CreateServerCorpseRagdoll; CWeaponFN2000 and CWeaponP90 PrimaryAttack; MakeSniperSpots | The same named locals as ours | Nothing to try | |
| CBaseEntity::GetVectors, IsInWorld; CBasePlayer::CBasePlayer, SetCurrentLocation, SetFOV, UpdateTransmitState; CGameMovement::FullWalkMove; CNavMesh::CommandNavWarpToMark; CRagdollProp::RecheckCollisionFilter, SetDebrisThink; CSDKGameRules::ClientCommand, RestartRound; CWeaponSDKBase::CreateLaserPointer; InstallGameRules; PassServerEntityFilter | Parameters only, no named locals | Nothing to try | |
| CHiddenSelector::SelectHidden | May's takes (int, int, bool) and has a different body | Nothing to try | |
| CHDNBot::~CHDNBot, CUtlLinkedList<CNavArea*,int>::Purge, FindMinimumCostArea<ShortestPathCost>, CEvacuationGameRulesProxy::~CEvacuationGameRulesProxy, the RespawnHiddens of both rules, CSprayCan::~CSprayCan | Not in the May PDB (post-May code, or not emitted there) | Nothing to try | |
| CWeaponPistol::ItemPostFrame, CWeaponSDKBase::Holster | | Same after the function reordering | |

## MSVC build (server.dll)

The same source also builds Hidden's Windows server.dll, with Hidden's compiler (cl 13.10.3077
under Wine) and the vcproj's "Release SDK|Win32" settings: 441 of 441 files compile and the link
has no unresolved externals. The DLL is 4,366,336 bytes, the size of Hidden's two August DLLs.
How to build and compare, the settings and how they were checked are in `msvc/MSVC.md` ("Building
server.dll", "Comparing server.dll"):

    JOBS=6 sh rebuild/msvc/build_server.sh ~/.cache/hsrebuild-msvc-server
    sh rebuild/msvc/compare_server.sh ~/.cache/hsrebuild-msvc-server
    sh rebuild/msvc/compare_server.sh ~/.cache/hsrebuild-msvc-server ~/.cache/subject51764/hsrebuild/builds/dll_2006-08-18_4b1.dll

The compare is strict by default, as the GCC one is (STRICT-COMPARE.md section 6); `--loose`
gives the original scoring. Main f12fee2:

| Reference | Mode | Mapped | Same | Changed | Stable subset: same / changed / unmapped |
|---|---|---|---|---|---|
| May 2006, by its PDB | strict | 30760 of 30779 | 29989 | 771 | 14579 / 44 / 0 |
| | --loose | 30759 of 30779 | 30000 | 759 | 14585 / 38 / 0 |
| 2006-08-18 4b1 (the build of the .so above) | strict | 30187 of 30701 | 30026 | 161 | 14490 / 40 / 93 |
| | --loose | 30158 of 30701 | 30007 | 151 | 14493 / 37 / 93 |
| 2006-08-08 4.1 | strict | 30185 of 30698 | 30023 | 162 | 14490 / 40 / 93 |
| | --loose | 30156 of 30698 | 30004 | 152 | 14493 / 37 / 93 |

Against the 2006-08-18 DLL, 10 functions are same only in the loose mode (STRICT-COMPARE.md
section 7): pointers into extents that hold two functions, atexit destructors of statics that
have no start of their own in Hidden's DLL, and `$E` functions paired by a number that differs.
All 10 are same in the strict GCC compare; none is a source constant. The strict mode maps 29
more functions, all same. The causes below are from the loose compare.

The May DLL is the only one with names and extents from Hidden's own PDB, but it was built from
older source (CBasePlayer is 12 bytes shorter, and Hidden's game code differs in many places), so
most of its 759 are real source differences between May and August. The 2006-08-18 DLL is the
main target.

What the MSVC build found that the GCC comparison cannot see: `compare_builds.py` treats every
immediate above 0xffff as an address, so a wrong large constant, a float constant, or a float
where Hidden had a double compiles to a "same" function in the .so. These were fixed against
server.dll and checked in the 4b1 .so: FCAP_DONT_SAVE in CHDNAmmoCrate::ObjectCaps, the
RandomFloat( 8, 10 ) splash scales in gamemovement.cpp and baseplayer_shared.cpp, the eye height
62 of g_DefaultViewVectors, MASK_SHOT in CSDKPlayer::FireBullet, float and double literals in
the weapons' WeaponIdle, the FN2000's Deploy and PrimaryAttack, Swing, DelayDeathThink,
CheckPounceButton and GetBombCount, and the order of CTriggerSoundscape's data description. The
GCC result above is unchanged by all of them.

The 151 functions that differ from the 2006-08-18 DLL, by cause:

- **LIBC (38).** Hidden's libc.lib differs from the Toolkit's in these objects (pow_pentium4, the
  printf and scanf cores, SEH helpers, some string functions), although both Rich headers count
  107 C and 35 MASM objects of build 3077. Which libc.lib Hidden had is not known. No source fix.
- **Hidden's game code (about 60).** sdk_gamerules.obj (ClientCommand, PlayerKilled, IsRoundOver,
  GameOverThink, the Count and IRISWins tests), player.obj (ChangeClass, PhysicsSimulate,
  OnTakeDamage_Alive, PlayerDeathThink, UpdateStun), sdk_player.obj (Spawn, CreateRagdollEntity,
  PlayerUse), the evacuation and overrun rules, hdn_hiddenselection, hdn_extraction_trigger,
  hdn_bot, weapon_fn303. Several are also among the 47 above (ClientCommand, IsRoundOver,
  IRISWins, RespawnHiddens, SelectHidden, OnTakeDamage_Alive, PlayerDeathThink,
  SetCurrentLocation, CSDKPlayer::Spawn, BoltTouch). The others are "same" in the .so, so either
  their difference is one the GCC comparison masks, or GCC 3.4 gives the same code for two source
  forms that MSVC tells apart. They need a closer look one by one.
- **Static initialiser order (13).** The `_$E` initialisers of sdk_gamerules.obj and
  weapon_fn303.obj pair in sequence, and the sequences differ: Hidden's globals (ConVars and the
  like) are defined in another order in those two files. The strict GCC compare doubts this: the
  static initialisers of sdk_gamerules.cpp and weapon_fn303.cpp are same there, so the order of
  the definitions matches and MSVC's count or numbering of `$E` functions differs instead
  (STRICT-COMPARE.md section 7).
- **Register choice and scheduling (about 15).** Same instructions, other registers or order:
  CheckBack, CHLTVDirector::LevelInitPostEntity, ParseEntryExitAnims, SampleStep,
  LoadDecalsFromScript, LoadSchedulesFromBuffer, the CBasePlayer and CSDKGameRules constructors,
  InstallGameRules, CWeaponShotgun::PrimaryAttack.
- **Inlining and stack layout.** CBaseEntity::FireBullets inlines CGameTrace::operator= where
  Hidden's calls it.
- **The reference has no PDB (about 25).** Some extents found from the code are cut at a tail call
  or run into the next function (ServiceEventQueue, SendProxy_SolidFlags), and the static
  NetworkStateChanged functions of CBasePlayer are small, alike and named from the May DLL, so a
  few pair with a neighbour. Of the 37 stable-subset functions that differ, 29 are the same
  against the May DLL, where the PDB gives the extents.

543 reference functions are not mapped: small ones that no vftable, name, call or neighbour ties
to one of ours.
