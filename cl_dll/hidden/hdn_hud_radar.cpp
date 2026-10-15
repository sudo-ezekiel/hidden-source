#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "iclientmode.h"
#include "c_baseplayer.h"
#include "igameevents.h"
#include "igameresources.h"
#include "utlvector.h"

#include <vgui_controls/Panel.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define MAX_RADAR_PLAYERS	9		// REBUILD: name; client.dll keeps nine entries
#define RADAR_SCALE			0.15f	// REBUILD: name; world units to radar pixels
#define RADAR_RANGE			130		// REBUILD: name; radar radius in pixels
#define RADAR_ICON_SIZE		20		// REBUILD: name

// REBUILD: names of the radar's two record types
struct RadarPlayer_t
{
	Vector	m_vecOrigin;
	bool	m_bSupport;		// m_iNewClass == 1 draws the support marine icon
	bool	m_bLocal;		// the local player is not drawn
	int		m_iUserID;
	float	m_flRadioTime;	// highlighted until this time after a radio call
};

struct RadarAlarm_t
{
	Vector	m_vecOrigin;
	float	m_flEndTime;
};

//-----------------------------------------------------------------------------
// Purpose: IRIS radar: team mates, who used the radio and triggered sonic alarms
//-----------------------------------------------------------------------------
class CHudRadar : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudRadar, vgui::Panel );

public:
	CHudRadar( const char *pElementName );
	virtual ~CHudRadar();

	virtual void Reset( void );
	virtual void FireGameEvent( IGameEvent *event );

	// REBUILD: name; refreshes the team mate list ten times a second
	virtual void UpdatePlayers( C_BasePlayer *pLocalPlayer );

protected:
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void OnThink( void );
	virtual void Paint( void );

private:
	int				m_iCenterX;
	int				m_iCenterY;
	int				m_iWide;			// width the center was computed for

	Vector			m_vecOrigin;		// the local player's, as of the last Paint
	QAngle			m_angView;

	RadarPlayer_t	m_Players[MAX_RADAR_PLAYERS];
	CUtlVector< RadarAlarm_t >	m_Alarms;
	int				m_iNumPlayers;
	float			m_flNextUpdate;

	CHudTexture		*m_pAssaultIcon;
	CHudTexture		*m_pSupportIcon;
	CHudTexture		*m_pAssaultRadioIcon;
	CHudTexture		*m_pSupportRadioIcon;
	CHudTexture		*m_pSonicIcon;
};

