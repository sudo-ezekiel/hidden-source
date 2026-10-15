//=============================================================================
//
// Purpose: IRIS second sidearm (weapon_pistol2). The same code as weapon_pistol
//			with its own spread, a faster dry refire and the laser updated every frame.
//
//=============================================================================

#include "cbase.h"
#include "weapon_sdkbase.h"
#include "sdk_fx_shared.h"
#include "in_buttons.h"
#include "hdn_lasersight.h"

#if defined( CLIENT_DLL )

	#define CWeaponPistol2 C_WeaponPistol2
	#include "c_sdk_player.h"

#else

	#include "sdk_player.h"
	#include "te_effect_dispatch.h"

#endif

#define	PISTOL_FASTEST_REFIRE_TIME		0.1f
#define	PISTOL_FASTEST_DRY_REFIRE_TIME	0.1f

#define	PISTOL_ACCURACY_MAXIMUM_PENALTY_TIME	1.5f	// Maximum penalty to deal out

class CWeaponPistol2 : public CWeaponSDKBase
{
public:
	DECLARE_CLASS( CWeaponPistol2, CWeaponSDKBase );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
	CWeaponPistol2();

	virtual void Precache();
	virtual void SetSafe();
	virtual bool Deploy();
	virtual bool Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
	virtual void ItemPreFrame();
	virtual void ItemPostFrame();
	virtual void ItemBusyFrame();
	virtual void WeaponIdle();
	virtual bool Reload();
	virtual void PrimaryAttack();

	virtual SDKWeaponID GetWeaponID( void ) const		{ return WEAPON_PISTOL2; }

	virtual void UpdatePenaltyTime();

private:

	CWeaponPistol2( const CWeaponPistol2 & );

	CNetworkVar( float,	m_flSoonestPrimaryAttack );
	CNetworkVar( float,	m_flLastAttackTime );
	CNetworkVar( float,	m_flAccuracyPenalty );
	CNetworkVar( int,	m_nNumShotsFired );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponPistol2, DT_WeaponPistol2 )

BEGIN_NETWORK_TABLE( CWeaponPistol2, DT_WeaponPistol2 )
#ifdef CLIENT_DLL
	RecvPropTime( RECVINFO( m_flSoonestPrimaryAttack ) ),
	RecvPropTime( RECVINFO( m_flLastAttackTime ) ),
	RecvPropFloat( RECVINFO( m_flAccuracyPenalty ) ),
	RecvPropInt( RECVINFO( m_nNumShotsFired ) ),
#else
	SendPropTime( SENDINFO( m_flSoonestPrimaryAttack ) ),
	SendPropTime( SENDINFO( m_flLastAttackTime ) ),
	SendPropFloat( SENDINFO( m_flAccuracyPenalty ) ),
	SendPropInt( SENDINFO( m_nNumShotsFired ) ),
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponPistol2 )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_pistol2, CWeaponPistol2 );
PRECACHE_WEAPON_REGISTER( weapon_pistol2 );



