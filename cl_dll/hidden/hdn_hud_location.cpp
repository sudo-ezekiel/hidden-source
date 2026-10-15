#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "iclientmode.h"
#include "clientmode_shared.h"
#include "c_baseplayer.h"
#include "igameevents.h"

#include <vgui/IScheme.h>
#include <vgui_controls/Panel.h>
#include <vgui_controls/Label.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Shows the place name the server sends with "player_location"
//-----------------------------------------------------------------------------
class CHudLocation : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudLocation, vgui::Panel );

public:
	CHudLocation( const char *pElementName );

	virtual void Init( void );
	virtual void VidInit( void );
	virtual void Reset( void );
	virtual void FireGameEvent( IGameEvent *event );

protected:
	virtual void OnThink( void );
	virtual void Paint( void );

private:
	vgui::Label *m_pLabel;
	char m_szLocation[25];
};

DECLARE_HUDELEMENT( CHudLocation );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudLocation::CHudLocation( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudLocation" )
{
	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile( "resource/ClientScheme.res", "ClientScheme" );
	SetScheme( scheme );

	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD | HIDEHUD_NEEDSUIT );

	m_pLabel = SETUP_PANEL( new vgui::Label( this, "LocationLabel", "" ) );
	m_pLabel->SetScheme( scheme );
	m_pLabel->SetFont( vgui::scheme()->GetIScheme( scheme )->GetFont( "Default" ) );
	m_pLabel->SetKeyBoardInputEnabled( false );
	m_pLabel->SetMouseInputEnabled( false );
	m_pLabel->SetPos( 0, 0 );
	m_pLabel->SetSize( GetWide(), GetTall() );
	m_pLabel->SetPaintBackgroundEnabled( false );
	m_pLabel->SetPaintBorderEnabled( false );
	m_pLabel->SetContentAlignment( vgui::Label::a_center );

	SetBgColor( Color( 0, 0, 0, 0 ) );
	m_pLabel->SetBgColor( Color( 0, 0, 0, 0 ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudLocation::Init( void )
{
	Reset();

	gameeventmanager->AddListener( this, "player_location", false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudLocation::VidInit( void )
{
	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudLocation::Reset( void )
{
	m_szLocation[0] = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Keeps the local player's place name
//-----------------------------------------------------------------------------
void CHudLocation::FireGameEvent( IGameEvent *event )
{
	if ( !Q_strcmp( event->GetName(), "player_location" ) )
	{
		C_BasePlayer *pPlayer = USERID2PLAYER( event->GetInt( "userid" ) );

		if ( pPlayer == C_BasePlayer::GetLocalPlayer() )
		{
			Q_strncpy( m_szLocation, event->GetString( "location", "" ), sizeof( m_szLocation ) );
			DevMsg( 1, "location is : %s\n", m_szLocation );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudLocation::OnThink( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudLocation::Paint( void )
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();

	// No place name yet: a living Hidden sees nothing here
	if ( !m_szLocation[0] && pPlayer->GetTeamNumber() == TEAM_HIDDEN && pPlayer->IsAlive() )
	{
		m_pLabel->SetText( (const char *)NULL );
		m_pLabel->SetFgColor( Color( 0, 0, 0, 0 ) );
		m_pLabel->SetVisible( false );
		SetBgColor( Color( 0, 0, 0, 0 ) );
		SetVisible( false );
		m_pLabel->SetBgColor( Color( 0, 0, 0, 0 ) );
		return;
	}

	m_pLabel->SetFgColor( Color( 184, 224, 232, 255 ) );
	SetBgColor( Color( 0, 0, 0, 0 ) );
	m_pLabel->SetText( m_szLocation );
	if ( pPlayer->GetTeamNumber() == TEAM_HIDDEN )
		m_pLabel->SetVisible( false );
	else
		m_pLabel->SetVisible( true );
	m_pLabel->SizeToContents();
	m_pLabel->SetSize( GetWide(), GetTall() );
}
