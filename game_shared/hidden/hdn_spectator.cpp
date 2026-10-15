//========= Hidden: Source ============//
//
// Purpose: info_spectator, the camera new players watch the map from until they
//			join a team (see FinishClientPutInServer). The mapper sets its field of
//			view and a location name that the client shows.
//
//			REBUILD: only the server half is reconstructed; the client class is not
//			in the server binary.
//
//=====================================//

#include "cbase.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#ifndef CLIENT_DLL

#define HDN_SPECTATOR_MODEL	"models/generic/gen_model_spec_camera.mdl"

class CHDNSpectator : public CBaseAnimating
{
public:
	DECLARE_CLASS( CHDNSpectator, CBaseAnimating );
	DECLARE_DATADESC();
	DECLARE_NETWORKCLASS();

	virtual void	Precache( void );
	virtual void	Spawn( void );
	virtual void	Think( void );

	virtual int		ShouldTransmit( const CCheckTransmitInfo *pInfo ) { return FL_EDICT_ALWAYS; }
	virtual int		UpdateTransmitState( void ) { return SetTransmitState( FL_EDICT_ALWAYS ); }

	CNetworkVar( float, m_fFOV );
	CNetworkString( m_Location, 25 );

	// Key values, copied to the networked members on spawn
	float			m_fFOVValue;
	char			*m_pLocation;	// FIELD_STRING, but used as a plain pointer: no NULL check
};

BEGIN_DATADESC( CHDNSpectator )
	DEFINE_KEYFIELD( m_fFOVValue, FIELD_FLOAT, "fov" ),
	DEFINE_KEYFIELD( m_pLocation, FIELD_STRING, "location" ),
	DEFINE_THINKFUNC( Think ),
END_DATADESC()

BEGIN_NETWORK_TABLE( CHDNSpectator, DT_HDNSpectator )
	SendPropFloat( SENDINFO( m_fFOV ) ),
	SendPropString( SENDINFO( m_Location ) ),
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( info_spectator, CHDNSpectator );
IMPLEMENT_NETWORKCLASS_ALIASED( HDNSpectator, DT_HDNSpectator )


void CHDNSpectator::Precache( void )
{
	PrecacheModel( HDN_SPECTATOR_MODEL );
}

void CHDNSpectator::Think( void )
{
}

void CHDNSpectator::Spawn( void )
{
	Precache();
	AddEFlags( EFL_FORCE_CHECK_TRANSMIT );
	SetModel( HDN_SPECTATOR_MODEL );
	SetSolid( SOLID_NONE );

	m_fFOV = m_fFOVValue;
	Q_strncpy( m_Location.GetForModify(), m_pLocation, sizeof( m_Location ) );
}

#endif
