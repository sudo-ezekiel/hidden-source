#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "c_baseplayer.h"
#include "igameevents.h"
#include "filesystem.h"

#include <vgui_controls/Panel.h>
#include <vgui_controls/Controls.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Invisible element that answers the server's "material_check" event:
//			a client whose materials or player model differ from the server's
//			asks to be dropped with the materialCRC command
//-----------------------------------------------------------------------------
class CHudMaterial : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudMaterial, vgui::Panel );

public:
	CHudMaterial( const char *pElementName );

	virtual void Init( void );
	virtual void VidInit( void );
	virtual bool ShouldDraw( void );
	virtual void FireGameEvent( IGameEvent *event );

protected:
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void Paint( void );
};

DECLARE_HUDELEMENT( CHudMaterial );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudMaterial::CHudMaterial( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudMaterial" )
{
	SetHiddenBits( HIDEHUD_MISCSTATUS );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudMaterial::Init( void )
{
	gameeventmanager->AddListener( this, "material_check", false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudMaterial::VidInit( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHudMaterial::ShouldDraw( void )
{
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudMaterial::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetPaintBackgroundEnabled( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudMaterial::Paint( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Compares our files with the CRCs the server sent
//-----------------------------------------------------------------------------
void CHudMaterial::FireGameEvent( IGameEvent *event )
{
	if ( Q_strcmp( event->GetName(), "material_check" ) )
		return;

	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	CRC32_t vmtCRC = pPlayer->GetMaterialCRC( "materials/models/manor/mn_tapestry.vmt" );
	CRC32_t bumpCRC = pPlayer->GetMaterialCRC( "materials/models/manor/mn_tapestry_dx7.vmt" );

	if ( (CRC32_t)event->GetInt( "vmt_CRC" ) != vmtCRC || (CRC32_t)event->GetInt( "bump_CRC" ) != bumpCRC )
	{
		Msg( "material mismatch, we reckon you're cheating!\n" );
		engine->ClientCmd( "materialCRC\n" );
	}

	// A living Hidden's model must carry both of its attachments
	if ( pPlayer->GetTeamNumber() == TEAM_HIDDEN && pPlayer->IsAlive() &&
		 ( !pPlayer->LookupAttachment( "geoff" ) || !pPlayer->LookupAttachment( "jim" ) ) )
	{
		Msg( "bad player model\n" );
		engine->ClientCmd( "materialCRC\n" );
	}

	if ( !vgui::filesystem()->FileExists( "materials/models/manor/mn_tapestry.vtf" ) )
	{
		Msg( "missing player materials\n" );
		engine->ClientCmd( "materialCRC\n" );
	}
}
