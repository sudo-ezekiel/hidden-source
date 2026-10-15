//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef WEAPON_SDKBASE_H
#define WEAPON_SDKBASE_H
#ifdef _WIN32
#pragma once
#endif

#include "sdk_playeranimstate.h"
#include "sdk_weapon_parse.h"

#if defined( CLIENT_DLL )
	#define CWeaponSDKBase C_WeaponSDKBase
	#define CLaserDot C_LaserDot
#endif

class CSDKPlayer;
class CLaserDot;

// These are the names of the ammo types that the weapon script files reference.
#define AMMO_BULLETS			"AMMO_BULLETS"
#define AMMO_ROCKETS			"AMMO_ROCKETS"
#define AMMO_GRENADE			"AMMO_GRENADE"

//--------------------------------------------------------------------------------------------------------
//
// Weapon IDs for all SDK Game weapons
//
typedef enum
{
	WEAPON_NONE = 0,

	WEAPON_CROWBAR,		// the Hidden's knife
	WEAPON_PISTOL,
	WEAPON_PISTOL2,
	WEAPON_P90,
	WEAPON_FN2000,
	WEAPON_FN303,
	WEAPON_SHOTGUN,
	WEAPON_GRENADE,		// the Hidden's pipe bomb
	
	WEAPON_MAX,		// number of weapons weapon index
} SDKWeaponID;

typedef enum
{
	Primary_Mode = 0,
	Secondary_Mode,
} SDKWeaponMode;

const char *WeaponIDToAlias( int id );

class CWeaponSDKBase : public CBaseCombatWeapon
{
public:
	DECLARE_CLASS( CWeaponSDKBase, CBaseCombatWeapon );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	CWeaponSDKBase();

	#ifdef GAME_DLL
		DECLARE_DATADESC();
	#endif

	// All predicted weapons need to implement and return true
	virtual bool	IsPredicted() const { return true; }
	virtual SDKWeaponID GetWeaponID( void ) const { return WEAPON_NONE; }

	virtual bool	Deploy( void );
	virtual bool	Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
	virtual void	ItemPostFrame( void );
	virtual void	Drop( const Vector &vecVelocity );

	// Lower the weapon when the player goes safe
	virtual void	SetSafe( void );

	// Laser aiming module
	virtual void	UpdateLaserPosition( Vector vecMuzzlePos = vec3_origin, Vector vecEndPos = vec3_origin );
	virtual void	CreateLaserPointer( void );
	
	// Get SDK weapon specific weapon data.
	CSDKWeaponInfo const	&GetSDKWpnData() const;

	// Get a pointer to the player that owns this weapon
	CSDKPlayer* GetPlayerOwner() const;

	bool ShouldRemoveOnRoundRestart() const;

	// override to play custom empty sounds
	virtual bool PlayEmptySound();

#ifdef GAME_DLL
	virtual void SendReloadEvents();
#endif

protected:
	bool	m_bDeployed;		// REBUILD: name. Set by Deploy, cleared by Holster; the weapons only fire when it is set.
	bool	m_bJustDeployed;	// REBUILD: name. Set by Deploy, cleared by the sonic weapon; nothing on the server reads it.

	CHandle<CLaserDot>	m_hLaserDot;

private:
	CWeaponSDKBase( const CWeaponSDKBase & );
};


#endif // WEAPON_SDKBASE_H
