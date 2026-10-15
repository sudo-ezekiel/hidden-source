#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "iclientmode.h"
#include "c_baseplayer.h"

#include <vgui_controls/Panel.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Draws the Hidden's round timer panel background
//-----------------------------------------------------------------------------
class CHudHTimer : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudHTimer, vgui::Panel );

public:
	CHudHTimer( const char *pElementName );

protected:
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void Paint( void );

private:
	CHudTexture *m_pIcon;
};

DECLARE_HUDELEMENT( CHudHTimer );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudHTimer::CHudHTimer( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudHTimer" )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD | HIDEHUD_NEEDSUIT );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudHTimer::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_pIcon = gHUD.GetIcon( "HiddenTimer" );

	SetPaintBackgroundEnabled( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudHTimer::Paint( void )
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	if ( pPlayer->GetTeamNumber() == TEAM_HIDDEN && m_pIcon )
	{
		Color clr = gHUD.m_clrNormal;
		m_pIcon->DrawSelf( 0, 0, GetWide(), GetTall(), clr );
	}
}
