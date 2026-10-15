//=============================================================================
//
// Purpose: The class every Hidden melee weapon derives from (the knife).
//
//=============================================================================

#include "cbase.h"
#include "in_buttons.h"
#include "takedamageinfo.h"

#if defined( CLIENT_DLL )
	#include "c_sdk_player.h"
	#include "c_te_effect_dispatch.h"
#else
	#include "sdk_player.h"
	#include "te_effect_dispatch.h"
	#include "gib.h"
	#include "physics_prop_ragdoll.h"
	#include "RagdollBoogie.h"
#endif

ConVar sv_pigstick( "sv_pigstick", "1", FCVAR_REPLICATED | FCVAR_NOTIFY );

// REBUILD: included after sv_pigstick, so the hull statics it defines are constructed after the
// ConVar, in the order Hidden's binary has them.
#include "hdn_bludgeonweapon_sdkbase.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_NETWORKCLASS_ALIASED( BludgeonWeaponSDKBase, DT_BludgeonWeaponSDKBase )

BEGIN_NETWORK_TABLE( CBludgeonWeaponSDKBase, DT_BludgeonWeaponSDKBase )
#ifdef CLIENT_DLL

#else
	// world weapon models have no animations
	SendPropExclude( "DT_AnimTimeMustBeFirst", "m_flAnimTime" ),
	SendPropExclude( "DT_BaseAnimating", "m_nSequence" ),
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CBludgeonWeaponSDKBase )
	DEFINE_PRED_FIELD( m_flTimeWeaponIdle, FIELD_FLOAT, FTYPEDESC_OVERRIDE | FTYPEDESC_NOERRORCHECK ),
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( bludgeonweapon_sdk_base, CBludgeonWeaponSDKBase );

#ifdef GAME_DLL

	BEGIN_DATADESC( CBludgeonWeaponSDKBase )
	END_DATADESC()

#endif

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CBludgeonWeaponSDKBase::CBludgeonWeaponSDKBase()
{
	m_fMinRange1 = 0;
	m_fMaxRange1 = 96;
	m_fMinRange2 = 0;
	m_fMaxRange2 = 96;

	m_bFiresUnderwater = true;
}

