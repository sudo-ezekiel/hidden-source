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
	#include "materialsystem/MaterialSystemUtil.h"

	#define CWeaponSDKBase C_WeaponSDKBase
	#define CLaserDot C_LaserDot
#endif

class CSDKPlayer;
class CLaserDot;
#if defined( CLIENT_DLL )
class Beam_t;
#endif

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

#ifdef CLIENT_DLL
	// The laser beam is drawn on the client, the way Valve's HL2MP RPG draws its guide laser.
	virtual bool	IsTranslucent( void );
	virtual void	DrawEffects( void );
#endif
	
	// Get SDK weapon specific weapon data.
	CSDKWeaponInfo const	&GetSDKWpnData() const;

	// Get a pointer to the player that owns this weapon
	CSDKPlayer* GetPlayerOwner() const;

	bool ShouldRemoveOnRoundRestart() const;

	// override to play custom empty sounds
	virtual bool PlayEmptySound();

	// The server's weapons only fire once Deploy has run. REBUILD: Hidden's client.dll, built
	// two months before its server, has no such test, so the client always says yes.
	bool	IsDeployed( void ) const
	{
#ifdef CLIENT_DLL
		return true;
#else
		return m_bDeployed;
#endif
	}

#ifdef GAME_DLL
	virtual void SendReloadEvents();
#else
	// We need to render opaque and translucent pieces
	virtual RenderGroup_t	GetRenderGroup( void ) { return RENDER_GROUP_TWOPASS; }

	virtual void	Release( void );
	virtual void	NotifyShouldTransmit( ShouldTransmitState_t state );
	virtual int		DrawModel( int flags );
	virtual void	ViewModelDrawn( C_BaseViewModel *pBaseViewModel );

	// View model bob, from Valve's HL2 weapons
	virtual void	AddViewmodelBob( CBaseViewModel *viewmodel, Vector &origin, QAngle &angles );
	virtual float	CalcViewmodelBob( void );

	void			InitBeam( void );
	void			GetWeaponAttachment( int attachmentId, Vector &outVector, Vector *dir = NULL );
#endif

protected:
#ifdef CLIENT_DLL
	int					m_iUnknown;			// REBUILD: 4 bytes in Hidden's client that no code reads or writes
	CMaterialReference	m_hSpriteMaterial;	// Used for the laser glint
	CMaterialReference	m_hBeamMaterial;	// Used for the laser beam
	Beam_t				*m_pBeam;			// Laser beam temp entity
#endif

	bool	m_bDeployed;		// REBUILD: name. Set by Deploy, cleared by Holster; the weapons only fire when it is set.
	bool	m_bJustDeployed;	// REBUILD: name. Set by Deploy, cleared by the sonic weapon; nothing on the server reads it.

#ifndef CLIENT_DLL
	CHandle<CLaserDot>	m_hLaserDot;
#endif

private:
	CWeaponSDKBase( const CWeaponSDKBase & );
};


#endif // WEAPON_SDKBASE_H
