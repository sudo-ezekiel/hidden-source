//========= Hidden: Source ============//
//
// Purpose: marine_clip, an invisible player clip brush. Its collision group lets
//			ordinary objects through (see COLLISION_GROUP_MARINECLIP in
//			CSDKGameRules::ShouldCollide).
//
//=====================================//

#include "cbase.h"
#include "basetoggle.h"
#include "sdk_shareddefs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


class CMarineClip : public CBaseToggle
{
public:
	DECLARE_CLASS( CMarineClip, CBaseToggle );
	DECLARE_DATADESC();

	void	Spawn( void );
	void	BrushTouch( CBaseEntity *pOther );
};

LINK_ENTITY_TO_CLASS( marine_clip, CMarineClip );

BEGIN_DATADESC( CMarineClip )
	DEFINE_FUNCTION( BrushTouch ),
END_DATADESC()


void CMarineClip::Spawn( void )
{
	SetTouch( &CMarineClip::BrushTouch );

	SetMoveType( MOVETYPE_NONE );
	SetSolid( SOLID_BSP );
	AddEffects( EF_NODRAW );
	SetCollisionGroup( COLLISION_GROUP_MARINECLIP );

	SetModel( STRING( GetModelName() ) );
}

void CMarineClip::BrushTouch( CBaseEntity *pOther )
{
}
