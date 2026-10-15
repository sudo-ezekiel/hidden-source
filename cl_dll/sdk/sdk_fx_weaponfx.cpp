//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Game-specific impact effect hooks
//
//=============================================================================//
#include "cbase.h"
#include "fx_impact.h"
#include "tempent.h"
#include "c_te_effect_dispatch.h"
#include "c_te_legacytempents.h"


//-----------------------------------------------------------------------------
// Purpose: Hidden: empty magazines (REBUILD: names; client.dll's 241e7790 to 241e7810)
//-----------------------------------------------------------------------------
void SDK_EjectClip( int clip, const CEffectData &data )
{
	tempents->EjectClip( data.m_vOrigin, data.m_vAngles, clip );
}

void SDK_FX_EjectPistolClip_Callback( const CEffectData &data )
{
	SDK_EjectClip( HDN_CLIP_PISTOL, data );
}

void SDK_FX_EjectFN2000Clip_Callback( const CEffectData &data )
{
	SDK_EjectClip( HDN_CLIP_FN2000, data );
}

void SDK_FX_EjectFNP90Clip_Callback( const CEffectData &data )
{
	SDK_EjectClip( HDN_CLIP_FNP90, data );
}

void SDK_FX_EjectFN303Clip_Callback( const CEffectData &data )
{
	SDK_EjectClip( HDN_CLIP_FN303, data );
}

//-----------------------------------------------------------------------------
// Purpose: Handle weapon effect callbacks
//-----------------------------------------------------------------------------
void SDK_EjectBrass( int shell, const CEffectData &data )
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();

	tempents->CSEjectBrass( data.m_vOrigin, data.m_vAngles, data.m_fFlags, shell, pPlayer );
}

void SDK_FX_EjectBrass_57_Callback( const CEffectData &data )
{
	SDK_EjectBrass( CS_SHELL_57, data );
}

void SDK_FX_EjectBrass_556_Callback( const CEffectData &data )
{
	SDK_EjectBrass( CS_SHELL_556, data );
}

void SDK_FX_EjectBrass_9mm_Callback( const CEffectData &data )
{
	SDK_EjectBrass( CS_SHELL_9MM, data );
}

void SDK_FX_EjectBrass_12Gauge_Callback( const CEffectData &data )
{
	SDK_EjectBrass( CS_SHELL_12GAUGE, data );
}



DECLARE_CLIENT_EFFECT( "EjectBrass_57", SDK_FX_EjectBrass_57_Callback );
DECLARE_CLIENT_EFFECT( "EjectBrass_556", SDK_FX_EjectBrass_556_Callback );
DECLARE_CLIENT_EFFECT( "EjectBrass_9mm", SDK_FX_EjectBrass_9mm_Callback );
DECLARE_CLIENT_EFFECT( "EjectBrass_12Gauge",SDK_FX_EjectBrass_12Gauge_Callback );
DECLARE_CLIENT_EFFECT( "EjectPistolClip", SDK_FX_EjectPistolClip_Callback );
DECLARE_CLIENT_EFFECT( "EjectFN2000Clip", SDK_FX_EjectFN2000Clip_Callback );
DECLARE_CLIENT_EFFECT( "EjectFNP90Clip", SDK_FX_EjectFNP90Clip_Callback );
DECLARE_CLIENT_EFFECT( "EjectFN303Clip", SDK_FX_EjectFN303Clip_Callback );

