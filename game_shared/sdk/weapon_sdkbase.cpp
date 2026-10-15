//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "in_buttons.h"
#include "takedamageinfo.h"
#include "weapon_sdkbase.h"
#include "ammodef.h"
#include "hidden/hdn_lasersight.h"


#if defined( CLIENT_DLL )

	#include "c_sdk_player.h"
	#include "iviewrender_beams.h"
	#include "beamdraw.h"

#else

	#include "sdk_player.h"

#endif


// ----------------------------------------------------------------------------- //
// Global functions.
// ----------------------------------------------------------------------------- //

//--------------------------------------------------------------------------------------------------------
static const char * s_WeaponAliasInfo[] = 
{
	"none",		// WEAPON_NONE
	"crowbar",	// WEAPON_CROWBAR
	"pistol",	// WEAPON_PISTOL
	"pistol2",	// WEAPON_PISTOL2
	"p90",		// WEAPON_P90
	"fn2000",	// WEAPON_FN2000
	"fn303",	// WEAPON_FN303
	"shotgun",	// WEAPON_SHOTGUN
	"grenade",	// WEAPON_GRENADE
	NULL,		// WEAPON_NONE
};

//--------------------------------------------------------------------------------------------------------
//
// Given an alias, return the associated weapon ID
//
int AliasToWeaponID( const char *alias )
{
	if (alias)
	{
		for( int i=0; s_WeaponAliasInfo[i] != NULL; ++i )
			if (!Q_stricmp( s_WeaponAliasInfo[i], alias ))
				return i;
	}

	return WEAPON_NONE;
}

//--------------------------------------------------------------------------------------------------------
//
// Given a weapon ID, return its alias
//
const char *WeaponIDToAlias( int id )
{
	if ( (id >= WEAPON_MAX) || (id < 0) )
		return NULL;

	return s_WeaponAliasInfo[id];
}

// ----------------------------------------------------------------------------- //
// CWeaponSDKBase tables.
// ----------------------------------------------------------------------------- //

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponSDKBase, DT_WeaponSDKBase )

BEGIN_NETWORK_TABLE( CWeaponSDKBase, DT_WeaponSDKBase )
#ifdef CLIENT_DLL
  
#else
	// world weapon models have no animations
  	SendPropExclude( "DT_AnimTimeMustBeFirst", "m_flAnimTime" ),
	SendPropExclude( "DT_BaseAnimating", "m_nSequence" ),
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponSDKBase )
	DEFINE_PRED_FIELD( m_flTimeWeaponIdle, FIELD_FLOAT, FTYPEDESC_OVERRIDE | FTYPEDESC_NOERRORCHECK ),
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_sdk_base, CWeaponSDKBase );


#ifdef GAME_DLL

	BEGIN_DATADESC( CWeaponSDKBase )

		// New weapon Think and Touch Functions go here..

	END_DATADESC()

#endif

// ----------------------------------------------------------------------------- //
// CWeaponCSBase implementation. 
// ----------------------------------------------------------------------------- //

#ifdef CLIENT_DLL
inline void CWeaponSDKBase::KillBeam( void )
{
	if ( m_pBeam )
	{
		m_pBeam->brightness = 0.0f;
		m_pBeam->flags &= ~FBEAM_FOREVER;
		m_pBeam->die = gpGlobals->curtime - 0.1;
		m_pBeam = NULL;
	}
}
#endif

CWeaponSDKBase::CWeaponSDKBase()
{
	SetPredictionEligible( true );

	AddSolidFlags( FSOLID_TRIGGER ); // Nothing collides with these but it gets touches.
}

const CSDKWeaponInfo &CWeaponSDKBase::GetSDKWpnData() const
{
	const FileWeaponInfo_t *pWeaponInfo = &GetWpnData();
	const CSDKWeaponInfo *pSDKInfo;

	#ifdef _DEBUG
		pSDKInfo = dynamic_cast< const CSDKWeaponInfo* >( pWeaponInfo );
		Assert( pSDKInfo );
	#else
		pSDKInfo = static_cast< const CSDKWeaponInfo* >( pWeaponInfo );
	#endif

	return *pSDKInfo;
}

