//========= Hidden: Source ============//
//
// Purpose: info_spectator, the camera new players watch the map from until they
//			join a team (see FinishClientPutInServer). The mapper sets its field of
//			view and a location name that the client shows.
//
//=====================================//

#ifndef HDN_SPECTATOR_H
#define HDN_SPECTATOR_H

#ifdef _WIN32
#pragma once
#endif


#ifdef CLIENT_DLL
	#define CHDNSpectator C_HDNSpectator
#endif


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

#ifndef CLIENT_DLL
	virtual int		ShouldTransmit( const CCheckTransmitInfo *pInfo ) { return FL_EDICT_ALWAYS; }
	virtual int		UpdateTransmitState( void ) { return SetTransmitState( FL_EDICT_ALWAYS ); }
#endif

	// REBUILD: accessor names are ours; the spectator GUI reads the location name.
	float			GetFOV( void ) const { return m_fFOV; }
	const char		*GetLocation( void ) const { return m_Location; }

	CNetworkVar( float, m_fFOV );
	CNetworkString( m_Location, 25 );

	// Key values, copied to the networked members on spawn
	float			m_fFOVValue;
	char			*m_pLocation;	// FIELD_STRING, but used as a plain pointer: no NULL check
};


#endif // HDN_SPECTATOR_H
