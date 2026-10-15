#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "iclientmode.h"
#include "c_baseplayer.h"

#include <vgui/ISurface.h>
#include <vgui_controls/Panel.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Draws the Hidden's stamina bar, cropped to the current stamina
//-----------------------------------------------------------------------------
class CHudHStaminabar : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudHStaminabar, vgui::Panel );

public:
	CHudHStaminabar( const char *pElementName );

protected:
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void Paint( void );

private:
	CHudTexture *m_pIcon;
};

DECLARE_HUDELEMENT( CHudHStaminabar );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudHStaminabar::CHudHStaminabar( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudHStaminabar" )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD | HIDEHUD_NEEDSUIT );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudHStaminabar::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_pIcon = gHUD.GetIcon( "HiddenStaminabar" );

	SetPaintBackgroundEnabled( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudHStaminabar::Paint( void )
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	if ( pPlayer->GetTeamNumber() == TEAM_HIDDEN && m_pIcon )
	{
		// stamina runs 0 to 100; show that fraction of the bar texture
		float flStamina = pPlayer->GetStamina() * 0.01f;

		vgui::surface()->DrawSetTexture( m_pIcon->textureId );
		vgui::surface()->DrawSetColor( Color( 255, 255, 255, 255 ) );
		vgui::surface()->DrawTexturedSubRect( 0, 0, (int)( GetWide() * flStamina ), GetTall(), 0, 0, flStamina, 1.0f );
	}
}
