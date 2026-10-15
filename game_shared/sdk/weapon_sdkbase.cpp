//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "in_buttons.h"
#include "takedamageinfo.h"
#include "weapon_sdkbase.h"
#include "ammodef.h"
#include "hidden/hdn_lasersight.h"


#if defined( CLIENT_DLL )

	#include "c_sdk_player.h"

#else

	#include "sdk_player.h"

#endif


// ----------------------------------------------------------------------------- //
// Global functions.
// ----------------------------------------------------------------------------- //

//--------------------------------------------------------------------------------------------------------
static const char * s_WeaponAliasInfo[] = 
{
	"none",		// WEAPON_NONE
	"crowbar",	// WEAPON_CROWBAR
	"pistol",	// WEAPON_PISTOL
	"pistol2",	// WEAPON_PISTOL2
	"p90",		// WEAPON_P90
	"fn2000",	// WEAPON_FN2000
	"fn303",	// WEAPON_FN303
	"shotgun",	// WEAPON_SHOTGUN
	"grenade",	// WEAPON_GRENADE
	NULL,		// WEAPON_NONE
};

//--------------------------------------------------------------------------------------------------------
//
// Given an alias, return the associated weapon ID
//
int AliasToWeaponID( const char *alias )
{
	if (alias)
	{
		for( int i=0; s_WeaponAliasInfo[i] != NULL; ++i )
			if (!Q_stricmp( s_WeaponAliasInfo[i], alias ))
				return i;
	}

	return WEAPON_NONE;
}

//--------------------------------------------------------------------------------------------------------
//
// Given a weapon ID, return its alias
//
const char *WeaponIDToAlias( int id )
{
	if ( (id >= WEAPON_MAX) || (id < 0) )
		return NULL;

	return s_WeaponAliasInfo[id];
}

// ----------------------------------------------------------------------------- //
// CWeaponSDKBase tables.
// ----------------------------------------------------------------------------- //

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponSDKBase, DT_WeaponSDKBase )

BEGIN_NETWORK_TABLE( CWeaponSDKBase, DT_WeaponSDKBase )
#ifdef CLIENT_DLL
  
#else
	// world weapon models have no animations
  	SendPropExclude( "DT_AnimTimeMustBeFirst", "m_flAnimTime" ),
	SendPropExclude( "DT_BaseAnimating", "m_nSequence" ),
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponSDKBase )
	DEFINE_PRED_FIELD( m_flTimeWeaponIdle, FIELD_FLOAT, FTYPEDESC_OVERRIDE | FTYPEDESC_NOERRORCHECK ),
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_sdk_base, CWeaponSDKBase );


#ifdef GAME_DLL

	BEGIN_DATADESC( CWeaponSDKBase )

		// New weapon Think and Touch Functions go here..

	END_DATADESC()

#endif

// ----------------------------------------------------------------------------- //
// CWeaponCSBase implementation. 
// ----------------------------------------------------------------------------- //
CWeaponSDKBase::CWeaponSDKBase()
{
	SetPredictionEligible( true );

	AddSolidFlags( FSOLID_TRIGGER ); // Nothing collides with these but it gets touches.
}

const CSDKWeaponInfo &CWeaponSDKBase::GetSDKWpnData() const
{
	const FileWeaponInfo_t *pWeaponInfo = &GetWpnData();
	const CSDKWeaponInfo *pSDKInfo;

	#ifdef _DEBUG
		pSDKInfo = dynamic_cast< const CSDKWeaponInfo* >( pWeaponInfo );
		Assert( pSDKInfo );
	#else
		pSDKInfo = static_cast< const CSDKWeaponInfo* >( pWeaponInfo );
	#endif

	return *pSDKInfo;
}

bool CWeaponSDKBase::PlayEmptySound()
{
	CPASAttenuationFilter filter( this );
	filter.UsePredictionRules();

	EmitSound( filter, entindex(), "Default.ClipEmpty_Rifle" );
	
	return 0;
}

CSDKPlayer* CWeaponSDKBase::GetPlayerOwner() const
{
	return dynamic_cast< CSDKPlayer* >( GetOwner() );
}

bool CWeaponSDKBase::Deploy()
{
	if ( m_bDeployed )
		return true;

	m_bDeployed = true;

	CSDKPlayer *pPlayer = GetPlayerOwner();

	// REBUILD: equipment 0 is the laser aiming module
	if ( !pPlayer->GetSafe() && pPlayer->GetEquipment() == 0 && pPlayer->LAMIsOn() )
		CreateLaserPointer();

	m_bJustDeployed = true;

	return BaseClass::Deploy();
}

bool CWeaponSDKBase::ShouldRemoveOnRoundRestart() const
{
	if ( GetPlayerOwner() )
		return true;

	return false;
}

#ifdef GAME_DLL

void CWeaponSDKBase::SendReloadEvents()
{
	CSDKPlayer *pPlayer = dynamic_cast< CSDKPlayer* >( GetOwner() );
	if ( !pPlayer )
		return;

	// Send a message to any clients that have this entity to play the reload.
	CPASFilter filter( pPlayer->GetAbsOrigin() );
	filter.RemoveRecipient( pPlayer );

	// Make the player play his reload animation.
	pPlayer->DoAnimationEvent( PLAYERANIMEVENT_RELOAD );
}

