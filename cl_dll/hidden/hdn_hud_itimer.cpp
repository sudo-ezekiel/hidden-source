#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "iclientmode.h"
#include "c_baseplayer.h"

#include <vgui_controls/Panel.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Draws the IRIS round timer panel background
//-----------------------------------------------------------------------------
class CHudITimer : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudITimer, vgui::Panel );

public:
	CHudITimer( const char *pElementName );

protected:
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void Paint( void );

private:
	CHudTexture *m_pIcon;
};

DECLARE_HUDELEMENT( CHudITimer );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudITimer::CHudITimer( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudITimer" )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD | HIDEHUD_NEEDSUIT );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudITimer::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_pIcon = gHUD.GetIcon( "IrisTimer" );

	SetPaintBackgroundEnabled( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudITimer::Paint( void )
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	if ( pPlayer->GetTeamNumber() == TEAM_IRIS )
	{
		Color clr = gHUD.m_clrNormal;
		if ( m_pIcon )
			m_pIcon->DrawSelf( 0, 0, GetWide(), GetTall(), clr );
	}
}