DECLARE_HUDELEMENT( CHudRadar );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudRadar::CHudRadar( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudRadar" )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	m_iNumPlayers = 0;
	m_iWide = GetWide();
	m_flNextUpdate = gpGlobals->curtime + 0.1;

	SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD | HIDEHUD_NEEDSUIT );

	gameeventmanager->AddListener( this, "alarm_trigger", false );
	gameeventmanager->AddListener( this, "iris_radio", false );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CHudRadar::~CHudRadar()
{
	gameeventmanager->RemoveListener( this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudRadar::Reset( void )
{
	m_flNextUpdate = 0;

	for ( int i = 0; i < MAX_RADAR_PLAYERS; i++ )
	{
		m_Players[i].m_flRadioTime = 0;
		m_Players[i].m_iUserID = 0;
		m_Players[i].m_vecOrigin.Init();
	}

	m_Alarms.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudRadar::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetPaintBackgroundEnabled( false );

	m_pAssaultIcon = gHUD.GetIcon( "AMarineIcon" );
	m_pSupportIcon = gHUD.GetIcon( "SMarineIcon" );
	m_pAssaultRadioIcon = gHUD.GetIcon( "AMarineEIcon" );
	m_pSupportRadioIcon = gHUD.GetIcon( "SMarineEIcon" );
	m_pSonicIcon = gHUD.GetIcon( "SonicIcon" );
}

//-----------------------------------------------------------------------------
// Purpose: Sonic alarms show for five seconds; so does a team mate's radio call
//-----------------------------------------------------------------------------
void CHudRadar::FireGameEvent( IGameEvent *event )
{
	if ( !event )
		return;

	const char *pszEventName = event->GetName();

	if ( !Q_strcmp( pszEventName, "alarm_trigger" ) )
	{
		RadarAlarm_t alarm;
		alarm.m_vecOrigin = Vector( event->GetFloat( "posx" ), event->GetFloat( "posy" ), event->GetFloat( "posz" ) );
		alarm.m_flEndTime = gpGlobals->curtime + 5.0f;

		for ( int i = 0; i < m_Alarms.Count(); i++ )
		{
			if ( m_Alarms[i].m_vecOrigin == alarm.m_vecOrigin )
			{
				m_Alarms[i].m_flEndTime = alarm.m_flEndTime;
				return;
			}
		}

		m_Alarms.AddToTail( alarm );
	}
	else if ( !Q_strcmp( pszEventName, "iris_radio" ) )
	{
		int iUserID = event->GetInt( "userid" );

		for ( int i = 0; i < m_iNumPlayers; i++ )
		{
			if ( m_Players[i].m_iUserID == iUserID )
			{
				m_Players[i].m_flRadioTime = gpGlobals->curtime + 5.0f;
				return;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudRadar::UpdatePlayers( C_BasePlayer *pLocalPlayer )
{
	if ( !pLocalPlayer )
		return;

	IGameResources *gr = GameResources();

	if ( !( m_flNextUpdate < gpGlobals->curtime ) )
		return;

	m_flNextUpdate = gpGlobals->curtime + 0.1f;

	if ( pLocalPlayer->GetTeamNumber() != TEAM_IRIS )
		return;

	m_iNumPlayers = 0;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		if ( !gr || !gr->IsConnected( i ) )
			continue;

		C_BasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( !pPlayer )
			continue;

		if ( gr->GetTeam( i ) == TEAM_HIDDEN || !gr->IsAlive( i ) )
			continue;

		m_Players[m_iNumPlayers].m_vecOrigin = pPlayer->GetLocalOrigin();

		if ( pLocalPlayer == pPlayer )
			m_Players[m_iNumPlayers].m_bLocal = true;
		else
			m_Players[m_iNumPlayers].m_bLocal = false;

		if ( pPlayer->GetNewClass() == 1 )
			m_Players[m_iNumPlayers].m_bSupport = true;
		else
			m_Players[m_iNumPlayers].m_bSupport = false;

		m_Players[m_iNumPlayers].m_iUserID = pPlayer->GetUserID();

		if ( m_Players[m_iNumPlayers].m_flRadioTime < gpGlobals->curtime )
			m_Players[m_iNumPlayers].m_flRadioTime = 0;

		m_iNumPlayers++;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudRadar::OnThink( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudRadar::Paint( void )
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer || pPlayer->GetTeamNumber() != TEAM_IRIS )
		return;

	UpdatePlayers( pPlayer );

	if ( m_iWide != GetWide() )
	{
		float wide = GetWide();
		float tall = GetTall();
		m_iCenterX = wide * 0.5f;
		m_iCenterY = tall * 0.8f;
		m_iWide = GetWide();
	}

	m_vecOrigin = pPlayer->GetLocalOrigin();
	m_angView = pPlayer->GetLocalAngles();

	// Turn the radar with the view, forward pointing up
	float flYaw = ( m_angView[YAW] - 90.0f ) * M_PI / 180.0;
	float flSin = sin( flYaw );
	float flCos = cos( flYaw );

	int i;
	for ( i = 0; i < m_iNumPlayers; i++ )
	{
		if ( m_Players[i].m_bLocal )
			continue;

		Vector vecRadar = m_vecOrigin - m_Players[i].m_vecOrigin;
		vecRadar *= RADAR_SCALE;
		vecRadar.x *= -1;
		if ( vecRadar.Length() < RADAR_RANGE )
		{
			int x = (int)( vecRadar.x * flCos - vecRadar.y * flSin ) - RADAR_ICON_SIZE / 2 + m_iCenterX;
			int y = (int)( vecRadar.y * flCos + vecRadar.x * flSin ) - RADAR_ICON_SIZE / 2 + m_iCenterY;

			Color clr = gHUD.m_clrNormal;

			CHudTexture *pIcon;
			if ( m_Players[i].m_flRadioTime < gpGlobals->curtime )
				pIcon = m_Players[i].m_bSupport ? m_pSupportIcon : m_pAssaultIcon;
			else
				pIcon = m_Players[i].m_bSupport ? m_pSupportRadioIcon : m_pAssaultRadioIcon;

			if ( pIcon )
			{
				pIcon->DrawSelf( x, y, RADAR_ICON_SIZE, RADAR_ICON_SIZE, clr );
			}
		}
	}

	for ( i = 0; i < m_Alarms.Count(); i++ )
	{
		RadarAlarm_t &alarm = m_Alarms[i];

		if ( alarm.m_flEndTime < gpGlobals->curtime )
		{
			m_Alarms.Remove( i );
			continue;
		}

		Vector vecRadar = m_vecOrigin - alarm.m_vecOrigin;
		vecRadar *= RADAR_SCALE;
		vecRadar.x *= -1;
		if ( vecRadar.Length() < RADAR_RANGE && m_pSonicIcon )
		{
			Color clr( 255, 255, 255, 255 );

			int x = vecRadar.x * flCos - vecRadar.y * flSin;
			int y = vecRadar.y * flCos + vecRadar.x * flSin;

			m_pSonicIcon->DrawSelf( x - RADAR_ICON_SIZE / 2 + m_iCenterX, y - RADAR_ICON_SIZE / 2 + m_iCenterY, RADAR_ICON_SIZE, RADAR_ICON_SIZE, clr );
		}
	}
}
