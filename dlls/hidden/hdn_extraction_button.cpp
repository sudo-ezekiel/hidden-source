//========= Hidden: Source ============//
//
// Purpose: ext_timer_button, a button a marine has to hold +use on for "time"
//			seconds before it fires OnFinish. If nobody uses it for "reset_time"
//			seconds the count starts over. Built from CMomentaryRotButton.
//
//=====================================//

#include "cbase.h"
#include "doors.h"
#include "buttons.h"
#include "sdk_shareddefs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


string_t MakeButtonSound( int sound );				// get string of button sound number

// The CBaseButton spawn flags this button uses, from buttons.cpp
#define SF_ROTBUTTON_NOTSOLID			1
#define SF_BUTTON_USE_ACTIVATES			1024	// Button fires when used.
#define SF_BUTTON_LOCKED				2048	// Whether the button is initially locked.


class CExtractionUse : public CBaseButton
{
public:
	DECLARE_CLASS( CExtractionUse, CBaseButton );
	DECLARE_DATADESC();

	void	Spawn( void );
	bool	CreateVPhysics( void );
	virtual int	ObjectCaps( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	UpdateTarget( float value, CBaseEntity *pActivator );

	// REBUILD: the names below are ours, except the key values and the output.
	int		m_lastUsed;
	float	m_flLastUse;		// when the button was last used
	float	m_fStartTime;		// the "time" key value, restored on reset
	bool	m_bFinished;		// OnFinish has fired

	float	m_fTime;			// seconds of use left
	float	m_fReset;			// seconds without use before the count resets, 0 for never

	COutputEvent m_OnPressed;
};

LINK_ENTITY_TO_CLASS( ext_timer_button, CExtractionUse );

BEGIN_DATADESC( CExtractionUse )
	DEFINE_KEYFIELD( m_fTime, FIELD_FLOAT, "time" ),
	DEFINE_KEYFIELD( m_fReset, FIELD_FLOAT, "reset_time" ),
	DEFINE_OUTPUT( m_OnPressed, "OnFinish" ),
END_DATADESC()


bool CExtractionUse::CreateVPhysics( void )
{
	VPhysicsInitShadow( false, false );

	return true;
}

void CExtractionUse::UpdateTarget( float value, CBaseEntity *pActivator )
{
}

void CExtractionUse::Spawn( void )
{
	if ( HasSpawnFlags( SF_BUTTON_LOCKED ) )
	{
		m_bLocked = true;
	}

	if ( HasSpawnFlags( SF_BUTTON_USE_ACTIVATES ) )
	{
		m_sNoise = MakeButtonSound( m_sounds );
		PrecacheScriptSound( m_sNoise.ToCStr() );
		m_lastUsed = 0;
		UpdateTarget( 0, this );
	}

	SetSolid( SOLID_VPHYSICS );
	if ( HasSpawnFlags( SF_ROTBUTTON_NOTSOLID ) )
	{
		AddSolidFlags( FSOLID_NOT_SOLID );
	}

	SetMoveType( MOVETYPE_PUSH );
	SetModel( STRING( GetModelName() ) );

	CreateVPhysics();

	// Slam the object back to solid - if we really want it to be solid.
	if ( m_bSolidBsp )
	{
		SetSolid( SOLID_BSP );
	}

	m_bFinished = false;
	m_flLastUse = gpGlobals->curtime;
	m_fStartTime = m_fTime;

	DevMsg( 1, "time [%.2f] reset [%.2f] \n ", m_fTime, m_fReset );
}

int	CExtractionUse::ObjectCaps( void )
{
	return BaseClass::ObjectCaps() | FCAP_CONTINUOUS_USE | FCAP_USE_IN_RADIUS;
}

void CExtractionUse::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator->IsPlayer() || pActivator->GetTeamNumber() == TEAM_HIDDEN )
		return;

	if ( m_fTime < 0 )
	{
		if ( !m_bFinished )
		{
			DevMsg( 1, "Finished Counting\n" );
			m_bFinished = true;
			m_hActivator = pActivator;
			m_OnPressed.FireOutput( m_hActivator, this );
			return;
		}
	}
	else
	{
		// Nobody held it for too long: start over
		if ( m_fReset > 0 && gpGlobals->curtime - m_flLastUse > m_fReset )
		{
			m_fTime = m_fStartTime;
			DevMsg( 1, "RESET\n" );
		}

		m_fTime -= gpGlobals->interval_per_tick;
		DevMsg( 1, "Count = [%.2f]\n", m_fTime );
	}

	m_flLastUse = gpGlobals->curtime;
}
