# Server (server_i486.so) status

## Summary

The rebuilt Linux server is compared function by function with Hidden's shipped, unstripped
`server_i486.so` (build of 2006-08-18). Of 18413 functions:

| same | changed | only in Hidden's build | only in ours |
|---|---|---|---|
| 18364 | 49 | 0 | 0 |

"Same" means the function compiles to exactly the same instructions as Hidden's, once addresses are
normalised (call and jump targets become symbol names or offsets inside the function, absolute
addresses become a placeholder). The build uses GCC 3.4 for i386 with Hidden's flags
(`-mtune=i686 -march=pentium -mmmx -O3`). Nothing is missing: every function in Hidden's binary
exists in the rebuild, and the rebuild has no extra functions.

These counts are from the original (now `--loose`) comparison, which hid every number above 0xFFFF
and the data behind addresses. The strict comparison, now the default of `compare.sh`, also checks
large constants, float constants, strings and the symbol behind every address: it gives 18346 same
and 67 changed. The 18 extra functions differ in a constant (a trace mask, a ConVar default, a
float); `STRICT-COMPARE.md` lists them with Hidden's value, ours and the source line.

None of the 49 "changed" functions is known to behave differently from Hidden's. Each one has the
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

## The 49 remaining functions

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
| weapon_sdkbase.cpp | CWeaponSDKBase::Holster | 0.938 | Register choice. |

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
| weapon_pistol.cpp | CWeaponPistol::ItemPostFrame | 0.930 | Register choice around the inlined dry-fire WeaponSound call. |

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
| player_lagcompensation.cpp | CLagCompensationManager::BacktrackPlayer | 0.792 | Hidden's build has 16 bytes more stack, from extra temporaries. Of the 49 this is the one most likely to hide a real source difference that has not been found. |
| sdk_gamerules.cpp | CSDKGameRules::RestartRound | 0.729 | The inlined CountHiddens and TotalPlayers loops have a different loop shape (rotated against jump-to-condition). Each of those functions is same on its own. |
| sdk_gamerules.cpp | CSDKGameRules::IsRoundOver | 0.825 | Hidden's build has a duplicated loop-entry block that we could not reproduce; a different loop form in the source is possible. |
