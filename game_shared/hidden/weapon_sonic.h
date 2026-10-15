//=============================================================================
//
// Purpose: IRIS sonic alarm (weapon_sonic), the class declaration. Built from
//			the HL2MP SLAM header (weapon_slam.h).
//
//=============================================================================

#ifndef WEAPON_SONIC_H
#define WEAPON_SONIC_H
#ifdef _WIN32
#pragma once
#endif

#include "weapon_sdkbase.h"

enum
{
	SLAM_TRIPMINE_READY,
	SLAM_SATCHEL_THROW,
	SLAM_SATCHEL_ATTACH,
};

#ifdef CLIENT_DLL
#define CWeapon_Sonic C_Weapon_Sonic
#endif

class CWeapon_Sonic : public CWeaponSDKBase
{
public:
	DECLARE_CLASS( CWeapon_Sonic, CWeaponSDKBase );

	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	CNetworkVar( int,	m_tSlamState );
	CNetworkVar( bool,				m_bDetonatorArmed );
	CNetworkVar( bool,				m_bNeedDetonatorDraw);
	CNetworkVar( bool,				m_bNeedDetonatorHolster);
	CNetworkVar( bool,				m_bNeedReload);
	CNetworkVar( bool,				m_bClearReload);
	CNetworkVar( bool,				m_bThrowSatchel);
	CNetworkVar( bool,				m_bAttachSatchel);
	CNetworkVar( bool,				m_bAttachTripmine);
	float				m_flWallSwitchTime;

	void				Spawn( void );
	void				Precache( void );

	void				PrimaryAttack( void );
	void				SecondaryAttack( void );
	void				WeaponIdle( void );
	void				Weapon_Switch( void );	// REBUILD: declared and never defined, as in the HL2MP SLAM; the PDB has it
	void				SLAMThink( void );
	
	void				SetPickupTouch( void );
	void				SlamTouch( CBaseEntity *pOther );	// default weapon touch
	void				ItemPostFrame( void );	
	bool				Reload( void );
	void				SetSlamState( int newState );
	bool				CanAttachSLAM(void);		// In position where can attach SLAM?
	void				StartTripmineAttach( void );
	void				TripmineAttach( void );

	bool				Deploy( void );
	bool				Holster( CBaseCombatWeapon *pSwitchingTo = NULL );

#ifdef CLIENT_DLL
	void				DrawEffects( void );
#endif

	CWeapon_Sonic();

#ifndef CLIENT_DLL
	DECLARE_DATADESC();
#endif

private:
	CWeapon_Sonic( const CWeapon_Sonic & );
};

#endif // WEAPON_SONIC_H
