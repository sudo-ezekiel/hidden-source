//========= Hidden: Source ============//
//
// Purpose: ext_extraction_point, the brush the marines must reach in evacuation
//			maps. It keeps a list of the living non-Hidden players inside and works
//			out what share of the living marines that is.
//
//			REBUILD: in this build it only reports through DevMsg; failure_message
//			and time are read from the map but not used.
//
//=====================================//

#include "cbase.h"
#include "basetoggle.h"
#include "player.h"
#include "sdk_shareddefs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


struct tPlayer
{
	CBaseEntity *pPlayer;	// REBUILD: name ours
};

class CExtractionBrush : public CBaseToggle
{
public:
	DECLARE_CLASS( CExtractionBrush, CBaseToggle );
	DECLARE_DATADESC();

	void	Spawn( void );
	void	OnThink( void );

	virtual void StartTouch( CBaseEntity *pOther );
	virtual void EndTouch( CBaseEntity *pOther );

	string_t	m_FailureMessage;
	float		m_fTime;
	float		m_fPercentage;		// share of the living marines needed inside
	bool		m_bTriggered;		// REBUILD: name ours
	bool		m_bVisible;

	CUtlVector<tPlayer>	m_Players;	// REBUILD: name ours. The players inside
};

LINK_ENTITY_TO_CLASS( ext_extraction_point, CExtractionBrush );

BEGIN_DATADESC( CExtractionBrush )
	DEFINE_KEYFIELD( m_fTime, FIELD_FLOAT, "time" ),
	DEFINE_KEYFIELD( m_fPercentage, FIELD_FLOAT, "percentage" ),
	DEFINE_KEYFIELD( m_FailureMessage, FIELD_STRING, "failure_message" ),
	DEFINE_KEYFIELD( m_bVisible, FIELD_BOOLEAN, "is_visible" ),
	DEFINE_THINKFUNC( OnThink ),
END_DATADESC()


void CExtractionBrush::OnThink( void )
{
	SetNextThink( gpGlobals->curtime + 0.05f );

	if ( m_Players.Count() <= 0 )
		return;

	int iMarines = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( pPlayer && pPlayer->m_lifeState == LIFE_ALIVE )
		{
			if ( pPlayer->GetTeamNumber() == TEAM_IRIS )
				iMarines++;
		}
	}

	if ( iMarines == 0 )
		return;

	float fPercentage = ( (float)m_Players.Count() / (float)iMarines ) * 100.0f;
	DevMsg( 1, "Number of Alive Marines:[%i] Percentage:[%.2f]\n", iMarines, fPercentage );

	if ( m_bTriggered )
	{
		if ( fPercentage < m_fPercentage )
		{
			DevMsg( 1, "Cancel Extraction\n" );
			m_bTriggered = false;
		}
	}
	else
	{
		if ( fPercentage > m_fPercentage )
			DevMsg( 1, "Trigger Extraction\n" );

		// REBUILD: set even when the share is still too low, as in the original
		m_bTriggered = true;
	}
}

void CExtractionBrush::Spawn( void )
{
	m_bTriggered = false;

	SetMoveType( MOVETYPE_NONE );
	SetSolid( GetParent() ? SOLID_VPHYSICS : SOLID_BSP );
	AddSolidFlags( FSOLID_NOT_SOLID );
	AddSolidFlags( FSOLID_TRIGGER );

	if ( !m_bVisible )
		AddEffects( EF_NODRAW );

	m_Players.RemoveAll();

	SetThink( &CExtractionBrush::OnThink );
	SetNextThink( gpGlobals->curtime + 0.05f );

	SetModel( STRING( GetModelName() ) );
}

void CExtractionBrush::EndTouch( CBaseEntity *pOther )
{
	for ( int i = 0; i < m_Players.Count(); i++ )
	{
		if ( pOther == m_Players[i].pPlayer )
		{
			m_Players.Remove( i );
			DevMsg( 1, "Players in Extraction Point :[%i]\n", m_Players.Count() );
		}
	}
}

void CExtractionBrush::StartTouch( CBaseEntity *pOther )
{
	if ( pOther->IsPlayer() && pOther->m_lifeState == LIFE_ALIVE && pOther->GetTeamNumber() != TEAM_HIDDEN )
	{
		for ( int i = 0; i < m_Players.Count(); i++ )
		{
			if ( pOther == m_Players[i].pPlayer )
				return;
		}

		tPlayer player;
		player.pPlayer = pOther;
		m_Players.AddToTail( player );

		DevMsg( 1, "Players in Extraction Point :[%i]\n", m_Players.Count() );
	}
}