//-----------------------------------------------------------------------------
// Purpose: Precache the weapon
//-----------------------------------------------------------------------------
void CBludgeonWeaponSDKBase::Precache( void )
{
	//Call base class first
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: Spawn the weapon
//-----------------------------------------------------------------------------
void CBludgeonWeaponSDKBase::Spawn( void )
{
	//Call base class first
	BaseClass::Spawn();

	AddEffects( EF_NOSHADOW );
	m_nBody = 1;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CBludgeonWeaponSDKBase::Deploy( void )
{
	return BaseClass::Deploy();
}

//-----------------------------------------------------------------------------
// Purpose: The knife cannot be put away in the middle of a pigstick
//-----------------------------------------------------------------------------
bool CBludgeonWeaponSDKBase::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	if ( m_bHitDelayed )
		return false;

	return BaseClass::Holster( pSwitchingTo );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBludgeonWeaponSDKBase::PrimaryAttack( void )
{
	if ( m_bHitDelayed )
		return;

	Swing( false );
}

//-----------------------------------------------------------------------------
// Purpose: Start the pigstick; the swing itself lands later in ItemPostFrame
//-----------------------------------------------------------------------------
void CBludgeonWeaponSDKBase::SecondaryAttack( void )
{
	if ( !sv_pigstick.GetBool() )
		return;

	CSDKPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	if ( m_bHitDelayed )
		return;

	if ( pPlayer->GetSafe() )
	{
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.2;
		m_flNextSecondaryAttack = gpGlobals->curtime + 0.2;
		return;
	}

	Activity nActivity = GetSecondaryAttackActivity();

	m_flDelayedHit = gpGlobals->curtime + 1.3f;
	m_flNextSecondaryAttack = gpGlobals->curtime + 3.0f;
	m_flNextPrimaryAttack = gpGlobals->curtime + 2.55f;
	m_bHitDelayed = true;

	SendWeaponAnim( nActivity );
	WeaponSound( SPECIAL1 );

	DevMsg( 1, "Sequence Duration : %.2f\n", SequenceDuration() );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBludgeonWeaponSDKBase::AddViewKick( void )
{
	CSDKPlayer *pPlayer = GetPlayerOwner();
	if ( pPlayer == NULL )
		return;

	pPlayer->ViewPunch( QAngle( random->RandomFloat( -1.0f, 1.0f ), random->RandomFloat( -1.0f, 1.0f ), 0.0f ) );
}

//------------------------------------------------------------------------------
// Purpose : Update weapon
//------------------------------------------------------------------------------
void CBludgeonWeaponSDKBase::ItemPostFrame( void )
{
	CSDKPlayer *pOwner = GetPlayerOwner();
	if ( pOwner == NULL )
		return;

	// Land a pigstick that is due
	if ( m_bHitDelayed && m_flDelayedHit < gpGlobals->curtime )
	{
		Swing( true );
		return;
	}

	if ( (pOwner->m_nButtons & IN_ATTACK2) && (m_flNextSecondaryAttack <= gpGlobals->curtime) && !m_bHitDelayed )
	{
		SecondaryAttack();
	}

	if ( (pOwner->m_nButtons & IN_ATTACK) && (m_flNextPrimaryAttack <= gpGlobals->curtime) && !m_bHitDelayed )
	{
		PrimaryAttack();
	}

	if ( !(pOwner->m_nButtons & IN_ATTACK) && !(pOwner->m_nButtons & IN_ATTACK2) )
	{
		WeaponIdle();
	}
}

//-----------------------------------------------------------------------------
// Animation and timing
//-----------------------------------------------------------------------------
Activity CBludgeonWeaponSDKBase::GetPrimaryAttackActivity( void )
{
	return ACT_VM_HITCENTER;
}

Activity CBludgeonWeaponSDKBase::GetSecondaryAttackActivity( void )
{
	return ACT_VM_HITCENTER2;
}

Activity CBludgeonWeaponSDKBase::GetPrimaryMissActivity( void )
{
	return ACT_VM_MISSCENTER;
}

Activity CBludgeonWeaponSDKBase::GetSecondaryMissActivity( void )
{
	return ACT_VM_MISSCENTER2;
}

float CBludgeonWeaponSDKBase::GetFireRate( void )
{
	return 0.5f;
}

float CBludgeonWeaponSDKBase::GetTime2Idle( void )
{
	return 3.0f;
}

float CBludgeonWeaponSDKBase::GetRange( void )
{
	return m_fMaxRange1;
}

float CBludgeonWeaponSDKBase::GetDamageForActivity( Activity hitActivity )
{
	return 1.0f;
}

//------------------------------------------------------------------------------
// Purpose : Starts the swing of the weapon and determines the animation
// Input   : bIsSecondary - is this a secondary attack?
//------------------------------------------------------------------------------
void CBludgeonWeaponSDKBase::Swing( int bIsSecondary )
{
	// Try a ray
	CSDKPlayer *pOwner = GetPlayerOwner();
	if ( !pOwner )
		return;

	if ( pOwner->GetSafe() )
	{
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.2;
		return;
	}

	Vector vSwingStart = pOwner->Weapon_ShootPosition( );
	Vector vForward;

	pOwner->EyeVectors( &vForward, NULL, NULL );

	Vector vSwingEnd = vSwingStart + vForward * GetRange();

	trace_t tr;
	UTIL_TraceLine( vSwingStart, vSwingEnd, MASK_SHOT_HULL, pOwner, COLLISION_GROUP_NONE, &tr );
	Activity nHitActivity = bIsSecondary ? GetSecondaryAttackActivity() : GetPrimaryAttackActivity();

#ifndef CLIENT_DLL
	// Like bullets, bludgeon traces have to trace against triggers.
	CTakeDamageInfo triggerInfo( GetOwner(), GetOwner(), 25.0f, DMG_CLUB );
	TraceAttackToTriggers( triggerInfo, tr.startpos, tr.endpos, vec3_origin );
#endif

	if ( tr.fraction == 1.0f )
	{
		float bludgeonHullRadius = 1.732f * BLUDGEON_HULL_DIM;  // hull is +/- 16, so use cuberoot of 2 to determine how big the hull is from center to the corner point

		// Back off by hull "radius"
		vSwingEnd -= vForward * bludgeonHullRadius;

		UTIL_TraceHull( vSwingStart, vSwingEnd, g_bludgeonMins, g_bludgeonMaxs, MASK_SHOT_HULL, pOwner, COLLISION_GROUP_NONE, &tr );
		if ( tr.fraction < 1.0 && tr.m_pEnt )
		{
			Vector vToTarget = tr.m_pEnt->GetAbsOrigin() - vSwingStart;
			VectorNormalize( vToTarget );

			float dot = vToTarget.Dot( vForward );

			// YWB:  Make sure they are sort of facing the guy at least...
			if ( dot < 0.70721f )
			{
				// Force amiss
				tr.fraction = 1.0f;
			}
			else
			{
				ChooseIntersectionPointAndActivity( tr, g_bludgeonMins, g_bludgeonMaxs, pOwner );
			}
		}
	}

	// See if we happened to hit water
	ImpactWater( vSwingStart, tr.endpos );

	// -------------------------
	//	Miss
	// -------------------------
	if ( tr.fraction == 1.0f )
	{
		nHitActivity = GetPrimaryMissActivity();
	}
	else
	{
		Hit( tr, nHitActivity );
	}

	WeaponSound( SINGLE );

	// The pigstick animation was already sent when it started
	if ( !bIsSecondary )
	{
		// Send the anim
		SendWeaponAnim( nHitActivity );

		//Setup our next attack times
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->curtime + SequenceDuration();
	}

	m_flTimeWeaponIdle = gpGlobals->curtime + GetTime2Idle();
	m_bHitDelayed = false;
}

//-----------------------------------------------------------------------------
// Purpose: Find the closest point of a hull hit, as Valve's bludgeon does
//-----------------------------------------------------------------------------
void CBludgeonWeaponSDKBase::ChooseIntersectionPointAndActivity( trace_t &tr, const Vector &mins, const Vector &maxs, CSDKPlayer *pPlayer )
{
	int			i, j, k;
	float		lowerDistance;
	const float	*minmaxs[2] = {mins.Base(), maxs.Base()};
	trace_t		tmptr;
	Vector		vecHullEnd = tr.endpos;
	Vector		vecEnd;

	lowerDistance = MAX_TRACE_LENGTH;
	Vector vecSrc = tr.startpos;

	vecHullEnd = vecSrc + ((vecHullEnd - vecSrc)*2);
	UTIL_TraceLine( vecSrc, vecHullEnd, MASK_SHOT_HULL, pPlayer, COLLISION_GROUP_NONE, &tmptr );
	if ( tmptr.fraction == 1.0 )
	{
		for ( i = 0; i < 2; i++ )
		{
			for ( j = 0; j < 2; j++ )
			{
				for ( k = 0; k < 2; k++ )
				{
					vecEnd.x = vecHullEnd.x + minmaxs[i][0];
					vecEnd.y = vecHullEnd.y + minmaxs[j][1];
					vecEnd.z = vecHullEnd.z + minmaxs[k][2];

					UTIL_TraceLine( vecSrc, vecEnd, MASK_SHOT_HULL, pPlayer, COLLISION_GROUP_NONE, &tmptr );
					if ( tmptr.fraction < 1.0 )
					{
						float thisDistance = (tmptr.endpos - vecSrc).Length();
						if ( thisDistance < lowerDistance )
						{
							tr = tmptr;
							lowerDistance = thisDistance;
						}
					}
				}
			}
		}
	}
	else
	{
		tr = tmptr;
	}
}

//------------------------------------------------------------------------------
// Purpose: Implement impact function. A pigstick gibs an IRIS corpse; a slash
//			on a corpse that still has health in it heals the Hidden.
//------------------------------------------------------------------------------
void CBludgeonWeaponSDKBase::Hit( trace_t &tr, Activity hitActivity )
{
	float dmgScale = 37.0f;

	CSDKPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	//Do view kick
	AddViewKick();

	//Apply damage to a hit target
	if ( tr.m_pEnt != NULL )
	{
		Vector vForward;
		pPlayer->EyeVectors( &vForward, NULL, NULL );
		VectorNormalize( vForward );

		if ( hitActivity == ACT_VM_HITCENTER2 )
			dmgScale = 925.0f;

		CTakeDamageInfo dmgInfo( GetOwner(), GetOwner(), dmgScale, DMG_SLASH );

		CalculateMeleeDamageForce( &dmgInfo, vForward, tr.endpos );

		tr.m_pEnt->DispatchTraceAttack( dmgInfo, vForward, &tr );
		ApplyMultiDamage();

		if ( !Q_stricmp( tr.m_pEnt->GetClassname(), "corpse_ragdoll" ) )
		{
#ifndef CLIENT_DLL
			CRagdollProp *pRagdoll = static_cast<CRagdollProp *>( tr.m_pEnt );

			if ( hitActivity != ACT_VM_HITCENTER2 )
			{
				// Feed on the corpse while it has health left
				if ( pRagdoll->GetHealth() > 0 )
				{
					int iHealth = pPlayer->m_iHealth + 5;
					if ( iHealth > 100 )
						iHealth = 100;

					pRagdoll->SetHealth( -5 );
					pPlayer->m_iHealth = iHealth;
				}

				CEffectData	data;

				data.m_vOrigin = tr.m_pEnt->GetAbsOrigin();
				data.m_vNormal = -vForward;
				data.m_flScale = RandomInt( 4, 8 );
				data.m_fFlags = FX_BLOODSPRAY_ALL;
				data.m_nColor = BLOOD_COLOR_RED;

				DispatchEffect( "BloodSpray", data );

				if ( pRagdoll->GetHealth() > 0 )
				{
					CRagdollBoogie::Create( pRagdoll, 250, gpGlobals->curtime, 0.5f );
				}
			}
			else
			{
				// Pigstick: tear the corpse apart
				CreateRagGib( "models/gibs/iris_raggibs_leg.mdl", pRagdoll->GetAbsOrigin(), pRagdoll->GetAbsAngles(), dmgInfo.GetDamageForce(), 10.0f );
				CGib::SpawnSpecificGibs( pRagdoll, 1, 750, 1500, "models/gibs/iris_gibs1.mdl", 10 );
				CGib::SpawnSpecificGibs( pRagdoll, 1, 750, 1500, "models/gibs/iris_gibs1.mdl", 10 );
				CGib::SpawnSpecificGibs( pRagdoll, 1, 750, 1500, "models/gibs/iris_gibs2.mdl", 10 );
				CGib::SpawnSpecificGibs( pRagdoll, 1, 750, 1500, "models/gibs/iris_gibs2.mdl", 10 );
				CGib::SpawnSpecificGibs( pRagdoll, 1, 750, 1500, "models/gibs/iris_gibs3.mdl", 10 );
				CGib::SpawnSpecificGibs( pRagdoll, 1, 750, 1500, "models/gibs/iris_gibs3.mdl", 10 );
				CreateRagGib( "models/gibs/iris_raggibs_arm.mdl", pRagdoll->GetAbsOrigin(), pRagdoll->GetAbsAngles(), dmgInfo.GetDamageForce(), 10.0f );
				CGib::SpawnSpecificGibs( pRagdoll, 1, 750, 1500, "models/gibs/iris_gibs4.mdl", 10 );
				CGib::SpawnSpecificGibs( pRagdoll, 1, 750, 1500, "models/gibs/iris_gibs6.mdl", 10 );
				CGib::SpawnSpecificGibs( pRagdoll, 1, 750, 1500, "models/gibs/iris_gibs7.mdl", 10 );
				CGib::SpawnSpecificGibs( pRagdoll, 1, 750, 1500, "models/gibs/iris_gibs7.mdl", 10 );
				CreateRagGib( "models/gibs/iris_raggibs_leg.mdl", pRagdoll->GetAbsOrigin(), pRagdoll->GetAbsAngles(), dmgInfo.GetDamageForce(), 10.0f );

				UTIL_Remove( pRagdoll );

				UTIL_BloodImpact( tr.m_pEnt->GetAbsOrigin(), vForward, BLOOD_COLOR_RED, 5 );
				UTIL_BloodSpray( tr.m_pEnt->GetAbsOrigin(), -vForward, BLOOD_COLOR_RED, 10, FX_BLOODSPRAY_ALL );
			}
#endif
		}

#ifndef CLIENT_DLL
		// Now hit all triggers along the ray that...
		TraceAttackToTriggers( dmgInfo, tr.startpos, tr.endpos, vForward );
#endif
	}

	// Apply an impact effect
	ImpactEffect( tr );
}

//-----------------------------------------------------------------------------
// Purpose: Splash when the swing goes into water
//-----------------------------------------------------------------------------
void CBludgeonWeaponSDKBase::ImpactWater( const Vector &vStart, const Vector &vEnd )
{
	// We must start outside the water
	if ( UTIL_PointContents( vStart ) & (CONTENTS_WATER|CONTENTS_SLIME))
		return;

	trace_t	tr;

	UTIL_TraceLine( vStart, vEnd, (CONTENTS_WATER|CONTENTS_SLIME), GetOwner(), COLLISION_GROUP_NONE, &tr );

	if ( tr.fraction < 1.0f )
	{
		CEffectData	effectData;

		effectData.m_fFlags  = 0;
		effectData.m_vOrigin = tr.endpos;
		effectData.m_vNormal = tr.plane.normal;
		effectData.m_flScale = 8.0f;

		// See if we hit slime
		if ( tr.contents & CONTENTS_SLIME )
		{
			effectData.m_fFlags |= FX_WATER_IN_SLIME;
		}

		DispatchEffect( "watersplash", effectData );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBludgeonWeaponSDKBase::ImpactEffect( trace_t &tr )
{
	UTIL_ImpactTrace( &tr, DMG_SLASH );
}