bool CWeaponSDKBase::PlayEmptySound()
{
	CPASAttenuationFilter filter( this );
	filter.UsePredictionRules();

	EmitSound( filter, entindex(), "Default.ClipEmpty_Rifle" );
	
	return 0;
}

CSDKPlayer* CWeaponSDKBase::GetPlayerOwner() const
{
	return dynamic_cast< CSDKPlayer* >( GetOwner() );
}

bool CWeaponSDKBase::Deploy()
{
#ifndef CLIENT_DLL
	if ( m_bDeployed )
		return true;

	m_bDeployed = true;

	CSDKPlayer *pPlayer = GetPlayerOwner();

	// REBUILD: equipment 0 is the laser aiming module
	if ( !pPlayer->GetSafe() && pPlayer->GetEquipment() == 0 && pPlayer->LAMIsOn() )
		CreateLaserPointer();
#endif

	m_bJustDeployed = true;

	return BaseClass::Deploy();
}

bool CWeaponSDKBase::ShouldRemoveOnRoundRestart() const
{
	if ( GetPlayerOwner() )
		return true;

	return false;
}

#ifdef GAME_DLL

void CWeaponSDKBase::SendReloadEvents()
{
	CSDKPlayer *pPlayer = dynamic_cast< CSDKPlayer* >( GetOwner() );
	if ( !pPlayer )
		return;

	// Send a message to any clients that have this entity to play the reload.
	CPASFilter filter( pPlayer->GetAbsOrigin() );
	filter.RemoveRecipient( pPlayer );

	// Make the player play his reload animation.
	pPlayer->DoAnimationEvent( PLAYERANIMEVENT_RELOAD );
}

#endif

