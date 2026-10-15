//========= Hidden: Source ============//
//
// Purpose: location_brush, names the part of the map a player is in. Touching it
//			sets the player's location (shown in team chat and on the HUD) and
//			fires a "player_location" event when the location changes.
//
//=====================================//

#include "cbase.h"
#include "basetoggle.h"
#include "player.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


class CLocationBrush : public CBaseToggle
{
public:
	DECLARE_CLASS( CLocationBrush, CBaseToggle );
	DECLARE_DATADESC();

	void	Spawn( void );
	void	BrushTouch( CBaseEntity *pOther );

	char	*m_Location;		// FIELD_STRING, but used as a plain pointer: no NULL check
	bool	m_location_change;			// the last toucher's location changed
};

LINK_ENTITY_TO_CLASS( location_brush, CLocationBrush );

BEGIN_DATADESC( CLocationBrush )
	DEFINE_KEYFIELD( m_Location, FIELD_STRING, "location" ),
	DEFINE_FUNCTION( BrushTouch ),
END_DATADESC()


void CLocationBrush::Spawn( void )
{
	m_location_change = true;
	SetTouch( &CLocationBrush::BrushTouch );

	SetMoveType( MOVETYPE_NONE );
	SetSolid( GetParent() ? SOLID_VPHYSICS : SOLID_BSP );
	AddSolidFlags( FSOLID_NOT_SOLID );
	AddSolidFlags( FSOLID_TRIGGER );
	AddEffects( EF_NODRAW );

	SetModel( STRING( GetModelName() ) );
}

void CLocationBrush::BrushTouch( CBaseEntity *pOther )
{
	if ( !pOther->IsPlayer() || pOther->m_lifeState != LIFE_ALIVE )
		return;

	CBasePlayer *pPlayer = static_cast<CBasePlayer *>( pOther );
	m_location_change = pPlayer->SetCurrentLocation( m_Location );

	if ( m_location_change == true )
	{
		IGameEvent *event = gameeventmanager->CreateEvent( "player_location" );
		if ( event )
		{
			event->SetInt( "userid", engine->GetPlayerUserId( pPlayer->edict() ) );
			event->SetString( "location", m_Location );
			gameeventmanager->FireEvent( event );
		}
	}
}
