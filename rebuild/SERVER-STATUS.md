# Server (server_i486.so) status

## Summary

The rebuilt Linux server is compared function by function with Hidden's shipped, unstripped
`server_i486.so` (build of 2006-08-18). Of 18413 functions:

| same | changed | only in Hidden's build | only in ours |
|---|---|---|---|
| 18377 | 36 | 0 | 0 |

"Same" means the function compiles to exactly the same instructions as Hidden's, once addresses are
normalised (call and jump targets become symbol names or offsets inside the function, absolute
addresses become a placeholder). The build uses GCC 3.4 for i386 with Hidden's flags
(`-mtune=i686 -march=pentium -mmmx -O3`). Nothing is missing: every function in Hidden's binary
exists in the rebuild, and the rebuild has no extra functions.

These counts are from the strict comparison, the default of `compare.sh`, which also checks large
constants, float constants, strings and the symbol behind every address (`STRICT-COMPARE.md`). Main
had 18366 same and 47 changed when the server-finish2 lane started; the lane read statement forms
from Hidden's server.dll and the May PDB's line table and made 11 more same (see the MSVC section).

None of the 36 "changed" functions is known to behave differently from Hidden's. Each one has the
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

## The 36 remaining functions

"Masked" below means the comparison with registers and stack slot offsets replaced by placeholders.
"Tried" lists the source forms that were built and measured without making the function same.

### Registers or stack slots only (identical once masked)

| File | Function | Similarity | Notes |
|---|---|---|---|
| baseentity.cpp | CBaseEntity::IsInWorld | 0.989 | The edict pointer is in esi in Hidden's build, ecx in ours. The source matches both Valve SDKs. |
| gamemovement.cpp | CGameMovement::CheckPounceButton | 0.980 | One edx/ecx swap. Tried: declaring flPounceSpeed after vecForward (worse). |
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
| hdn_hiddenselection.cpp | CHiddenSelector::SelectHidden | 0.915 | Register choice and scheduling. Same in server.dll since method 2 is tested before 1 and the selection sits in one `if ( !m_pCurrentHidden )` block; GCC's code did not change with that. |
| physics_prop_ragdoll.cpp | CRagdollProp::SetDebrisThink | 0.781 | Register choice and scheduling. |
| player.cpp | CBasePlayer::CBasePlayer (both) | 0.994 | The loads of the DevMsg arguments are in a different order. |
| sdk_gamerules.cpp | CSDKGameRules::ClientCommand | 0.954 | Register choice and scheduling. Same in server.dll since each command branch ends in its own SetReady and return. |

### Small differences in register allocation, spills or block layout

| File | Function | Similarity | Notes |
|---|---|---|---|
| hdn_bot.cpp | FindMinimumCostArea<ShortestPathCost> (dlls/nav_pathfind.h) | 0.761 | Back to Valve's order (the "random cheap area" return first), which is what Hidden's server.dll has: same there. GCC differs in register allocation with either order; the reversed order (degenerate case first) gave 0.832 in GCC and a changed server.dll function. Tried before: the 2006 form, `if (count == 0) {...} return ...`, and dropping the numAreas local. |
| hltvdirector.cpp | CHLTVDirector::CreateShotFromEvent | 0.616 | The event pointer is spilled to the stack differently. Three source forms tried. |
| nav_edit.cpp | CNavMesh::CommandNavWarpToMark | 0.644 | Also changed in server.dll (0.889, block layout and one register). Block layout only: GCC moves the EmitSound call out of line and Hidden's falls through to it. GCC predicts a guarded call as unlikely, and no source form changed that. Tried: early returns, nested ifs, one `&&` condition, a bool local, `goto`, `do {} while (0)`, a dead `if (area)` branch, `switch`. Hidden's version reads no marked area, so it is a cut-down form of Valve's 2006 command. |
| physics_prop_ragdoll.cpp | CreateServerCorpseRagdoll | 0.938 | Register allocation (three extra spill instructions in ours). |
| player.cpp | CBasePlayer::SetFOV | 0.815 | Scheduling; two ternary rewrites were worse. |
| player.cpp | CBasePlayer::Spawn | 0.886 | Register allocation (one extra spill). |
| sdk_player.cpp | CSDKPlayer::Spawn | 0.928 | Register allocation. Same in server.dll since the support class test reads `GetEquipment() != 3` with GiveNamedItem first. |
| util_shared.cpp | PassServerEntityFilter | 0.793 | The marine-clip test is now an `&&` with an early return, which Hidden's server.dll has (same there and in the MSVC client). Hidden's .so computes the test without a branch (`!(a & b)` with `test` and `sete`) and no form tried gives both: with `!(a & b)` GCC folds it to `!a | !b`, `return ((a) & (b)) == 0;` and two bool locals widen the operands to int, and every branchless form is changed in server.dll. Eleven forms tried. |
| weapon_fn2000.cpp, weapon_p90.cpp | PrimaryAttack | 0.978, 0.977 | Register allocation and tail sharing in the recoil `? :` with SHARED_RANDOMINT; the FN2000 also swaps two spill slots. |
| weapon_sdkbase.cpp | CWeaponSDKBase::CreateLaserPointer | 0.809 | Scheduling, and a tail that Hidden's build duplicates. Same in server.dll. Tried: the early-return shape of the HL2 RPG's CreateLaserPointer (worse in GCC). |