#endif

//-----------------------------------------------------------------------------
// Purpose: Move the laser dot to where the weapon is pointing
//-----------------------------------------------------------------------------
void CWeaponSDKBase::UpdateLaserPosition( Vector vecMuzzlePos, Vector vecEndPos )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( !pPlayer )
		return;

	if ( !pPlayer->LAMIsOn() )
		return;

	if ( vecMuzzlePos == vec3_origin || vecEndPos == vec3_origin )
	{
		vecMuzzlePos = pPlayer->Weapon_ShootPosition();

		Vector	forward;
		pPlayer->EyeVectors( &forward );
		vecEndPos = vecMuzzlePos + ( forward * MAX_TRACE_LENGTH );
	}

	// Trace out for the endpoint
	trace_t	tr;
	UTIL_TraceLine( vecMuzzlePos, vecEndPos, (MASK_SHOT & ~CONTENTS_WINDOW), GetOwner(), COLLISION_GROUP_NONE, &tr );

	// Move the laser sprite
	if ( m_hLaserDot != NULL )
	{
		Vector	laserPos = tr.endpos;
		m_hLaserDot->SetLaserPosition( laserPos, tr.plane.normal );
		
		if ( tr.DidHitNonWorldEntity() )
		{
			CBaseEntity *pHit = tr.m_pEnt;

			if ( ( ( pHit != NULL ) && ( pHit->m_takedamage ) ) || pHit->GetTeamNumber() == TEAM_HIDDEN )
			{
				m_hLaserDot->SetTargetEntity( pHit );
			}
			else
			{
				m_hLaserDot->SetTargetEntity( NULL );
			}
		}
		else
		{
			m_hLaserDot->SetTargetEntity( NULL );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Create the laser dot if there is none yet
//-----------------------------------------------------------------------------
void CWeaponSDKBase::CreateLaserPointer( void )
{
	if ( !m_hLaserDot && GetOwner() != NULL )
	{
		m_hLaserDot = CLaserDot::Create( GetAbsOrigin(), GetOwner() );

		UpdateLaserPosition();
	}
}

void CWeaponSDKBase::ItemPostFrame( void )
{
	BaseClass::ItemPostFrame();

	if ( m_bInReload )
		return;

	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( !pPlayer )
		return;

	if ( !pPlayer->IsOnLadder() && pPlayer->GetTeamNumber() == TEAM_IRIS )
		Deploy();

	if ( pPlayer->GetSafe() || pPlayer->GetTeamNumber() == TEAM_HIDDEN )
		return;

	UpdateLaserPosition();

	if ( pPlayer->LAMIsOn() && m_hLaserDot == NULL )
	{
		DevMsg( 1, "Creating laser pointer in ItemPostFrame\n" );
		CreateLaserPointer();
	}

	if ( !pPlayer->LAMIsOn() && m_hLaserDot )
	{
		m_hLaserDot->TurnOff();
		UTIL_Remove( m_hLaserDot );
		m_hLaserDot = NULL;
	}
}

bool CWeaponSDKBase::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	if ( m_hLaserDot != NULL )
	{
		m_hLaserDot->TurnOff();
		UTIL_Remove( m_hLaserDot );
		m_hLaserDot = NULL;
	}

	m_bDeployed = false;

	return BaseClass::Holster( pSwitchingTo );
}

void CWeaponSDKBase::SetSafe( void )
{
	SendWeaponAnim( ACT_VM_IDLE_TO_LOWERED );
	SetWeaponIdleTime( gpGlobals->curtime + 5.0f );

	if ( m_hLaserDot != NULL )
	{
		m_hLaserDot->TurnOff();
		UTIL_Remove( m_hLaserDot );
		m_hLaserDot = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Same as CBaseCombatWeapon::Drop
//-----------------------------------------------------------------------------
void CWeaponSDKBase::Drop( const Vector &vecVelocity )
{
#if !defined( CLIENT_DLL )

	// Once somebody drops a gun, it's fair game for removal when/if
	// a game_weapon_manager does a cleanup on surplus weapons in the
	// world.
	SetRemoveable( true );

	//If it was dropped then there's no need to respawn it.
	AddSpawnFlags( SF_NORESPAWN );

	StopAnimation();
	StopFollowingEntity( );
	SetMoveType( MOVETYPE_FLYGRAVITY );
	// clear follow stuff, setup for collision
	SetGravity(1.0);
	m_iState = WEAPON_NOT_CARRIED;
	RemoveEffects( EF_NODRAW );
	FallInit();
	SetGroundEntity( NULL );
	SetThink( &CBaseCombatWeapon::SetPickupTouch );
	SetTouch(NULL);

	IPhysicsObject *pObj = VPhysicsGetObject();
	if ( pObj != NULL )
	{
		AngularImpulse	angImp( 200, 200, 200 );
		pObj->AddVelocity( &vecVelocity, &angImp );
	}
	else
	{
		SetAbsVelocity( vecVelocity );
	}

	SetNextThink( gpGlobals->curtime + 1.0f );
	SetOwnerEntity( NULL );
	SetOwner( NULL );
#endif
}