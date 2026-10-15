//=============================================================================
//
// Purpose: The class every Hidden melee weapon derives from (the knife).
//			Based on Valve's CBaseHLBludgeonWeapon. Primary attack is a slash,
//			secondary attack is the pigstick: a delayed heavy stab that gibs
//			IRIS corpses, while slashing a corpse feeds the Hidden.
//
//=============================================================================

#ifndef HDN_BLUDGEONWEAPON_SDKBASE_H
#define HDN_BLUDGEONWEAPON_SDKBASE_H

#ifdef _WIN32
#pragma once
#endif

#include "weapon_sdkbase.h"

#if defined( CLIENT_DLL )
#define CBludgeonWeaponSDKBase C_BludgeonWeaponSDKBase
#endif

#define BLUDGEON_HULL_DIM		16

static const Vector g_bludgeonMins(-BLUDGEON_HULL_DIM,-BLUDGEON_HULL_DIM,-BLUDGEON_HULL_DIM);
static const Vector g_bludgeonMaxs(BLUDGEON_HULL_DIM,BLUDGEON_HULL_DIM,BLUDGEON_HULL_DIM);

//=========================================================
// CBludgeonWeaponSDKBase
//=========================================================
class CBludgeonWeaponSDKBase : public CWeaponSDKBase
{
public:
	DECLARE_CLASS( CBludgeonWeaponSDKBase, CWeaponSDKBase );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

#ifdef GAME_DLL
	DECLARE_DATADESC();
#endif

	CBludgeonWeaponSDKBase();

	virtual	void	Spawn( void );
	virtual	void	Precache( void );

	// Melee weapons are never lowered
	virtual void	SetSafe( void ) {}

	virtual bool	Deploy( void );
	virtual bool	Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
	virtual void	ItemPostFrame( void );

	//Attack functions
	virtual	void	PrimaryAttack( void );
	virtual	void	SecondaryAttack( void );

	//Functions to select animation sequences
	virtual Activity	GetPrimaryAttackActivity( void );
	virtual Activity	GetSecondaryAttackActivity( void );

	virtual	float	GetFireRate( void );
	virtual void	AddViewKick( void );

	virtual Activity	GetPrimaryMissActivity( void );
	virtual Activity	GetSecondaryMissActivity( void );

	virtual void	DrawEffects( void ) {}

	virtual float	GetTime2Idle( void );
	virtual float	GetRange( void );
	virtual	float	GetDamageForActivity( Activity hitActivity );

	CBludgeonWeaponSDKBase( const CBludgeonWeaponSDKBase & );

protected:
	void			ImpactEffect( trace_t &trace );

private:
	void			ImpactWater( const Vector &start, const Vector &end );
	void			Swing( int bIsSecondary );
	void			Hit( trace_t &traceHit, Activity nHitActivity );
	void			ChooseIntersectionPointAndActivity( trace_t &hitTrace, const Vector &mins, const Vector &maxs, CSDKPlayer *pOwner );

	bool			m_bPigsticking;		// REBUILD: name. Set when the pigstick starts, cleared when its swing lands.
	float			m_flPigstickTime;	// REBUILD: name. When the pigstick swing lands.
};

#endif // HDN_BLUDGEONWEAPON_SDKBASE_H
