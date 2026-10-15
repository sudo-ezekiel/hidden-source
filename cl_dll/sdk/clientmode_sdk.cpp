//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"
#include "hud.h"
#include "clientmode_sdk.h"
#include "cdll_client_int.h"
#include "iinput.h"
#include "vgui/isurface.h"
#include "vgui/ipanel.h"
#include <vgui_controls/AnimationController.h>
#include "ivmodemanager.h"
#include "BuyMenu.h"
#include "filesystem.h"
#include "vgui/ivgui.h"
#include "keydefs.h"
#include "hud_chat.h"
#include "view_shared.h"
#include "view.h"
#include "ivrenderview.h"
#include "model_types.h"
#include "iefx.h"
#include "dlight.h"
#include <imapoverview.h>
#include "c_playerresource.h"
#include <keyvalues.h>
#include "text_message.h"
#include "panelmetaclassmgr.h"
#include "c_baseanimatingoverlay.h"
#include "hidden/hdn_teammenu.h"


ConVar default_fov( "default_fov", "90", FCVAR_CHEAT );

IClientMode *g_pClientMode = NULL;

// ----------------------------------------------------------------------------- //
// The class image: the marine drawn into the team and weapon menus' class image
// panel (CSClassImagePanel in the .res files), as Counter-Strike: Source draws
// it. See UpdateClassImageEntity and ClientModeSDKNormal::PostRenderVGui below.
// ----------------------------------------------------------------------------- //

// The current class image entities. Client-only, never drawn by the renderer itself.
CHandle<C_BaseAnimatingOverlay> g_ClassImagePlayer;	// REBUILD: name; the player
CHandle<C_BaseAnimating> g_ClassImageWeapon;		// REBUILD: name; the weapon it holds

// REBUILD: name
bool WillPanelBeVisible( vgui::VPANEL hPanel )
{
	while ( hPanel )
	{
		if ( !vgui::ipanel()->IsVisible( hPanel ) )
			return false;

		hPanel = vgui::ipanel()->GetParent( hPanel );
	}
	return true;
}

// REBUILD: name
bool ShouldRecreateClassImageEntity( C_BaseAnimating *pEnt, const char *pNewModelName )
{
	if ( !pNewModelName || !pNewModelName[0] )
		return false;
	if ( !pEnt )
		return true;

	const model_t *pModel = pEnt->GetModel();

	if ( !pModel )
		return true;
	const char *pName = modelinfo->GetModelName( pModel );
	if ( !pName )
		return true;

	// reload only if names are different
	return( Q_stricmp( pName, pNewModelName ) != 0 );
}


// --------------------------------------------------------------------------------- //
// CSDKModeManager.
// --------------------------------------------------------------------------------- //

class CSDKModeManager : public IVModeManager
{
public:
	virtual void	Init();
	virtual void	SwitchMode( bool commander, bool force ) {}
	virtual void	LevelInit( const char *newmap );
	virtual void	LevelShutdown( void );
	virtual void	ActivateMouse( bool isactive ) {}
};

static CSDKModeManager g_ModeManager;
IVModeManager *modemanager = ( IVModeManager * )&g_ModeManager;

// --------------------------------------------------------------------------------- //
// CSDKModeManager implementation.
// --------------------------------------------------------------------------------- //

#define SCREEN_FILE		"scripts/vgui_screens.txt"

void CSDKModeManager::Init()
{
	g_pClientMode = GetClientModeNormal();
	
	PanelMetaClassMgr()->LoadMetaClassDefinitionFile( SCREEN_FILE );
}

void CSDKModeManager::LevelInit( const char *newmap )
{
	g_pClientMode->LevelInit( newmap );
}