//-----------------------------------------------------------------------------
// Purpose: Move the laser dot to where the weapon is pointing
//-----------------------------------------------------------------------------
void CWeaponSDKBase::UpdateLaserPosition( Vector vecMuzzlePos, Vector vecEndPos )
{
#ifndef CLIENT_DLL
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( !pPlayer )
		return;

	if ( !pPlayer->LAMIsOn() )
		return;

	if ( vecMuzzlePos == vec3_origin || vecEndPos == vec3_origin )
	{
		vecMuzzlePos = pPlayer->Weapon_ShootPosition();

		Vector	forward;
		pPlayer->EyeVectors( &forward );
		vecEndPos = vecMuzzlePos + ( forward * MAX_TRACE_LENGTH );
	}

	// Trace out for the endpoint
	trace_t	tr;
	UTIL_TraceLine( vecMuzzlePos, vecEndPos, (MASK_SHOT & ~CONTENTS_WINDOW), GetOwner(), COLLISION_GROUP_NONE, &tr );

	// Move the laser sprite
	if ( m_hLaserDot != NULL )
	{
		Vector	laserPos = tr.endpos;
		m_hLaserDot->SetLaserPosition( laserPos, tr.plane.normal );
		
		if ( tr.DidHitNonWorldEntity() )
		{
			CBaseEntity *pHit = tr.m_pEnt;

			if ( ( ( pHit != NULL ) && ( pHit->m_takedamage ) ) || pHit->GetTeamNumber() == TEAM_HIDDEN )
			{
				m_hLaserDot->SetTargetEntity( pHit );
			}
			else
			{
				m_hLaserDot->SetTargetEntity( NULL );
			}
		}
		else
		{
			m_hLaserDot->SetTargetEntity( NULL );
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Create the laser dot if there is none yet
//-----------------------------------------------------------------------------
void CWeaponSDKBase::CreateLaserPointer( void )
{
#ifndef CLIENT_DLL
	if ( !m_hLaserDot && GetOwner() != NULL )
	{
		m_hLaserDot = CLaserDot::Create( GetAbsOrigin(), GetOwner() );

		UpdateLaserPosition();
	}
#endif
}

void CWeaponSDKBase::ItemPostFrame( void )
{
	BaseClass::ItemPostFrame();

	if ( m_bInReload )
		return;

	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( !pPlayer )
		return;

#ifndef CLIENT_DLL
	if ( !pPlayer->IsOnLadder() && pPlayer->GetTeamNumber() == TEAM_IRIS )
		Deploy();
#endif

	if ( pPlayer->GetSafe() || pPlayer->GetTeamNumber() == TEAM_HIDDEN )
		return;

#ifdef CLIENT_DLL
	// The beam is created dark: light it once the laser aiming module is on
	if ( pPlayer->LAMIsOn() && m_pBeam != NULL && m_pBeam->brightness == 0.0f )
	{
		DevMsg( 1, "Setting beam in ItemPostFrame\n" );
		m_pBeam->brightness = 255.0f;
	}
#else
	UpdateLaserPosition();

	if ( pPlayer->LAMIsOn() && m_hLaserDot == NULL )
	{
		DevMsg( 1, "Creating laser pointer in ItemPostFrame\n" );
		CreateLaserPointer();
	}

	if ( !pPlayer->LAMIsOn() && m_hLaserDot )
	{
		m_hLaserDot->TurnOff();
		UTIL_Remove( m_hLaserDot );
		m_hLaserDot = NULL;
	}
#endif
}

bool CWeaponSDKBase::Holster( CBaseCombatWeapon *pSwitchingTo )
{
#ifdef CLIENT_DLL
	KillBeam();
#else
	if ( m_hLaserDot != NULL )
	{
		m_hLaserDot->TurnOff();
		UTIL_Remove( m_hLaserDot );
		m_hLaserDot = NULL;
	}
#endif

	m_bDeployed = false;

	return BaseClass::Holster( pSwitchingTo );
}

void CWeaponSDKBase::SetSafe( void )
{
	SendWeaponAnim( ACT_VM_IDLE_TO_LOWERED );
	SetWeaponIdleTime( gpGlobals->curtime + 5.0f );

#ifdef CLIENT_DLL
	KillBeam();
#else
	if ( m_hLaserDot != NULL )
	{
		m_hLaserDot->TurnOff();
		UTIL_Remove( m_hLaserDot );
		m_hLaserDot = NULL;
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Same as CBaseCombatWeapon::Drop
//-----------------------------------------------------------------------------
void CWeaponSDKBase::Drop( const Vector &vecVelocity )
{
#if !defined( CLIENT_DLL )

	// Once somebody drops a gun, it's fair game for removal when/if
	// a game_weapon_manager does a cleanup on surplus weapons in the
	// world.
	SetRemoveable( true );

	//If it was dropped then there's no need to respawn it.
	AddSpawnFlags( SF_NORESPAWN );

	StopAnimation();
	StopFollowingEntity( );
	SetMoveType( MOVETYPE_FLYGRAVITY );
	// clear follow stuff, setup for collision
	SetGravity(1.0);
	m_iState = WEAPON_NOT_CARRIED;
	RemoveEffects( EF_NODRAW );
	FallInit();
	SetGroundEntity( NULL );
	SetThink( &CBaseCombatWeapon::SetPickupTouch );
	SetTouch(NULL);

	IPhysicsObject *pObj = VPhysicsGetObject();
	if ( pObj != NULL )
	{
		AngularImpulse	angImp( 200, 200, 200 );
		pObj->AddVelocity( &vecVelocity, &angImp );
	}
	else
	{
		SetAbsVelocity( vecVelocity );
	}

	SetNextThink( gpGlobals->curtime + 1.0f );
	SetOwnerEntity( NULL );
	SetOwner( NULL );
#else
	KillBeam();
#endif
}

float	g_lateralBob;
float	g_verticalBob;

#if defined( CLIENT_DLL )

#define	LASER_BEAM_LENGTH	80	// REBUILD: name

// Valve's HL2 view model bob variables, with Hidden's default for cl_bob
static ConVar	cl_bobcycle( "cl_bobcycle","0.8" );
static ConVar	cl_bob( "cl_bob","0.02" );
static ConVar	cl_bobup( "cl_bobup","0.5" );

// Register these cvars if needed for easy tweaking
static ConVar	v_iyaw_cycle( "v_iyaw_cycle", "2"/*, FCVAR_UNREGISTERED*/ );
static ConVar	v_iroll_cycle( "v_iroll_cycle", "0.5"/*, FCVAR_UNREGISTERED*/ );
static ConVar	v_ipitch_cycle( "v_ipitch_cycle", "1"/*, FCVAR_UNREGISTERED*/ );
static ConVar	v_iyaw_level( "v_iyaw_level", "0.3"/*, FCVAR_UNREGISTERED*/ );
static ConVar	v_iroll_level( "v_iroll_level", "0.1"/*, FCVAR_UNREGISTERED*/ );
static ConVar	v_ipitch_level( "v_ipitch_level", "0.3"/*, FCVAR_UNREGISTERED*/ );

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float CWeaponSDKBase::CalcViewmodelBob( void )
{
	static	float bobtime;
	static	float lastbobtime;
	float	cycle;
	
	CBasePlayer *player = ToBasePlayer( GetOwner() );
	//Assert( player );

	//NOTENOTE: For now, let this cycle continue when in the air, because it snaps badly without it

	if ( ( !gpGlobals->frametime ) || ( player == NULL ) )
	{
		//NOTENOTE: We don't use this return value in our case (need to restructure the calculation function setup!)
		return 0.0f;// just use old value
	}

	//Find the speed of the player
	float speed = player->GetLocalVelocity().Length2D();

	//FIXME: This maximum speed value must come from the server.
	//		 MaxSpeed() is not sufficient for dealing with sprinting - jdw

	speed = clamp( speed, -320, 320 );

	float bob_offset = RemapVal( speed, 0, 320, 0.0f, 1.0f );
	
	bobtime += ( gpGlobals->curtime - lastbobtime ) * bob_offset;
	lastbobtime = gpGlobals->curtime;

	//Calculate the vertical bob
	cycle = bobtime - (int)(bobtime/cl_bobcycle.GetFloat())*cl_bobcycle.GetFloat();
	cycle /= cl_bobcycle.GetFloat();

	if ( cycle < cl_bobup.GetFloat() )
	{
		cycle = M_PI * cycle / cl_bobup.GetFloat();
	}
	else
	{
		cycle = M_PI + M_PI*(cycle-cl_bobup.GetFloat())/(1.0 - cl_bobup.GetFloat());
	}
	
	g_verticalBob = speed*cl_bob.GetFloat();
	g_verticalBob = g_verticalBob*0.3 + g_verticalBob*0.7*sin(cycle);

	g_verticalBob = clamp( g_verticalBob, -7.0f, 4.0f );

	//Calculate the lateral bob
	cycle = bobtime - (int)(bobtime/cl_bobcycle.GetFloat()*2)*cl_bobcycle.GetFloat()*2;
	cycle /= cl_bobcycle.GetFloat()*2;

	if ( cycle < cl_bobup.GetFloat() )
	{
		cycle = M_PI * cycle / cl_bobup.GetFloat();
	}
	else
	{
		cycle = M_PI + M_PI*(cycle-cl_bobup.GetFloat())/(1.0 - cl_bobup.GetFloat());
	}

	g_lateralBob = speed*0.005f;
	g_lateralBob = g_lateralBob*0.3 + g_lateralBob*0.7*sin(cycle);
	g_lateralBob = clamp( g_lateralBob, -7.0f, 4.0f );
	
	//NOTENOTE: We don't use this return value in our case (need to restructure the calculation function setup!)
	return 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &origin - 
//			&angles - 
//			viewmodelindex - 
//-----------------------------------------------------------------------------
void CWeaponSDKBase::AddViewmodelBob( CBaseViewModel *viewmodel, Vector &origin, QAngle &angles )
{
	Vector	forward, right;
	AngleVectors( angles, &forward, &right, NULL );

	CalcViewmodelBob();

	// Apply bob, but scaled down to 40%
	VectorMA( origin, g_verticalBob * 0.1f, forward, origin );
	
	// Z bob a bit more
	origin[2] += g_verticalBob * 0.1f;
	
	// bob the angles
	angles[ ROLL ]	+= g_verticalBob * 0.5f;
	angles[ PITCH ]	-= g_verticalBob * 0.4f;

	angles[ YAW ]	-= g_lateralBob  * 0.3f;

	VectorMA( origin, g_lateralBob * 0.8f, right, origin );
}

extern void FormatViewModelAttachment( Vector &vOrigin, bool bInverse );

//-----------------------------------------------------------------------------
// Purpose: Returns the attachment point on either the world or viewmodel
//			This should really be worked into the CBaseCombatWeapon class!
//-----------------------------------------------------------------------------
void CWeaponSDKBase::GetWeaponAttachment( int attachmentId, Vector &outVector, Vector *dir /*= NULL*/ )
{
	QAngle	angles;

	if ( IsCarriedByLocalPlayer() )
	{
		CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
		
		if ( pOwner != NULL )
		{
			pOwner->GetViewModel()->GetAttachment( attachmentId, outVector, angles );
			::FormatViewModelAttachment( outVector, true );
		}
	}
	else
	{
		// We offset the IDs to make them correct for our world model
		BaseClass::GetAttachment( attachmentId, outVector, angles );
	}

	// Supply the direction, if requested
	if ( dir != NULL )
	{
		AngleVectors( angles, dir, NULL, NULL );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Setup our laser beam
//-----------------------------------------------------------------------------
void CWeaponSDKBase::InitBeam( void )
{
	if ( m_pBeam != NULL )
		return;

	CBaseCombatCharacter *pOwner = GetOwner();
	
	if ( pOwner == NULL )
		return;

	if ( pOwner->GetAmmoCount(m_iPrimaryAmmoType) <= 0 )
		return;


	BeamInfo_t beamInfo;

	CBaseEntity *pEntity = this;

	if ( IsCarriedByLocalPlayer() )
	{
		CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

		pEntity = NULL;
		if ( pOwner != NULL )
		{
			if ( !pOwner->LAMIsOn() )
				return;

			pEntity = pOwner->GetViewModel();
		}
	}

	beamInfo.m_pStartEnt = pEntity;
	beamInfo.m_pEndEnt = pEntity;
	beamInfo.m_nType = TE_BEAMPOINTS;
	beamInfo.m_vecStart = vec3_origin;
	beamInfo.m_vecEnd = vec3_origin;

	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	if ( !pPlayer->LAMIsOn() )
	{
		beamInfo.m_pszModelName = "sprites/glow_test02";
	}
	else
	{
		beamInfo.m_pszModelName = ( IsCarriedByLocalPlayer() ) ? "effects/laser1_noz.vmt" : "effects/laser1.vmt";
	}

	if ( !pPlayer->LAMIsOn() )
	{
		// A white glow along the weapon
		beamInfo.m_pszHaloName = "sprites/light_glow03.vmt";
		beamInfo.m_flHaloScale = 30.0f;
		beamInfo.m_flLife = 0.0f;
		beamInfo.m_flWidth = 2.5f;
		beamInfo.m_flEndWidth = 25.0f;
		beamInfo.m_nStartAttachment = pEntity->GetBaseAnimating()->LookupAttachment( "beamstart" );
		beamInfo.m_nEndAttachment = pEntity->GetBaseAnimating()->LookupAttachment( "beamend" );
		beamInfo.m_flRed = 255.0;
		beamInfo.m_flBlue = 255.0;
	}
	else
	{
		// The green laser beam
		beamInfo.m_flHaloScale = 0.0f;
		beamInfo.m_flLife = 0.0f;
	
		if ( IsCarriedByLocalPlayer() )
		{
			beamInfo.m_flWidth = 2.0f;
			beamInfo.m_flEndWidth = 2.0f;
		}
		else
		{
			beamInfo.m_flWidth = 1.0f;
			beamInfo.m_flEndWidth = 1.0f;
		}

		beamInfo.m_nStartAttachment = pEntity->GetBaseAnimating()->LookupAttachment( "beamstart" );
		beamInfo.m_nEndAttachment = pEntity->GetBaseAnimating()->LookupAttachment( "beamend" );
		beamInfo.m_flRed = 0.0;
		beamInfo.m_flBlue = 0.0;
	}

	beamInfo.m_flFadeLength = 0.0f;
	beamInfo.m_flAmplitude = 0;
	beamInfo.m_flBrightness = 255.0;
	beamInfo.m_flSpeed = 1.0f;
	beamInfo.m_nStartFrame = 0.0;
	beamInfo.m_flFrameRate = 30.0;
	beamInfo.m_flGreen = 255.0;
	beamInfo.m_nSegments = 4;
	beamInfo.m_bRenderable = true;
	beamInfo.m_nFlags = (FBEAM_FOREVER|FBEAM_SHADEOUT);

	m_pBeam = beams->CreateBeamEntPoint( beamInfo );
}

//-----------------------------------------------------------------------------
// Purpose: Used to determine sorting of model when drawn
//-----------------------------------------------------------------------------
bool CWeaponSDKBase::IsTranslucent( void )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	return pPlayer->LAMIsOn();
}

//-----------------------------------------------------------------------------
// Purpose: Draw the laser aiming module's glint and beam
//-----------------------------------------------------------------------------
void CWeaponSDKBase::DrawEffects( void )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	// Dropped: kill the beam
	if ( GetOwner() == NULL )
	{
		KillBeam();
		return;
	}

	if ( pPlayer == NULL )
		return;

	// A spectator watching through this player's eyes sees the view model's beam only
	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	if ( pLocalPlayer && pLocalPlayer->GetObserverMode() == OBS_MODE_IN_EYE && pLocalPlayer->GetObserverTarget() == pPlayer )
	{
		KillBeam();
		return;
	}

	// The laser must be on (EF_DIMLIGHT)
	if ( !pPlayer->LAMIsOn() || !pPlayer->IsEffectActive( EF_DIMLIGHT ) )
	{
		if ( m_pBeam != NULL )
		{
			m_pBeam->brightness = 0;
		}

		return;
	}

	// No laser when safe, dead, the Hidden, or without the laser aiming module (equipment 0)
	if ( pPlayer->GetSafe() || !pPlayer->IsAlive() || pPlayer->GetTeamNumber() == TEAM_HIDDEN || pPlayer->GetEquipment() != 0 )
		return;

	// Setup our sprite
	if ( m_hSpriteMaterial == NULL )
	{
		m_hSpriteMaterial.Init( "sprites/laserglow1", TEXTURE_GROUP_CLIENT_EFFECTS );
	}

	// Setup our beam
	if ( m_hBeamMaterial == NULL )
	{
		m_hBeamMaterial.Init( "effects/laser1.vmt", TEXTURE_GROUP_CLIENT_EFFECTS );
	}

	color32 color={255,255,255,255};
	Vector	vecAttachment, vecDir;

	float scale = 8.0f + random->RandomFloat( -2.0f, 2.0f );

	GetWeaponAttachment( LookupAttachment( "beamstart" ), vecAttachment, &vecDir );

	// Draw the sprite
	materials->Bind( m_hSpriteMaterial, this );
	DrawSprite( vecAttachment, scale, scale, color );
	
	// Get the beam's run
	trace_t tr;
	UTIL_TraceLine( vecAttachment, vecAttachment + ( vecDir * LASER_BEAM_LENGTH ), MASK_SHOT, GetOwner(), COLLISION_GROUP_NONE, &tr );
	
	InitBeam();

	if ( m_pBeam != NULL )
	{
		m_pBeam->fadeLength = LASER_BEAM_LENGTH;
		m_pBeam->brightness = random->RandomInt( 128, 200 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called on third-person weapon drawing
//-----------------------------------------------------------------------------
int	CWeaponSDKBase::DrawModel( int flags )
{
	// Only render these on the transparent pass
	if ( flags & STUDIO_TRANSPARENCY )
	{
		DrawEffects();
		return 1;
	}

	// Draw the model as normal
	return BaseClass::DrawModel( flags );
}

//-----------------------------------------------------------------------------
// Purpose: Called after first-person viewmodel is drawn
//-----------------------------------------------------------------------------
void CWeaponSDKBase::ViewModelDrawn( C_BaseViewModel *pBaseViewModel )
{
	// Draw our laser effects
	DrawEffects();
	
	BaseClass::ViewModelDrawn( pBaseViewModel );
}

//-----------------------------------------------------------------------------
// Purpose: Turns off effects when leaving the PVS
//-----------------------------------------------------------------------------
void CWeaponSDKBase::NotifyShouldTransmit( ShouldTransmitState_t state )
{
	BaseClass::NotifyShouldTransmit(state);

	if ( state == SHOULDTRANSMIT_END )
	{
		if ( m_pBeam != NULL )
		{
			m_pBeam->brightness = 0.0f;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: The beam goes with the weapon
//-----------------------------------------------------------------------------
void CWeaponSDKBase::Release( void )
{
	KillBeam();

	BaseClass::Release();
}

#endif	//CLIENT_DLL
