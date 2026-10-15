#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "iclientmode.h"
#include "c_baseplayer.h"

#include <vgui_controls/Panel.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Draws the Hidden's stamina panel background
//-----------------------------------------------------------------------------
class CHudHStamina : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudHStamina, vgui::Panel );

public:
	CHudHStamina( const char *pElementName );

protected:
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void Paint( void );

private:
	CHudTexture *m_pIcon;
};

DECLARE_HUDELEMENT( CHudHStamina );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudHStamina::CHudHStamina( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudHStamina" )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD | HIDEHUD_NEEDSUIT );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudHStamina::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_pIcon = gHUD.GetIcon( "HiddenStaminaBG" );

	SetPaintBackgroundEnabled( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudHStamina::Paint( void )
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
