//========= Hidden: Source ============//
//
// Purpose: info_spectator, the camera new players watch the map from until they
//			join a team (see FinishClientPutInServer). The mapper sets its field of
//			view and a location name that the client shows.
//
//=====================================//

#include "cbase.h"
#include "hidden/hdn_spectator.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


BEGIN_DATADESC( CHDNSpectator )
	DEFINE_KEYFIELD( m_fFOVValue, FIELD_FLOAT, "fov" ),
	DEFINE_KEYFIELD( m_pLocation, FIELD_STRING, "location" ),
#ifndef CLIENT_DLL
	DEFINE_THINKFUNC( Think ),
#endif
END_DATADESC()

BEGIN_NETWORK_TABLE( CHDNSpectator, DT_HDNSpectator )
#ifdef CLIENT_DLL
	RecvPropFloat( RECVINFO( m_fFOV ) ),
	RecvPropString( RECVINFO( m_Location ) ),
#else
	SendPropFloat( SENDINFO( m_fFOV ) ),
	SendPropString( SENDINFO( m_Location ) ),
#endif
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( info_spectator, CHDNSpectator );
IMPLEMENT_NETWORKCLASS_ALIASED( HDNSpectator, DT_HDNSpectator )


void CHDNSpectator::Precache( void )
{
	PrecacheModel( HDN_SPECTATOR_MODEL );
}

void CHDNSpectator::Spawn( void )
{
	Precache();
	AddEFlags( EFL_FORCE_CHECK_TRANSMIT );
	SetModel( HDN_SPECTATOR_MODEL );
	SetSolid( SOLID_NONE );

#ifndef CLIENT_DLL
	m_fFOV = m_fFOVValue;
	Q_strncpy( m_Location.GetForModify(), m_pLocation, sizeof( m_Location ) );
#endif
}

void CHDNSpectator::Think( void )
{
}