### Larger layout differences, still with the same calls and compares

| File | Function | Similarity | Notes |
|---|---|---|---|
| player_command.cpp | CPlayerMove::RunCommand | 0.265 | Calls, compares and their order match; the rarely used blocks are laid out in a different order, which moves almost every jump. Also changed in server.dll (0.706): there ours keeps `&pl.v_angle` in ebp and Hidden's keeps `this`, and May's code is Hidden's August code. |
| player_lagcompensation.cpp | CLagCompensationManager::BacktrackPlayer | 0.792 | Hidden's build has 16 bytes more stack, from extra temporaries. Also changed in server.dll (0.917). Of the 36 this is the one most likely to hide a real source difference that has not been found. |

## Function order and the May PDB's locals

Function order inside a source file counts. When the definitions of Hidden's own .cpp files were put
in the order of their first line in the May PDB, CWeaponPistol::ItemPostFrame and
CWeaponSDKBase::Holster became same (the 49 became 47), and moving CSDKGameRules::GoToIntermission
to its May place (after ClientConnected) turned DeathNotice and MarineDisconnected from same to
changed, so it stays before DeathNotice. Functions that pass `__LINE__` to SharedRandomInt (the
PrimaryAttack of the FN2000 and P90) keep their line numbers, since the line is an argument in the
code: moving the P90's PrimaryAttack changed its copy in client.dll.

The May PDB (rebuild/PDB.md) records the named locals that kept a stack slot or register in May's
build. For each of the 49 functions left at the time its locals were compared with ours, and where they differ the
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

On the server-finish2 branch, after the statement forms described below:

| Reference | Mode | Same | Changed |
|---|---|---|---|
| May 2006 | strict | 29995 | 767 |
| 2006-08-18 4b1 | strict | 30079 | 114 |
| | --loose | 30060 | 104 |

Against the 2006-08-18 DLL, 10 functions are same only in the loose mode (STRICT-COMPARE.md
section 7): pointers into extents that hold two functions, atexit destructors of statics that
have no start of their own in Hidden's DLL, and `$E` functions paired by a number that differs.
All 10 are same in the strict GCC compare; none is a source constant. The causes below are from
the loose compare.