CWeaponPistol2::CWeaponPistol2()
{
	m_flSoonestPrimaryAttack = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: Precache the laser aiming module too
//-----------------------------------------------------------------------------
void CWeaponPistol2::Precache()
{
	PrecacheMaterial( RPG_LASER_SPRITE );
	PrecacheMaterial( RPG_BEAM_SPRITE );
	PrecacheMaterial( RPG_BEAM_SPRITE_NOZ );

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: Lower the pistol when the player goes safe
//-----------------------------------------------------------------------------
void CWeaponPistol2::SetSafe()
{
	BaseClass::SetSafe();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWeaponPistol2::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	CSDKPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return false;

	// Show the pistol in its holster on the player model
	if ( pPlayer->m_lifeState == LIFE_ALIVE )
		pPlayer->SetBodygroup( 2, 2 );

	return BaseClass::Holster( pSwitchingTo );
}

void CWeaponPistol2::WeaponIdle()
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

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPistol2::ItemPreFrame( void )
{
	UpdatePenaltyTime();

	BaseClass::ItemPreFrame();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPistol2::ItemBusyFrame( void )
{
	UpdatePenaltyTime();

	BaseClass::ItemBusyFrame();
}

bool CWeaponPistol2::Reload()
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

	// Drop the empty magazine
	CEffectData data;
	data.m_vOrigin = pPlayer->Weapon_ShootPosition();
	data.m_vAngles = pPlayer->GetLocalAngles();
	DispatchEffect( "EjectPistolClip", data );
#endif

	pPlayer->m_iShotsFired = 0;

	return true;
}

bool CWeaponPistol2::Deploy()
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

//-----------------------------------------------------------------------------
// Purpose: Allows firing as fast as button is pressed
//-----------------------------------------------------------------------------
void CWeaponPistol2::ItemPostFrame( void )
{
	BaseClass::ItemPostFrame();

	if ( m_bInReload )
		return;
	
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

	if ( pOwner == NULL )
		return;

	if ( pOwner->GetSafe() )
		return;

	UpdateLaserPosition();

	//Allow a refire as fast as the player can click
	if ( ( ( pOwner->m_nButtons & IN_ATTACK ) == false ) && ( m_flSoonestPrimaryAttack < gpGlobals->curtime ) )
	{
		m_flNextPrimaryAttack = gpGlobals->curtime - 0.1f;
	}
	else if ( ( pOwner->m_nButtons & IN_ATTACK ) && ( m_flNextPrimaryAttack < gpGlobals->curtime ) && ( m_iClip1 <= 0 ) )
	{
		// Dry fire
		WeaponSound( EMPTY );
		SendWeaponAnim( ACT_VM_DRYFIRE );
		
		m_flSoonestPrimaryAttack	= gpGlobals->curtime + PISTOL_FASTEST_DRY_REFIRE_TIME;
		m_flNextPrimaryAttack		= gpGlobals->curtime + SequenceDuration();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponPistol2::UpdatePenaltyTime( void )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

	if ( pOwner == NULL )
		return;

	// Check our penalty time decay
	if ( ( ( pOwner->m_nButtons & IN_ATTACK ) == false ) && ( m_flSoonestPrimaryAttack < gpGlobals->curtime ) )
	{
		m_flAccuracyPenalty -= gpGlobals->frametime;
		m_flAccuracyPenalty = clamp( m_flAccuracyPenalty, 0.0f, PISTOL_ACCURACY_MAXIMUM_PENALTY_TIME );
	}
}

void CWeaponPistol2::PrimaryAttack( void )
{
	CSDKPlayer *pPlayer = GetPlayerOwner();

	if ( !m_bDeployed )
		return;

	// No firing while safe
	if ( pPlayer->GetSafe() )
	{
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.2;
		return;
	}

	m_flLastAttackTime = gpGlobals->curtime;
	m_flSoonestPrimaryAttack = gpGlobals->curtime + PISTOL_FASTEST_REFIRE_TIME;

	float flSpread = 0.04f;

	// more spread when jumping
	if ( !FBitSet( pPlayer->GetFlags(), FL_ONGROUND ) )
		flSpread = 0.09f;
	
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

	SendWeaponAnim( ACT_VM_PRIMARYATTACK );

	m_iClip1--;

	// player "shoot" animation
	pPlayer->SetAnimation( PLAYER_ATTACK1 );

	float flCycleTime = SequenceDuration() + 0.1f;

	FX_FireBullets(
		pPlayer->entindex(),
		pPlayer->Weapon_ShootPosition(),
		pPlayer->EyeAngles() + 1.0f * pPlayer->GetPunchAngle(),	// REBUILD: a 1.0 punch scale, as the binary copies the punch like the shotgun's 5.0f * punch
		GetWeaponID(),
		Primary_Mode,
		CBaseEntity::GetPredictionRandomSeed() & 255,
		flSpread );

	pPlayer->DoMuzzleFlash();
	
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->curtime + flCycleTime;

	if (!m_iClip1 && pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) <= 0)
	{
		// HEV suit - indicate out of ammo condition
		pPlayer->SetSuitUpdate("!HEV_AMO0", false, 0);
	}

	// start idle animation in 5 seconds
	SetWeaponIdleTime( gpGlobals->curtime + 5.0 );
}
