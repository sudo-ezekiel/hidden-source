#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "iclientmode.h"
#include "c_baseplayer.h"

#include <vgui_controls/Panel.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Draws the Hidden's health panel background
//-----------------------------------------------------------------------------
class CHudHHealth : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudHHealth, vgui::Panel );

public:
	CHudHHealth( const char *pElementName );

protected:
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void Paint( void );

private:
	CHudTexture *m_pIcon;
};

DECLARE_HUDELEMENT( CHudHHealth );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudHHealth::CHudHHealth( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudHHealth" )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD | HIDEHUD_NEEDSUIT );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudHHealth::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_pIcon = gHUD.GetIcon( "HiddenHealth" );

	SetPaintBackgroundEnabled( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudHHealth::Paint( void )
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	if ( pPlayer->GetTeamNumber() == TEAM_HIDDEN )
	{
		Color clr = gHUD.m_clrNormal;
		if ( m_pIcon )
			m_pIcon->DrawSelf( 0, 0, GetWide(), GetTall(), clr );
	}
}