The May DLL is the only one with names and extents from Hidden's own PDB, but it was built from
older source (CBasePlayer is 12 bytes shorter, and Hidden's game code differs in many places), so
most of its 767 are real source differences between May and August. The 2006-08-18 DLL is the
main target.

What the MSVC build found that the GCC comparison cannot see: `compare_builds.py` treats every
immediate above 0xffff as an address, so a wrong large constant, a float constant, or a float
where Hidden had a double compiles to a "same" function in the .so. These were fixed against
server.dll and checked in the 4b1 .so: FCAP_DONT_SAVE in CHDNAmmoCrate::ObjectCaps, the
RandomFloat( 8, 10 ) splash scales in gamemovement.cpp and baseplayer_shared.cpp, the eye height
62 of g_DefaultViewVectors, MASK_SHOT in CSDKPlayer::FireBullet, float and double literals in
the weapons' WeaponIdle, the FN2000's Deploy and PrimaryAttack, Swing, DelayDeathThink,
CheckPounceButton and GetBombCount, and the order of CTriggerSoundscape's data description.

### Statement forms read from server.dll

MSVC 7.1 keeps much more of the statement shape than GCC 3.4 does: the direction of a compare
(`a > b` against `b < a`), which operand is named first, an early return against an if/else, a
one-case `switch` (`sub` and `je`) against an `if` (`cmp`), `!handle` (CHandle's operator!)
against `handle == NULL`, a getter against a member read, and a value stored into a local before
a virtual call (which stops the vtable load from being hoisted). The May PDB's line table shows
the statement order where May's code is still August's. Reading those forms from Hidden's
server.dll and checking each one against the .so took server.dll from 30007 same / 151 changed to
30060 / 104 (loose) and the .so from 47 changed to 36, with no regressions in either:

- gamerules: IsRoundOver reads GetRoundTimerRemain, IRISWins tests `< 1` and sets the win flag
  before the print, the Count functions return from inside the loop, GameOverThink and
  IntermissionThink compare `<= curtime`, ClientCommand ends each branch in its own SetReady and
  return, the random picks are stored in a local, the character ranges test `<= 8`, the taunt
  cases run 7, 6, 5, and PlayerKilled forfeits the Hidden only for a Hidden's suicide.
- CBasePlayer: AddWeighting is out of line (Hidden's server.dll calls it from PlayerKilled,
  OnTakeDamage_Alive, PlayerUse, BoltTouch and the overrun rules), GetSpawnTimer and GetSpawnBool
  come from May, and EntSelectSpawnPoint, SetSpawnTimer, SetCurrentLocation, ChangeClass,
  InitialSpawn, PlayerDeathThink and the constructor follow the line table.
- nav_edit.cpp: round() returns the number of units and every caller multiplies by
  GenerationStepSize (both of Hidden's DLLs have it): round, MakeSniperSpots and GetActiveNavArea
  are same in both compilers.
- gamemovement.cpp: FullWalkMove pounces only for the Hidden team; Accelerate computes the speed
  as `sqrt( DotProduct( v, v ) )`, an inline fsqrt where VectorLength calls FastSqrt; AirMove has
  an unused vRight beside vForward, which is the 16 bytes more stack of Hidden's GCC frame.
- CPhysicsHook::LevelInitPreEntity sets a fixed 0.015 step; CLagCompensationManager has a
  ResetHistory (invented name) that FrameUpdatePostEntityThink tail-calls.
- The Hidden code: InstallGameRules, ClientPrintPercentage, SelectHidden, the extraction
  trigger's touch functions, the FN303's Reload and BoltTouch, the bludgeon weapon's
  ItemPostFrame and Hit, CWeaponSDKBase::ItemPostFrame, CSDKPlayer::Spawn, CreateRagdollEntity
  and PlayerUse, GenericBotPutInServer, the evacuation rules' SurvivalThink and RespawnHiddens,
  the overrun rules' SurvivalThink, RespawnHiddens, PlayerKilled and IRISWins,
  PassServerEntityFilter and FindMinimumCostArea (Valve's order).
- CHLTVDirector::LevelInitPostEntity clears the PVS origin before the PVS entity.

### The 104 left against the 2006-08-18 DLL (loose), by cause

- **LIBC (38).** Hidden's libc.lib differs from the Toolkit's in these objects (pow_pentium4, the
  printf and scanf cores, SEH helpers, some string functions), although both Rich headers count
  107 C and 35 MASM objects of build 3077. Which libc.lib Hidden had is not known. No source fix.
- **Static initialisers (14 `$E` rows).** The `$E` functions of sdk_gamerules.obj,
  weapon_fn303.obj and hdn_overrunrules.obj are paired by the
  `$E` number of the May PDB. Their bodies, compared in sequence with registers and addresses
  masked, are identical to ours; the numbers differ because MSVC numbers `$E` functions per
  object and Hidden's August objects have a different count before them. The static
  initialisers of these files are same in the GCC compare. This is a pairing artefact; no
  definitions were reordered. The same numbering shows in strict mode as a different `$E` name
  pushed to atexit in GetAmmoDef and the ServerClassInit of DT_SDKGameRules,
  DT_SDKGameRulesProxy, DT_OverRunGameRulesProxy and DT_WeaponFN303.
- **Extents the reference has no PDB for (39).** The code is identical to ours over the whole
  range (the reference function plus the unmapped chunks after it), but the extent found for
  the 2006-08-18 DLL is cut at a tail call (`jmp` to CVProfile::ExitScope, Msg or UTIL_Remove),
  at a jump table inside the function, or starts at a small function placed before it:
  CAI_BaseNPC::RunAnimation, CAI_AssaultBehavior::CanSelectSchedule, the NetworkVar_m_Collision
  constructor (a 4-instruction setter before it), CFuncRotating::SetTargetSpeed,
  ServiceEventQueue, ConsoleKillTarget, SendProxy_SolidFlags (runs into the ICollideable
  constructor), SendPropInt, CGlobalEntityList::CleanupDeleteList,
  CEnvEffectsScript::IsRootCommand (a string getter before it), CBreakable::Precache and
  CGameMovement::PlayerMove (jump tables), CServerGameClients::PostClientMessagesSent,
  CGameRules::FrameUpdatePostEntityThink, SpawnAllEntities, CommandNavUpdateBlocked (a
  26-instruction function before it), CNavMesh::CreateLadder and CNavLadder::OnSplit (jump
  tables; both are same against the May DLL), CPhysicsHook::FrameUpdatePostEntityThink,
  PhysicsNone, PhysicsPusher, the five static NetworkStateChanged functions of CBasePlayer
  (m_iSecondary, m_flBlur, m_flConstraintRadius twice, m_flLaggedMovementValue: small, alike and
  named from the May DLL, so they pair with a neighbour), CBasePlayer::PhysicsSimulate,
  CLagCompensationManager::FrameUpdatePostEntityThink, UpdateAllClientData, the
  m_flFadeScale `operator+=` of CBreakableProp (a float setter before it),
  CBreakableProp::Spawn, the static CBaseEntity::EmitSound, report_soundpatch (the CMemoryPool
  Alloc and Free wrappers before it), CSoundControllerImp::SystemUpdate, CTextStatInt::PrintFn (a
  list unlink before it; the CTextStatInt constructor then pushes an unnamed address in strict
  mode), CTimedEventMgr::FireEvents, NWCEdit::CreateAILink, GameStartFrame and
  CHDNBot::FindRoute (its tail is the unmapped chunk at 222ab7cd). These are listed for the
  compare tools; no source was changed for them.
- **Register choice and scheduling in Valve code (6).** LoadSchedulesFromBuffer,
  LoadDecalsFromScript, SendPropUtlVector and ParseEntryExitAnims reload two spilled registers
  in the other order at a loop exit; CWeaponShotgun::PrimaryAttack swaps a `push` and a `mov`;
  SampleStep swaps ebx and edi. LoadSchedulesFromBuffer and LoadDecalsFromScript are same against
  the May DLL, so the August build changed them with no change to their own source: the order
  comes from MSVC's internal numbering, which moves with the declarations a header adds. Nothing
  in these functions' source to try.
- **Hidden's code and shared code (7).**
  - CGameMovement::CheckBack: Hidden's server.dll stores m_bSpeedCropped after the multiply.
    Swapping the two statements makes it same in server.dll and changed in the .so, where it is
    same now; `*= 0.8f`, `x = x * 0.8f` and `0.8f * x` were also tried. Left as it is, since it
    would trade one compiler for the other.
  - CBasePlayer::UpdateStun: Hidden's server.dll decays the blur as `( m_flBlur / m_flBlur ) *
    0.2f`, its .so as `( 1.0f / m_flBlur ) * m_flBlur * 0.2f` (ours). The same conflict: the form
    that fits one compiler breaks the other. Also tried: `m_flBlur * ( 1.0f / m_flBlur )`, a
    double reciprocal.
  - CBludgeonWeaponSDKBase::Hit: register choice. Hidden keeps hitActivity in ebp, so pPlayer is
    spilled; ours compares hitActivity in memory. May's Hit is the August code.
  - CPlayerMove::RunCommand: ours keeps `&pl.v_angle` in ebp, Hidden keeps `this`; also changed
    in the .so (block layout).
  - CLagCompensationManager::BacktrackPlayer: also changed in the .so (16 bytes more stack in
    Hidden's build). The likeliest of all to hide a source difference.
  - CNavMesh::CommandNavWarpToMark: block layout and one register; also changed in the .so.
  - CBaseEntity::FireBullets inlines CGameTrace::operator= where Hidden's calls it. The May line
    table has the same Tracer statements, the frame is the same size (0x2a8) and the two call
    lists are the same apart from `??4CGameTrace` against `??4CBaseTrace`. Our DLL has 8 callers
    of the out-of-line CGameTrace::operator= and Hidden's has 9. The header is the same, so this
    is MSVC's inlining budget for this object, which moves with inline bodies elsewhere in the
    headers (as with DataMapInit), not FireBullets' own source.

### Functions that only the strict mode finds (10)

Strict mode adds 10 changed rows against the 2006-08-18 DLL: the `$E` names in GetAmmoDef and the
ServerClassInit functions above, the CTextStatInt constructor (it pushes PrintFn, whose extent is
wrong), and `$E` rows at other numbers (nav_area `$E19`, soundenvelope `$E13`, sdk_gamerules
`$E26` and `$E50`). All are artefacts of the extents or of the `$E` pairing; none is a source
constant.

### Unmapped reference functions

Of the reference functions the compare leaves unmapped, 343 in Hidden's objects are over 32
bytes. 63 are libc, 50 pair at 0.9 or better with one of our unmapped functions, and 210 have an
exact copy somewhere in our DLL (the same code under another extent). The rest are composite
ranges whose pieces exist in ours, jump table data, or the tails of split extents. No unmapped
function references a string that our DLL lacks.

The one candidate for code we do not have is a 12-instruction thiscall function at 2229e300 in
sdk_player.obj, between DetectChange for m_angEyeAngles and the CTEPlayerAnimEvent constructor. It
sets CBasePlayer+0x7a4 (m_iWeighting) to zero through the network variable. Nothing in Hidden's
DLL calls it or points to it, so it is dead code; its name is not known and it was not added.