void CSDKModeManager::LevelShutdown( void )
{
	g_pClientMode->LevelShutdown();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
ClientModeSDKNormal::ClientModeSDKNormal()
{
}

//-----------------------------------------------------------------------------
// Purpose: If you don't know what a destructor is by now, you are probably going to get fired
//-----------------------------------------------------------------------------
ClientModeSDKNormal::~ClientModeSDKNormal()
{
}


void ClientModeSDKNormal::InitViewport()
{
	m_pViewport = new SDKViewport();
	m_pViewport->Start( gameuifuncs, gameeventmanager );
}

ClientModeSDKNormal g_ClientModeNormal;

IClientMode *GetClientModeNormal()
{
	return &g_ClientModeNormal;
}


ClientModeSDKNormal* GetClientModeSDKNormal()
{
	Assert( dynamic_cast< ClientModeSDKNormal* >( GetClientModeNormal() ) );

	return static_cast< ClientModeSDKNormal* >( GetClientModeNormal() );
}

float ClientModeSDKNormal::GetViewModelFOV( void )
{
	return 74.0f;
}

int ClientModeSDKNormal::GetDeathMessageStartHeight( void )
{
	return m_pViewport->GetDeathMessageStartHeight();
}


//-----------------------------------------------------------------------------
// Purpose: Draws pModelName holding an F2000 into the screen rectangle x, y,
//			width, height, lit by a dynamic light, with a view of its own
//-----------------------------------------------------------------------------
// REBUILD: name
void UpdateClassImageEntity( 
		const char *pModelName,
		int x, int y, int width, int height )
{
	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	
	if ( !pLocalPlayer )
		return;

	const char *pWeaponName = "models/weapons/f2000/w_f2000.mdl";

	C_BaseAnimatingOverlay *pPlayerModel = g_ClassImagePlayer.Get();

	// Does the entity even exist yet?
	bool recreatePlayer = ShouldRecreateClassImageEntity( pPlayerModel, pModelName );
	if ( recreatePlayer )
	{
		if ( pPlayerModel )
			pPlayerModel->Remove();

		pPlayerModel = new C_BaseAnimatingOverlay;
		pPlayerModel->InitializeAsClientEntity( pModelName, RENDER_GROUP_OPAQUE_ENTITY );
		pPlayerModel->AddEffects( EF_NODRAW ); // don't let the renderer draw the model normally

		pPlayerModel->SetSequence( pPlayerModel->LookupSequence( "Idle_lower" ) );
		pPlayerModel->SetPoseParameter( 0, 0.0f ); // move_yaw
		pPlayerModel->SetPoseParameter( 1, 10.0f ); // body_pitch, look down a bit
		pPlayerModel->SetPoseParameter( 2, 0.0f ); // body_yaw
		pPlayerModel->SetPoseParameter( 3, 0.0f ); // move_y
		pPlayerModel->SetPoseParameter( 4, 0.0f ); // move_x

		g_ClassImagePlayer = pPlayerModel;
	}

	C_BaseAnimating *pWeaponModel = g_ClassImageWeapon.Get();

	// Does the entity even exist yet?
	if ( recreatePlayer || ShouldRecreateClassImageEntity( pWeaponModel, pWeaponName ) )
	{
		if ( pWeaponModel )
			pWeaponModel->Remove();

		pWeaponModel = new C_BaseAnimating;
		pWeaponModel->InitializeAsClientEntity( pWeaponName, RENDER_GROUP_OPAQUE_ENTITY );
		pWeaponModel->AddEffects( EF_NODRAW ); // don't let the renderer draw the model normally
		pWeaponModel->FollowEntity( pPlayerModel ); // attach to player model
		g_ClassImageWeapon = pWeaponModel;
	}

	Vector origin = pLocalPlayer->EyePosition();
	Vector lightOrigin = origin;

	// find a spot inside the world for the dlight's origin, or it won't illuminate the model
	Vector testPos( origin.x - 100, origin.y, origin.z + 100 );
	trace_t tr;
	UTIL_TraceLine( origin, testPos, MASK_OPAQUE, pLocalPlayer, COLLISION_GROUP_NONE, &tr );
	if ( tr.fraction == 1.0f )
	{
		lightOrigin = tr.endpos;
	}
	else
	{
		// Now move the model away so we get the correct illumination
		lightOrigin = tr.endpos + Vector( 1, 0, -1 );	// pull out from the solid
		Vector start = lightOrigin;
		Vector end = lightOrigin + Vector( 100, 0, -100 );
		UTIL_TraceLine( start, end, MASK_OPAQUE, pLocalPlayer, COLLISION_GROUP_NONE, &tr );
		origin = tr.endpos;
	}

	float ambient = engine->GetLightForPoint( origin, true ).Length();

	// Make a light so the model is well lit.
	// use a non-zero number so we cannibalize ourselves next frame
	dlight_t *dl = effects->CL_AllocDlight( LIGHT_INDEX_TE_DYNAMIC+1 );

	dl->flags = DLIGHT_NO_WORLD_ILLUMINATION;
	dl->origin = lightOrigin;
	// Go away immediately so it doesn't light the world 
	dl->die = gpGlobals->curtime + 0.1f;

	dl->color.r = dl->color.g = dl->color.b = 250;
	if ( ambient < 1.0f )
	{
		dl->color.exponent = 1 + (1 - ambient) * 2;
	}
	dl->radius = 400;

	// move player model in front of our view
	pPlayerModel->SetAbsOrigin( origin );
	pPlayerModel->SetAbsAngles( QAngle( 0, 210, 0 ) );

	// wacky hacky, set upper body animation
	pPlayerModel->m_SequenceTransitioner.Update( 
		pPlayerModel->GetModelPtr(),
		pPlayerModel->LookupSequence( "walk_lower" ),
		pPlayerModel->GetCycle(),
		pPlayerModel->GetPlaybackRate(),
		gpGlobals->realtime,
		false,
		true
		);

	// Now, blend the lower and upper (aim) anims together
	pPlayerModel->SetNumAnimOverlays( 2 );
	int numOverlays = pPlayerModel->GetNumAnimOverlays();
	for ( int i=0; i < numOverlays; ++i )
	{
		C_AnimationLayer *layer = pPlayerModel->GetAnimOverlay( i );

		layer->flCycle = pPlayerModel->GetCycle();
		if ( i )
			layer->nSequence = pPlayerModel->LookupSequence( "Idle_Upper_Aug" );
		else
			layer->nSequence = pPlayerModel->LookupSequence( "walk_lower" );

		layer->flPlaybackrate = 1.0;
		layer->flWeight = 1.0f;
		layer->SetOrder( i );
	}

	pPlayerModel->FrameAdvance( gpGlobals->frametime );

	// Now draw it.
	CViewSetup view;
	view.x = x;
	view.y = y;
	view.width = width;
	view.height = height;

	view.m_bOrtho = false;
	view.fov = 54;

	view.origin = origin - Vector( 110, 5, 5 );

	Vector vMins, vMaxs;
	pPlayerModel->C_BaseAnimating::GetRenderBounds( vMins, vMaxs );
	view.origin.z += ( vMins.z + vMaxs.z ) * 0.55f;

	view.angles.Init();
	view.m_vUnreflectedOrigin = view.origin;
	view.zNear = VIEW_NEARZ;
	view.zFar = 1000;
	view.m_bForceAspectRatio1To1 = false;

	Frustum dummyFrustum;
	render->ViewSetup3D( &view, dummyFrustum );

	pPlayerModel->DrawModel( STUDIO_RENDER );

	if ( pWeaponModel )
	{
		pWeaponModel->DrawModel( STUDIO_RENDER );
	}
}

void ClientModeSDKNormal::PostRenderVGui()
{
	// If the team menu is up, then we will render the model of the character that is currently selected.
	for ( int i=0; i < g_ClassImagePanels.Count(); i++ )
	{
		CCSClassImagePanel *pPanel = g_ClassImagePanels[i];
		if ( WillPanelBeVisible( pPanel->GetVPanel() ) )
		{
			// Ok, we have a visible class image panel.
			int x, y, w, h;
			pPanel->GetBounds( x, y, w, h );
			pPanel->LocalToScreen( x, y );

			// Allow for the border.
			x += 3;
			y += 5;
			w -= 2;
			h -= 10;

			UpdateClassImageEntity( g_ClassImagePanels[i]->m_ModelName, x, y, w, h );
			return;
		}
	}
}




