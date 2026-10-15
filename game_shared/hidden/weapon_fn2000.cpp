//=============================================================================
//
// Purpose: IRIS FN F2000 assault rifle (weapon_fn2000). Built from the SDK's
//			MP5, with the scope zoom of Valve's HL2MP crossbow.
//
//=============================================================================

#include "cbase.h"
#include "weapon_sdkbase.h"
#include "sdk_fx_shared.h"
#include "in_buttons.h"

#if defined( CLIENT_DLL )

	#define CWeaponFN2000 C_WeaponFN2000
	#include "c_sdk_player.h"
	#include "c_te_effect_dispatch.h"

#else

	#include "sdk_player.h"
	#include "te_effect_dispatch.h"

#endif

class CWeaponFN2000 : public CWeaponSDKBase
{
public:
	DECLARE_CLASS( CWeaponFN2000, CWeaponSDKBase );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
	CWeaponFN2000();

	virtual void SetSafe();
	virtual bool Deploy();
	virtual bool Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
	virtual void ItemPostFrame();
	virtual void ItemBusyFrame();
	virtual void WeaponIdle();
	virtual bool Reload();
	virtual void PrimaryAttack();

	virtual SDKWeaponID GetWeaponID( void ) const		{ return WEAPON_FN2000; }

private:

	CWeaponFN2000( const CWeaponFN2000 & );

	void	CheckZoomToggle( void );
	void	ToggleZoom( void );

	CNetworkVar( bool,	m_bInZoom );

	float	m_flLastShot;	// a shot more than a second after the last one is perfectly accurate
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponFN2000, DT_WeaponFN2000 )

BEGIN_NETWORK_TABLE( CWeaponFN2000, DT_WeaponFN2000 )
#ifdef CLIENT_DLL
	RecvPropBool( RECVINFO( m_bInZoom ) ),
#else
	SendPropBool( SENDINFO( m_bInZoom ) ),
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponFN2000 )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_fn2000, CWeaponFN2000 );
PRECACHE_WEAPON_REGISTER( weapon_fn2000 );

CWeaponFN2000::CWeaponFN2000()
{
}

// Lower the rifle when the player goes safe
void CWeaponFN2000::SetSafe()
{
	BaseClass::SetSafe();
}

bool CWeaponFN2000::Deploy()
{
	CSDKPlayer *pPlayer = GetPlayerOwner();
	pPlayer->m_iShotsFired = 0;
	m_flLastShot = gpGlobals->curtime - 1.0;
	if ( pPlayer && pPlayer->GetSafe() )
	{
		SendWeaponAnim( ACT_VM_LOWERED );
		SetWeaponIdleTime( gpGlobals->curtime + 0.1 );
	}

	return BaseClass::Deploy();
}

void CWeaponFN2000::WeaponIdle()
{
	if (m_flTimeWeaponIdle > gpGlobals->curtime)
		return;

	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( !pPlayer )
		return;

	if ( pPlayer->GetSafe() )
	{
		SendWeaponAnim( ACT_VM_LOWERED );
		SetWeaponIdleTime( gpGlobals->curtime + 5.0 );
	}
	// only idle if the slid isn't back
	else if ( m_iClip1 != 0 )
	{
		SetWeaponIdleTime( gpGlobals->curtime + 5.0 );
		SendWeaponAnim( ACT_VM_IDLE );
	}
}

void CWeaponFN2000::ItemBusyFrame( void )
{
	// Allow zoom toggling even when we're reloading
	CheckZoomToggle();

	BaseClass::ItemBusyFrame();
}

void CWeaponFN2000::ItemPostFrame( void )
{
	// Allow zoom toggling
	CheckZoomToggle();

	BaseClass::ItemPostFrame();
}

//-----------------------------------------------------------------------------
// Purpose: Zoom the scope in or out
//-----------------------------------------------------------------------------
void CWeaponFN2000::ToggleZoom( void )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	if ( !IsDeployed() || pPlayer == NULL )
		return;
#ifndef CLIENT_DLL	// the server zooms; the client has no SetFOV or SetZoom
	if ( m_bInZoom )
	{
		if ( pPlayer->SetFOV( this, 0, 0.2f ) )
		{
			m_bInZoom = false;
			pPlayer->SetZoom( false );
		}
	}
	else
	{
		if ( pPlayer->SetFOV( this, 20, 0.1f ) )
		{
			m_bInZoom = true;
			pPlayer->SetZoom( true );
		}
	}
#endif
}

void CWeaponFN2000::CheckZoomToggle( void )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( pPlayer->m_afButtonPressed & IN_ATTACK2 )
	{
		ToggleZoom();
	}
}

void CWeaponFN2000::PrimaryAttack( void )
{
	const CSDKWeaponInfo &pWeaponInfo = GetSDKWpnData();
	CSDKPlayer *pPlayer = GetPlayerOwner();

	if ( !IsDeployed() )
		return;

	// No firing while safe
	if ( pPlayer->GetSafe() )
	{
		m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
		return;
	}

	float flCycleTime = pWeaponInfo.m_flCycleTime;
	// tighter when zoomed, perfect for a first shot after a pause
	float flSpread;
	if ( m_bInZoom )
		flSpread = 0.01f;
	else
		flSpread = 0.03f;
	if ( m_flLastShot + 1.0 < gpGlobals->curtime )
		flSpread = 0.0f;

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

	m_flLastShot = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWeaponFN2000::Holster( CBaseCombatWeapon *pSwitchingTo )
{
#ifndef CLIENT_DLL
	CSDKPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return false;

	// Show the rifle on the player model
	if ( pPlayer->m_lifeState == LIFE_ALIVE )
		pPlayer->SetBodygroup( 2, 0 );
#endif

	if ( m_bInZoom )
		ToggleZoom();

	return BaseClass::Holster( pSwitchingTo );
}

bool CWeaponFN2000::Reload()
{
	CSDKPlayer *pPlayer = GetPlayerOwner();

	if (pPlayer->GetAmmoCount( GetPrimaryAmmoType() ) <= 0)
		return false;

	int iResult = DefaultReload( GetMaxClip1(), GetMaxClip2(), ACT_VM_RELOAD );
	if ( !iResult )
		return false;

	if ( m_bInZoom )
		ToggleZoom();

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
	DispatchEffect( "EjectFN2000Clip", data );

	pPlayer->m_iShotsFired = 0;

	return true;
}
