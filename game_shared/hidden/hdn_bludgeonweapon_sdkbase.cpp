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
// Purpose: Precache the weapon
//-----------------------------------------------------------------------------
void CBludgeonWeaponSDKBase::Precache( void )
{
	//Call base class first
	BaseClass::Precache();
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
	if ( m_bPigsticking )
		return false;

	return BaseClass::Holster( pSwitchingTo );
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

//-----------------------------------------------------------------------------
// Purpose: Find the closest point of a hull hit, as Valve's bludgeon does
//-----------------------------------------------------------------------------
void CBludgeonWeaponSDKBase::ChooseIntersectionPointAndActivity( trace_t &hitTrace, const Vector &mins, const Vector &maxs, CSDKPlayer *pOwner )
{
	int			i, j, k;
	float		distance;
	const float	*minmaxs[2] = {mins.Base(), maxs.Base()};
	trace_t		tmpTrace;
	Vector		vecHullEnd = hitTrace.endpos;
	Vector		vecEnd;

	distance = MAX_TRACE_LENGTH;
	Vector vecSrc = hitTrace.startpos;

	vecHullEnd = vecSrc + ((vecHullEnd - vecSrc)*2);
	UTIL_TraceLine( vecSrc, vecHullEnd, MASK_SHOT_HULL, pOwner, COLLISION_GROUP_NONE, &tmpTrace );
	if ( tmpTrace.fraction == 1.0 )
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

					UTIL_TraceLine( vecSrc, vecEnd, MASK_SHOT_HULL, pOwner, COLLISION_GROUP_NONE, &tmpTrace );
					if ( tmpTrace.fraction < 1.0 )
					{
						float thisDistance = (tmpTrace.endpos - vecSrc).Length();
						if ( thisDistance < distance )
						{
							hitTrace = tmpTrace;
							distance = thisDistance;
						}
					}
				}
			}
		}
	}
	else
	{
		hitTrace = tmpTrace;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Splash when the swing goes into water
//-----------------------------------------------------------------------------
void CBludgeonWeaponSDKBase::ImpactWater( const Vector &start, const Vector &end )
{
	// We must start outside the water
	if ( UTIL_PointContents( start ) & (CONTENTS_WATER|CONTENTS_SLIME))
		return;

	trace_t	waterTrace;

	UTIL_TraceLine( start, end, (CONTENTS_WATER|CONTENTS_SLIME), GetOwner(), COLLISION_GROUP_NONE, &waterTrace );

	if ( waterTrace.fraction < 1.0f )
	{
#ifndef CLIENT_DLL
		CEffectData	data;

		data.m_fFlags  = 0;
		data.m_vOrigin = waterTrace.endpos;
		data.m_vNormal = waterTrace.plane.normal;
		data.m_flScale = 8.0f;

		// See if we hit slime
		if ( waterTrace.contents & CONTENTS_SLIME )
		{
			data.m_fFlags |= FX_WATER_IN_SLIME;
		}

		DispatchEffect( "watersplash", data );
#endif
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBludgeonWeaponSDKBase::ImpactEffect( trace_t &traceHit )
{
	UTIL_ImpactTrace( &traceHit, DMG_SLASH );
}

//------------------------------------------------------------------------------
// Purpose: Implement impact function. A pigstick gibs an IRIS corpse; a slash
//			on a corpse that still has health in it heals the Hidden.
//------------------------------------------------------------------------------
void CBludgeonWeaponSDKBase::Hit( trace_t &traceHit, Activity nHitActivity )
{
	float flDamage = 37.0f;

	CSDKPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	//Do view kick
	AddViewKick();

	//Apply damage to a hit target
	if ( traceHit.m_pEnt != NULL )
	{
		Vector hitDirection;
		pPlayer->EyeVectors( &hitDirection, NULL, NULL );
		VectorNormalize( hitDirection );

#ifndef CLIENT_DLL
		if ( nHitActivity == ACT_VM_HITCENTER2 )
			flDamage = 925.0f;

		CTakeDamageInfo info( GetOwner(), GetOwner(), flDamage, DMG_SLASH );

		CalculateMeleeDamageForce( &info, hitDirection, traceHit.endpos );

		traceHit.m_pEnt->DispatchTraceAttack( info, hitDirection, &traceHit );
		ApplyMultiDamage();

		CRagdollProp *pRagdoll = static_cast<CRagdollProp *>( traceHit.m_pEnt );
		if ( !Q_stricmp( pRagdoll->GetClassname(), "corpse_ragdoll" ) )
		{
			if ( nHitActivity != ACT_VM_HITCENTER2 )
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

				data.m_vOrigin = traceHit.m_pEnt->GetAbsOrigin();
				data.m_vNormal = -hitDirection;
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
				CreateRagGib( "models/gibs/iris_raggibs_leg.mdl", pRagdoll->GetAbsOrigin(), pRagdoll->GetAbsAngles(), info.GetDamageForce(), 10.0f );
				CGib::SpawnSpecificGibs( pRagdoll, 1, 750, 1500, "models/gibs/iris_gibs1.mdl", 10 );
				CGib::SpawnSpecificGibs( pRagdoll, 1, 750, 1500, "models/gibs/iris_gibs1.mdl", 10 );
				CGib::SpawnSpecificGibs( pRagdoll, 1, 750, 1500, "models/gibs/iris_gibs2.mdl", 10 );
				CGib::SpawnSpecificGibs( pRagdoll, 1, 750, 1500, "models/gibs/iris_gibs2.mdl", 10 );
				CGib::SpawnSpecificGibs( pRagdoll, 1, 750, 1500, "models/gibs/iris_gibs3.mdl", 10 );
				CGib::SpawnSpecificGibs( pRagdoll, 1, 750, 1500, "models/gibs/iris_gibs3.mdl", 10 );
				CreateRagGib( "models/gibs/iris_raggibs_arm.mdl", pRagdoll->GetAbsOrigin(), pRagdoll->GetAbsAngles(), info.GetDamageForce(), 10.0f );
				CGib::SpawnSpecificGibs( pRagdoll, 1, 750, 1500, "models/gibs/iris_gibs4.mdl", 10 );
				CGib::SpawnSpecificGibs( pRagdoll, 1, 750, 1500, "models/gibs/iris_gibs6.mdl", 10 );
				CGib::SpawnSpecificGibs( pRagdoll, 1, 750, 1500, "models/gibs/iris_gibs7.mdl", 10 );
				CGib::SpawnSpecificGibs( pRagdoll, 1, 750, 1500, "models/gibs/iris_gibs7.mdl", 10 );
				CreateRagGib( "models/gibs/iris_raggibs_leg.mdl", pRagdoll->GetAbsOrigin(), pRagdoll->GetAbsAngles(), info.GetDamageForce(), 10.0f );

				UTIL_Remove( pRagdoll );

				UTIL_BloodImpact( traceHit.m_pEnt->GetAbsOrigin(), hitDirection, BLOOD_COLOR_RED, 5 );
				UTIL_BloodSpray( traceHit.m_pEnt->GetAbsOrigin(), -hitDirection, BLOOD_COLOR_RED, 10, FX_BLOODSPRAY_ALL );
			}
		}

		// Now hit all triggers along the ray that...
		TraceAttackToTriggers( info, traceHit.startpos, traceHit.endpos, hitDirection );
#endif
	}

	// Apply an impact effect
	ImpactEffect( traceHit );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBludgeonWeaponSDKBase::PrimaryAttack( void )
{
	if ( m_bPigsticking )
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

	if ( m_bPigsticking )
		return;

	if ( pPlayer->GetSafe() )
	{
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.2;
		m_flNextSecondaryAttack = gpGlobals->curtime + 0.2;
		return;
	}

	Activity nActivity = GetSecondaryAttackActivity();

	m_flPigstickTime = gpGlobals->curtime + 1.3f;
	m_flNextSecondaryAttack = gpGlobals->curtime + 3.0f;
	m_flNextPrimaryAttack = gpGlobals->curtime + 2.55f;
	m_bPigsticking = true;

	SendWeaponAnim( nActivity );
	WeaponSound( SPECIAL1 );

	DevMsg( 1, "Sequence Duration : %.2f\n", SequenceDuration() );
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

	Vector swingStart = pOwner->Weapon_ShootPosition( );
	Vector forward;

	pOwner->EyeVectors( &forward, NULL, NULL );

	Vector swingEnd = swingStart + forward * GetRange();

	trace_t traceHit;
	UTIL_TraceLine( swingStart, swingEnd, MASK_SHOT_HULL, pOwner, COLLISION_GROUP_NONE, &traceHit );
	Activity nHitActivity = bIsSecondary ? GetSecondaryAttackActivity() : GetPrimaryAttackActivity();

#ifndef CLIENT_DLL
	// Like bullets, bludgeon traces have to trace against triggers.
	CTakeDamageInfo triggerInfo( GetOwner(), GetOwner(), 25.0f, DMG_CLUB );
	TraceAttackToTriggers( triggerInfo, traceHit.startpos, traceHit.endpos, vec3_origin );
#endif

	if ( traceHit.fraction == 1.0 )
	{
		float bludgeonHullRadius = 1.732f * BLUDGEON_HULL_DIM;  // hull is +/- 16, so use cuberoot of 2 to determine how big the hull is from center to the corner point

		// Back off by hull "radius"
		swingEnd -= forward * bludgeonHullRadius;

		UTIL_TraceHull( swingStart, swingEnd, g_bludgeonMins, g_bludgeonMaxs, MASK_SHOT_HULL, pOwner, COLLISION_GROUP_NONE, &traceHit );
		if ( traceHit.fraction < 1.0 && traceHit.m_pEnt )
		{
			Vector vecToTarget = traceHit.m_pEnt->GetAbsOrigin() - swingStart;
			VectorNormalize( vecToTarget );

			float dot = vecToTarget.Dot( forward );

			// YWB:  Make sure they are sort of facing the guy at least...
			if ( dot < 0.70721f )
			{
				// Force amiss
				traceHit.fraction = 1.0f;
			}
			else
			{
				ChooseIntersectionPointAndActivity( traceHit, g_bludgeonMins, g_bludgeonMaxs, pOwner );
			}
		}
	}

	// See if we happened to hit water
	ImpactWater( swingStart, traceHit.endpos );

	// -------------------------
	//	Miss
	// -------------------------
	if ( traceHit.fraction == 1.0f )
	{
		nHitActivity = GetPrimaryMissActivity();
	}
	else
	{
		Hit( traceHit, nHitActivity );
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
	m_bPigsticking = false;
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
	if ( m_bPigsticking && m_flPigstickTime < gpGlobals->curtime )
	{
		Swing( true );
		return;
	}

	if ( (pOwner->m_nButtons & IN_ATTACK2) && (m_flNextSecondaryAttack <= gpGlobals->curtime) && !m_bPigsticking )
	{
		SecondaryAttack();
	}

	if ( (pOwner->m_nButtons & IN_ATTACK) && (m_flNextPrimaryAttack <= gpGlobals->curtime) && !m_bPigsticking )
	{
		PrimaryAttack();
	}

	if ( !(pOwner->m_nButtons & IN_ATTACK) && !(pOwner->m_nButtons & IN_ATTACK2) )
	{
		WeaponIdle();
	}
}
