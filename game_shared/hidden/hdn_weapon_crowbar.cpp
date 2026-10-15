//=============================================================================
//
// Purpose: The Hidden's knife (weapon_knife). Kept the HL2 crowbar's class name.
//
//=============================================================================

#include "cbase.h"
#include "hdn_bludgeonweapon_sdkbase.h"

#if defined( CLIENT_DLL )
	#include "c_sdk_player.h"
#else
	#include "sdk_player.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#if defined( CLIENT_DLL )
#define CWeaponCrowbar C_WeaponCrowbar
#endif

//-----------------------------------------------------------------------------
// CWeaponCrowbar
//-----------------------------------------------------------------------------
class CWeaponCrowbar : public CBludgeonWeaponSDKBase
{
public:
	DECLARE_CLASS( CWeaponCrowbar, CBludgeonWeaponSDKBase );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponCrowbar() { m_fMaxRange1 = 65; }

	virtual SDKWeaponID GetWeaponID( void ) const { return WEAPON_CROWBAR; }

	virtual void	SetSafe( void ) {}

#ifdef CLIENT_DLL
	virtual int		DrawModel( int flags );
#endif

private:
	CWeaponCrowbar( const CWeaponCrowbar & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponCrowbar, DT_WeaponCrowbar )

BEGIN_NETWORK_TABLE( CWeaponCrowbar, DT_WeaponCrowbar )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponCrowbar )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_knife, CWeaponCrowbar );
PRECACHE_WEAPON_REGISTER( weapon_knife );

#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: The world model is not drawn while a living player carries the knife
//-----------------------------------------------------------------------------
int CWeaponCrowbar::DrawModel( int flags )
{
	C_BasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( pPlayer && pPlayer->IsAlive() )
		return 1;

	return BaseClass::DrawModel( flags );
}
#endif
