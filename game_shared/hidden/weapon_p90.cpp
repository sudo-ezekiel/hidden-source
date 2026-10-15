//=============================================================================
//
// Purpose: IRIS FN P90 (weapon_p90). Built from the SDK's MP5.
//
//=============================================================================

#include "cbase.h"
#include "weapon_sdkbase.h"
#include "sdk_fx_shared.h"

#if defined( CLIENT_DLL )

	#define CWeaponP90 C_WeaponP90
	#include "c_sdk_player.h"
	#include "c_te_effect_dispatch.h"

#else

	#include "sdk_player.h"
	#include "te_effect_dispatch.h"

#endif

class CWeaponP90 : public CWeaponSDKBase
{
public:
	DECLARE_CLASS( CWeaponP90, CWeaponSDKBase );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
	CWeaponP90();

	virtual void SetSafe();
	virtual bool Deploy();
	virtual bool Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
	virtual void WeaponIdle();
	virtual bool Reload();
	virtual void PrimaryAttack();

	virtual SDKWeaponID GetWeaponID( void ) const		{ return WEAPON_P90; }

private:
	CWeaponP90( const CWeaponP90 & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponP90, DT_WeaponP90 )

BEGIN_NETWORK_TABLE( CWeaponP90, DT_WeaponP90 )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponP90 )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_p90, CWeaponP90 );
PRECACHE_WEAPON_REGISTER( weapon_p90 );
CWeaponP90::CWeaponP90()
{
}

// Lower the P90 when the player goes safe
void CWeaponP90::SetSafe()
{
	BaseClass::SetSafe();
}

bool CWeaponP90::Holster( CBaseCombatWeapon *pSwitchingTo )
{
#ifndef CLIENT_DLL
	CSDKPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return false;

	// Show the P90 on the player model
	if ( pPlayer->m_lifeState == LIFE_ALIVE )
		pPlayer->SetBodygroup( 2, 0 );
#endif

	return BaseClass::Holster( pSwitchingTo );
}

void CWeaponP90::WeaponIdle()
{
	if (m_flTimeWeaponIdle > gpGlobals->curtime)
		return;

	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( !pPlayer )
		return;

	if ( pPlayer->GetSafe() )
	{
		SendWeaponAnim( ACT_VM_LOWERED );
		SetWeaponIdleTime( gpGlobals->curtime + 5.0f );
	}
	// only idle if the slid isn't back
	else if ( m_iClip1 != 0 )
	{
		SetWeaponIdleTime( gpGlobals->curtime + 5.0f );
		SendWeaponAnim( ACT_VM_IDLE );
	}
}

bool CWeaponP90::Reload()
{
	CSDKPlayer *pPlayer = GetPlayerOwner();

	if (pPlayer->GetAmmoCount( GetPrimaryAmmoType() ) <= 0)
		return false;

	int iResult = DefaultReload( GetMaxClip1(), GetMaxClip2(), ACT_VM_RELOAD );
	if ( !iResult )
		return false;

	pPlayer->SetAnimation( PLAYER_RELOAD );

#ifndef CLIENT_DLL
	SendReloadEvents();

	if ((iResult) && (pPlayer->GetFOV() != pPlayer->GetDefaultFOV()))
	{
		pPlayer->SetFOV( pPlayer, pPlayer->GetDefaultFOV() );
	}
#endif

	// Drop the empty magazine
	CEffectData data;
	data.m_vOrigin = pPlayer->GetLocalOrigin();
	data.m_vAngles = pPlayer->GetLocalAngles();
	DispatchEffect( "EjectFNP90Clip", data );

	pPlayer->m_iShotsFired = 0;

	return true;
}

bool CWeaponP90::Deploy()
{
	CSDKPlayer *pPlayer = GetPlayerOwner();
	pPlayer->m_iShotsFired = 0;

	if ( pPlayer && pPlayer->GetSafe() )
	{
		SendWeaponAnim( ACT_VM_LOWERED );
		SetWeaponIdleTime( gpGlobals->curtime + 0.1 );
	}

	return BaseClass::Deploy();
}

void CWeaponP90::PrimaryAttack( void )
{
	const CSDKWeaponInfo &pWeaponInfo = GetSDKWpnData();
	CSDKPlayer *pPlayer = GetPlayerOwner();

	// No firing while safe
	if ( pPlayer->GetSafe() )
	{
		m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
		return;
	}

	if ( !IsDeployed() )
		return;
	float flCycleTime = pWeaponInfo.m_flCycleTime;
	float flSpread = 0.03f;

	// more spread when jumping
	if ( !FBitSet( pPlayer->GetFlags(), FL_ONGROUND ) )
		flSpread = 0.08f;
	pPlayer->m_iShotsFired++;

	// Out of ammo?
	if ( m_iClip1 <= 0 )
	{
		if (m_bFireOnEmpty)
		{
			PlayEmptySound();
			m_flNextPrimaryAttack = gpGlobals->curtime + 0.2;
		}
	}

	pPlayer->DoMuzzleFlash();
	SendWeaponAnim( ACT_VM_PRIMARYATTACK );

	m_iClip1--;
	// player "shoot" animation
	pPlayer->SetAnimation( PLAYER_ATTACK1 );

	FX_FireBullets(
		pPlayer->entindex(),
		pPlayer->Weapon_ShootPosition(),
		pPlayer->EyeAngles() + 2.0f * pPlayer->GetPunchAngle(),
		GetWeaponID(),
		Primary_Mode,
		CBaseEntity::GetPredictionRandomSeed() & 255,
		flSpread );

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->curtime + flCycleTime;

	if (!m_iClip1 && pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) <= 0)
	{
		// HEV suit - indicate out of ammo condition
		pPlayer->SetSuitUpdate("!HEV_AMO0", false, 0);
	}
	// start idle animation in 5 seconds
	SetWeaponIdleTime( gpGlobals->curtime + 5.0 );

	// Update punch angles.
	QAngle angle = pPlayer->GetPunchAngle();
	angle.x -= !(pPlayer->GetFlags() & FL_ONGROUND ) ? SHARED_RANDOMINT( 5, 8 ) : SHARED_RANDOMINT( 1, 3 );
	pPlayer->SetPunchAngle( angle );
}
