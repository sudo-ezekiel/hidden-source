#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "iclientmode.h"
#include "c_baseplayer.h"

#include <vgui/IScheme.h>
#include <vgui_controls/Panel.h>
#include <vgui_controls/Label.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Names the player under the crosshair, with his health
//-----------------------------------------------------------------------------
class CHudName : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudName, vgui::Panel );

public:
	CHudName( const char *pElementName );

	virtual void Init( void );
	virtual void VidInit( void );
	virtual void Reset( void );

protected:
	virtual void OnThink( void );
	virtual void Paint( void );

private:
	vgui::Label *m_pLabel;
	const char *m_pszName;
	C_BasePlayer *m_pPlayer;
};

DECLARE_HUDELEMENT( CHudName );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudName::CHudName( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudName" )
{
	vgui::HScheme scheme = vgui::scheme()->LoadSchemeFromFile( "resource/ClientScheme.res", "ClientScheme" );
	SetScheme( scheme );

	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD | HIDEHUD_NEEDSUIT );

	m_pLabel = SETUP_PANEL( new vgui::Label( this, "NameLabel", "" ) );
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
void CHudName::Init( void )
{
	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudName::VidInit( void )
{
	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudName::Reset( void )
{
	m_pPlayer = NULL;
	m_pszName = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Finds the player the local player is looking at
//-----------------------------------------------------------------------------
void CHudName::OnThink( void )
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	Vector forward;
	AngleVectors( pPlayer->EyeAngles(), &forward );

	// The Hidden only reads names up close
	trace_t tr;
	if ( pPlayer->GetTeamNumber() == TEAM_HIDDEN )
		UTIL_TraceLine( pPlayer->Weapon_ShootPosition(), pPlayer->Weapon_ShootPosition() + forward * 256, MASK_SHOT, NULL, &tr );
	else
		UTIL_TraceLine( pPlayer->Weapon_ShootPosition(), pPlayer->Weapon_ShootPosition() + forward * 8192, MASK_SHOT, NULL, &tr );

	if ( tr.m_pEnt && tr.m_pEnt->IsPlayer() && tr.m_pEnt != pPlayer )
	{
		m_pPlayer = (C_BasePlayer *)tr.m_pEnt;
		m_pszName = m_pPlayer->GetPlayerName();
		return;
	}

	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudName::Paint( void )
{
	if ( !m_pszName || !m_pszName[0] || !m_pPlayer )
	{
		m_pLabel->SetText( (const char *)NULL );
		m_pLabel->SetFgColor( Color( 0, 0, 0, 0 ) );
		m_pLabel->SetVisible( false );
		SetBgColor( Color( 0, 0, 0, 0 ) );
		SetVisible( false );
		m_pLabel->SetBgColor( Color( 0, 0, 0, 0 ) );
		return;
	}

	char szText[1024];

	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	if ( pLocalPlayer && pLocalPlayer->GetTeamNumber() == TEAM_HIDDEN )
	{
		sprintf( szText, "Enemy: %s - %i", m_pszName, m_pPlayer->GetHealth() );
	}
	else if ( m_pPlayer->GetTeamNumber() == TEAM_HIDDEN )
	{
		sprintf( szText, "" );
	}
	else
	{
		sprintf( szText, "Friend: %s - %i", m_pszName, m_pPlayer->GetHealth() );
	}

	m_pLabel->SetFgColor( Color( 227, 189, 0, 255 ) );
	m_pLabel->SetText( szText );
	m_pLabel->SetVisible( true );
	m_pLabel->SizeToContents();
	m_pLabel->SetSize( GetWide(), GetTall() );
}
