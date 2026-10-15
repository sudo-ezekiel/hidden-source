#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "iclientmode.h"
#include "c_baseplayer.h"

#include <vgui_controls/Panel.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Draws the IRIS health panel background
//-----------------------------------------------------------------------------
class CHudIHealth : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudIHealth, vgui::Panel );

public:
	CHudIHealth( const char *pElementName );

protected:
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void Paint( void );

private:
	CHudTexture *m_pIcon;
};

DECLARE_HUDELEMENT( CHudIHealth );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudIHealth::CHudIHealth( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudIHealth" )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD | HIDEHUD_NEEDSUIT );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudIHealth::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	// REBUILD: client.dll does not chain to BaseClass::ApplySchemeSettings here
	m_pIcon = gHUD.GetIcon( "IrisHealth" );

	SetPaintBackgroundEnabled( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudIHealth::Paint( void )
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
