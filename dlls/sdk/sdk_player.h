//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose:		Player for SDK Game
//
// $NoKeywords: $
//=============================================================================

#ifndef SDK_PLAYER_H
#define SDK_PLAYER_H
#pragma once


#include "player.h"
#include "server_class.h"
#include "sdk_playeranimstate.h"
#include "sdk_shareddefs.h"

class CBeam;

extern int	gEvilImpulse101;

//=============================================================================
// >> SDK Game player
//=============================================================================
class CSDKPlayer : public CBasePlayer, public ISDKPlayerAnimStateHelpers
{
public:
	DECLARE_CLASS( CSDKPlayer, CBasePlayer );
	DECLARE_SERVERCLASS();
	DECLARE_PREDICTABLE();

	CSDKPlayer();
	~CSDKPlayer();

	static CSDKPlayer *CreatePlayer( const char *className, edict_t *ed );
	static CSDKPlayer* Instance( int iEnt );

	// This passes the event to the client's and server's CPlayerAnimState.
	void DoAnimationEvent( PlayerAnimEvent_t event );

	virtual void PreThink();
	virtual void PostThink();
	virtual void Spawn();
	virtual void InitialSpawn();
	virtual void Precache();
	virtual void Event_Killed( const CTakeDamageInfo &info );
	virtual void LeaveVehicle( const Vector &vecExitPoint, const QAngle &vecExitAngles );
	virtual void CheatImpulseCommands( int iImpulse );

	// Hidden: +use picks up physics objects (the Hidden only) and hands ammo to a marine who asked for it
	virtual void PlayerUse( void );
	virtual void PickupObject( CBaseEntity *pObject, bool bLimitMassAndSize );
	virtual void ForceDropOfCarriedPhysObjects( CBaseEntity *pOnlyIfHoldingThis );
	virtual bool IsHoldingEntity( CBaseEntity *pEnt );

	// Hidden: the marines' flashlight
	virtual int  FlashlightIsOn( void );
	virtual void FlashlightTurnOn( void );
	virtual void FlashlightTurnOff( void );

	// Hidden: +walk
	virtual void StopWalking( void );
	virtual void StartWalking( void );
	bool IsWalking( void ) { return m_bIsWalking; }

	CWeaponSDKBase* GetActiveSDKWeapon() const;
	virtual void	CreateViewModel( int viewmodelindex = 0 );

	CNetworkVar( int, m_iThrowGrenadeCounter );	// used to trigger grenade throw animations.
	CNetworkQAngle( m_angEyeAngles );	// Copied from EyeAngles() so we can send it to the client.
	CNetworkVar( int, m_iShotsFired );	// number of shots fired recently

	// Tracks our ragdoll entity.
	CNetworkHandle( CBaseEntity, m_hRagdoll );	// networked entity handle

	bool m_bIsWalking;
	bool m_bPlayUseDenySound;		// REBUILD: name, from CHL2_Player; set when +use finds nothing

// In shared code.
public:
	// ISDKPlayerAnimState overrides.
	virtual CWeaponSDKBase* SDKAnim_GetActiveWeapon();
	virtual bool SDKAnim_CanMove();


	void FireBullet(
		Vector vecSrc,
		const QAngle &shootAngles,
		float vecSpread,
		int iDamage,
		int iBulletType,
		CBaseEntity *pevAttacker,
		bool bDoEffects,
		float x,
		float y );

private:

	void CreateRagdollEntity( const CTakeDamageInfo &info );

	ISDKPlayerAnimState *m_PlayerAnimState;

	int		m_iGameType;			// REBUILD: name; from the map prefix: 1 normal, 2 ovr_ (Overrun), 3 mtr_, 4 htr_ (tutorials)
	int		m_iUnknownE48;			// REBUILD: 4 bytes at 0xe48, no use found
	CBeam	*m_pFlashlightBeam;		// REBUILD: name; only FlashlightTurnOff reads it, nothing sets it
};


inline CSDKPlayer *ToSDKPlayer( CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsPlayer() )
		return NULL;

#ifdef _DEBUG
	Assert( dynamic_cast<CSDKPlayer*>( pEntity ) != 0 );
#endif
	return static_cast< CSDKPlayer* >( pEntity );
}


#endif	// SDK_PLAYER_H
