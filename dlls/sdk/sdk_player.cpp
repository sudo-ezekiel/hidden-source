//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose:		Player for HL1.
//
// $NoKeywords: $
//=============================================================================

#include "cbase.h"
#include "sdk_player.h"
#include "sdk_gamerules.h"
#include "weapon_sdkbase.h"
#include "predicted_viewmodel.h"
#include "iservervehicle.h"
#include "KeyValues.h"
#include "player_pickup.h"
#include "hl2_dll/weapon_physcannon.h"
#include "physics_prop_ragdoll.h"
#include "gib.h"
#include "RagdollBoogie.h"
#include "EntityParticleTrail.h"
#include "beam_shared.h"
#include "in_buttons.h"

extern ConVar hdn_limitbombs;		// sdk_gamerules.cpp


// -------------------------------------------------------------------------------- //
// Player animation event. Sent to the client when a player fires, jumps, reloads, etc..
// -------------------------------------------------------------------------------- //

class CTEPlayerAnimEvent : public CBaseTempEntity
{
public:
	DECLARE_CLASS( CTEPlayerAnimEvent, CBaseTempEntity );
	DECLARE_SERVERCLASS();

					CTEPlayerAnimEvent( const char *name ) : CBaseTempEntity( name )
					{
					}

	CNetworkHandle( CBasePlayer, m_hPlayer );
	CNetworkVar( int, m_iEvent );
};

#define THROWGRENADE_COUNTER_BITS 3

IMPLEMENT_SERVERCLASS_ST_NOBASE( CTEPlayerAnimEvent, DT_TEPlayerAnimEvent )
	SendPropEHandle( SENDINFO( m_hPlayer ) ),
	SendPropInt( SENDINFO( m_iEvent ), Q_log2( PLAYERANIMEVENT_COUNT ) + 1, SPROP_UNSIGNED ),
END_SEND_TABLE()

static CTEPlayerAnimEvent g_TEPlayerAnimEvent( "PlayerAnimEvent" );

void TE_PlayerAnimEvent( CBasePlayer *pPlayer, PlayerAnimEvent_t event )
{
	CPVSFilter filter( (const Vector&)pPlayer->EyePosition() );

	g_TEPlayerAnimEvent.m_hPlayer = pPlayer;
	g_TEPlayerAnimEvent.m_iEvent = event;
	g_TEPlayerAnimEvent.Create( filter, 0 );
}

// -------------------------------------------------------------------------------- //
// Tables.
// -------------------------------------------------------------------------------- //

LINK_ENTITY_TO_CLASS( player, CSDKPlayer );
PRECACHE_REGISTER(player);

BEGIN_SEND_TABLE_NOBASE( CSDKPlayer, DT_SDKLocalPlayerExclusive )
	SendPropInt( SENDINFO( m_iShotsFired ), 8, SPROP_UNSIGNED ),
END_SEND_TABLE()

IMPLEMENT_SERVERCLASS_ST( CSDKPlayer, DT_SDKPlayer )
	SendPropExclude( "DT_BaseAnimating", "m_flPoseParameter" ),
	SendPropExclude( "DT_BaseAnimating", "m_flPlaybackRate" ),
	SendPropExclude( "DT_BaseAnimating", "m_nSequence" ),
	SendPropExclude( "DT_BaseEntity", "m_angRotation" ),
	SendPropExclude( "DT_BaseAnimatingOverlay", "overlay_vars" ),

	// playeranimstate and clientside animation takes care of these on the client
	SendPropExclude( "DT_ServerAnimationData" , "m_flCycle" ),
	SendPropExclude( "DT_AnimTimeMustBeFirst" , "m_flAnimTime" ),

	// Data that only gets sent to the local player.
	SendPropDataTable( "sdklocaldata", 0, &REFERENCE_SEND_TABLE(DT_SDKLocalPlayerExclusive), SendProxy_SendLocalDataTable ),

	SendPropAngle( SENDINFO_VECTORELEM(m_angEyeAngles, 0), 11 ),
	SendPropAngle( SENDINFO_VECTORELEM(m_angEyeAngles, 1), 11 ),
	SendPropEHandle( SENDINFO( m_hRagdoll ) ),

	SendPropInt( SENDINFO( m_iThrowGrenadeCounter ), THROWGRENADE_COUNTER_BITS, SPROP_UNSIGNED ),
END_SEND_TABLE()

class CSDKRagdoll : public CBaseAnimatingOverlay
{
public:
	DECLARE_CLASS( CSDKRagdoll, CBaseAnimatingOverlay );
	DECLARE_SERVERCLASS();

	// Transmit ragdolls to everyone.
	virtual int UpdateTransmitState()
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}

public:
	// In case the client has the player entity, we transmit the player index.
	// In case the client doesn't have it, we transmit the player's model index, origin, and angles
	// so they can create a ragdoll in the right place.
	CNetworkHandle( CBaseEntity, m_hPlayer );	// networked entity handle
	CNetworkVector( m_vecRagdollVelocity );
	CNetworkVector( m_vecRagdollOrigin );
};

LINK_ENTITY_TO_CLASS( sdk_ragdoll, CSDKRagdoll );

IMPLEMENT_SERVERCLASS_ST_NOBASE( CSDKRagdoll, DT_SDKRagdoll )
	SendPropVector( SENDINFO(m_vecRagdollOrigin), -1,  SPROP_COORD ),
	SendPropEHandle( SENDINFO( m_hPlayer ) ),
	SendPropModelIndex( SENDINFO( m_nModelIndex ) ),
	SendPropInt		( SENDINFO(m_nForceBone), 8, 0 ),
	SendPropVector	( SENDINFO(m_vecForce), -1, SPROP_NOSCALE ),
	SendPropVector( SENDINFO( m_vecRagdollVelocity ) )
END_SEND_TABLE()


// -------------------------------------------------------------------------------- //

void cc_CreatePredictionError_f()
{
	CBaseEntity *pEnt = CBaseEntity::Instance( 1 );
	pEnt->SetAbsOrigin( pEnt->GetAbsOrigin() + Vector( 63, 0, 0 ) );
}

ConCommand cc_CreatePredictionError( "CreatePredictionError", cc_CreatePredictionError_f, "Create a prediction error", FCVAR_CHEAT );


//-----------------------------------------------------------------------------
// Purpose: Hidden: how many pipe bombs the Hidden spawns with. With hdn_limitbombs
//			it is one per two marines, less one; otherwise 3.
//-----------------------------------------------------------------------------
int GetBombCount( void )
{
	if ( !hdn_limitbombs.GetBool() )
		return 3;

	int iMarines = 0;
	for ( int i = 0; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( pPlayer && pPlayer->GetTeamNumber() == TEAM_IRIS )
			iMarines++;
	}

	return (int)( ceil( iMarines * 0.5f ) - 1 );
}


CSDKPlayer::CSDKPlayer()
{
	m_PlayerAnimState = CreatePlayerAnimState( this, this, LEGANIM_9WAY, true );

	UseClientSideAnimation();
	m_angEyeAngles.Init();

	SetViewOffset( SDK_PLAYER_VIEW_OFFSET );

	m_iThrowGrenadeCounter = 0;

	// Hidden: the game type comes from the map name prefix
	char szMapName[256];
	Q_FileBase( STRING( gpGlobals->mapname ), szMapName, sizeof( szMapName ) - 1 );
	char *pszPrefix = strtok( szMapName, "_" );

	m_iGameType = 1;
	if ( !Q_strcmp( pszPrefix, "ovr" ) )
		m_iGameType = 2;
	else if ( !Q_strcmp( pszPrefix, "mtr" ) )
		m_iGameType = 3;
	else if ( !Q_strcmp( pszPrefix, "htr" ) )
		m_iGameType = 4;
}


CSDKPlayer::~CSDKPlayer()
{
	m_PlayerAnimState->Release();
}


CSDKPlayer *CSDKPlayer::CreatePlayer( const char *className, edict_t *ed )
{
	CSDKPlayer::s_PlayerEdict = ed;
	return (CSDKPlayer*)CreateEntityByName( className );
}

void CSDKPlayer::LeaveVehicle( const Vector &vecExitPoint, const QAngle &vecExitAngles )
{
	BaseClass::LeaveVehicle( vecExitPoint, vecExitAngles );

	//teleoprt physics shadow too
	Vector newPos = GetAbsOrigin();
	QAngle newAng = GetAbsAngles();

	Teleport( &newPos, &newAng, &vec3_origin );
}

void CSDKPlayer::PreThink(void)
{
	// Riding a vehicle?
	if ( IsInAVehicle() )
	{
		// make sure we update the client, check for timed damage and update suit even if we are in a vehicle
		UpdateClientData();
		CheckTimeBasedDamage();

		// Allow the suit to recharge when in the vehicle.
		CheckSuitUpdate();

		WaterMove();
		return;
	}

	int buttonsChanged = m_afButtonPressed | m_afButtonReleased;

	// The state of the WALK button has changed.
	if ( buttonsChanged & IN_WALK )
	{
		if ( IsWalking() && !( m_afButtonPressed & IN_WALK ) )
		{
			StopWalking();
		}
		else if ( !IsWalking() && ( m_afButtonPressed & IN_WALK ) && !( m_nButtons & IN_DUCK ) )
		{
			StartWalking();
		}
	}

	// Hidden: +aura toggles the Hidden's aura vision
	if ( GetTeamNumber() == TEAM_HIDDEN && ( buttonsChanged & IN_AURA ) )
	{
		if ( IsViewing() && !( m_afButtonPressed & IN_AURA ) )
		{
			SetAura( false );
		}
		else if ( !IsViewing() && ( m_afButtonPressed & IN_AURA ) )
		{
			SetAura( true );
		}
	}

	BaseClass::PreThink();
}


void CSDKPlayer::PostThink()
{
	BaseClass::PostThink();

	QAngle angles = GetLocalAngles();
	angles[PITCH] = 0;
	SetLocalAngles( angles );

	// Store the eye angles pitch so the client can compute its animation state correctly.
	m_angEyeAngles = EyeAngles();

    m_PlayerAnimState->Update( m_angEyeAngles[YAW], m_angEyeAngles[PITCH] );

	// Hidden: the adrenaline boost wears off
	if ( IsBoosted() && GetBoostTimer() < gpGlobals->curtime )
	{
		SetMaxSpeed( 180 );
		SetBoosted( false );
	}
}


void CSDKPlayer::Precache()
{
	PrecacheModel( "models/manor/mn_fixture1.mdl" );
	PrecacheModel( "models/player/iris.mdl" );
	PrecacheModel( "models/player/iris_supply.mdl" );
	PrecacheModel( "models/player/hidden.mdl" );

	PrecacheMaterial( "vgui/hud/hvision" );
	PrecacheMaterial( "vgui/hud/svision" );
	PrecacheMaterial( "vgui/hud/hdn_nightvision" );
	PrecacheMaterial( "vgui/hud/nightvision_dx6" );
	PrecacheMaterial( "vgui/hud/svision_dx6" );
	PrecacheMaterial( "vgui/hud/blur_dx6" );
	PrecacheMaterial( "vgui/hud/blur" );
	PrecacheMaterial( "vgui/hud/helmetcam" );
	PrecacheMaterial( "vgui/hud/hdn_invert" );
	PrecacheMaterial( "sprites/aura" );
	PrecacheMaterial( "frontbuffer" );
	PrecacheMaterial( "models/weapons/v_fn2000/scopemask" );
	PrecacheMaterial( "models/weapons/v_fn2000/scopemask_dx6" );

	PrecacheScriptSound( "Hidden.BehindYou" );
	PrecacheScriptSound( "Hidden.ImHere" );
	PrecacheScriptSound( "Hidden.ISeeYou" );
	PrecacheScriptSound( "Hidden.LookUp" );
	PrecacheScriptSound( "Hidden.TurnAround" );
	PrecacheScriptSound( "Hidden.OverHere" );
	PrecacheScriptSound( "Hidden.YouAreNext" );
	PrecacheScriptSound( "Hidden.FreshMeat" );
	PrecacheScriptSound( "Hidden.ComingForYou" );

	PrecacheScriptSound( "IRIS.Damage" );
	PrecacheScriptSound( "IRIS.RoundStart" );
	PrecacheScriptSound( "IRIS.RoundEnd" );
	PrecacheScriptSound( "IRIS.UnDeployNV" );
	PrecacheScriptSound( "IRIS.DeployNV" );
	PrecacheScriptSound( "IRIS.DeploySonicAlarm" );
	PrecacheScriptSound( "IRIS.EnemySighted" );
	PrecacheScriptSound( "IRIS.RequestingAmmo" );
	PrecacheScriptSound( "IRIS.Affirmative" );
	PrecacheScriptSound( "IRIS.ReportIn" );
	PrecacheScriptSound( "IRIS.ReportingIn" );
	PrecacheScriptSound( "IRIS.AgentDown" );
	PrecacheScriptSound( "IRIS.IfItBleeds" );
	PrecacheScriptSound( "IRIS.OneUglyMother" );
	PrecacheScriptSound( "IRIS.WhereAreYou" );
	PrecacheScriptSound( "IRIS.ComeOnOut" );
	PrecacheScriptSound( "IRIS.BringIt" );

	PrecacheScriptSound( "Player.KnifePigstick.Death" );
	PrecacheScriptSound( "Player.KnifeSlash.Death" );
	PrecacheScriptSound( "Player.FriendlyFire.Death" );

	PrecacheModel( "models/gibs/iris_gibs1.mdl" );
	PrecacheModel( "models/gibs/iris_gibs2.mdl" );
	PrecacheModel( "models/gibs/iris_gibs3.mdl" );
	PrecacheModel( "models/gibs/iris_gibs4.mdl" );
	PrecacheModel( "models/gibs/iris_gibs5.mdl" );
	PrecacheModel( "models/gibs/iris_gibs6.mdl" );
	PrecacheModel( "models/gibs/iris_gibs7.mdl" );
	PrecacheModel( "models/gibs/iris_raggibs_arm.mdl" );
	PrecacheModel( "models/gibs/iris_raggibs_leg.mdl" );
	PrecacheModel( "models/gibs/iristorso_1.mdl" );
	PrecacheModel( "models/gibs/iristorso_2.mdl" );
	PrecacheModel( "models/gibs/iristorso_3.mdl" );

	PrecacheModel( "sprites/light_glow03.vmt" );

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: Hidden: new players get the message of the day and start out spectating
//-----------------------------------------------------------------------------
void CSDKPlayer::InitialSpawn( void )
{
	BaseClass::InitialSpawn();

	const ConVar *hostname = cvar->FindVar( "hostname" );
	const char *title = (hostname) ? hostname->GetString() : "MESSAGE OF THE DAY";

	KeyValues *data = new KeyValues("data");
	data->SetString( "title", title );		// info panel title
	data->SetString( "type", "1" );			// show userdata from stringtable entry
	data->SetString( "msg",	"motd" );		// use this stringtable entry
	data->SetString( "cmd", "classmenu" );	// exec this command if panel closed

	ShowViewPortPanel( "info", true, data );

	data->deleteThis();

	SetModel( "models/player/iris.mdl" );
	ChangeTeam( TEAM_SPECTATOR );
	ChangeClass( -1 );
	m_lifeState = LIFE_DEAD;
	StartObserverMode( OBS_MODE_FIXED );
}

void CSDKPlayer::Spawn()
{
	BaseClass::Spawn();

	// Hidden: reset this life's state
	m_iWeighting.GetForModify() = 0;
	SetNightVision( false );
	SetZoom( false );
	LaserTurnOff();
	CancelStun();
	SetBoost( 0 );
	m_bAmmoRecieved = false;
	SetAmmoRequest( false );

	// A marine needs a full loadout before he can play
	if ( IsReady() )
	{
		if ( GetEquipment() == -1 )
			SetReady( false );
		if ( GetSecondary() == -1 )
			SetReady( false );
		if ( GetPrimary() == -1 )
			SetReady( false );
		if ( GetClass() == -1 )
			SetReady( false );
	}

	if ( GetClass() == -1 || !IsReady() )
	{
		ChangeTeam( TEAM_SPECTATOR );
		StartObserverMode( OBS_MODE_FIXED );
		return;
	}

	if ( GetClass() < 0 )
		return;

	m_takedamage = DAMAGE_YES;
	pl.deadflag = false;
	m_lifeState = LIFE_ALIVE;
	StopObserverMode();
	SetSafe( false );
	SetBloodColor( BLOOD_COLOR_RED );

	SetMoveType( MOVETYPE_WALK );
	RemoveSolidFlags( FSOLID_NOT_SOLID );

	m_hRagdoll = NULL;

	RemoveAllItems( true );

	gEvilImpulse101 = true;

	EquipSuit();
	SetMaxSpeed( 180 );

	if ( GetTeamNumber() == TEAM_IRIS )
	{
		if ( GetClass() == 0 )
			SetModel( "models/player/iris.mdl" );
		else
			SetModel( "models/player/iris_supply.mdl" );

		SetFOV( this, 0 );

		m_nBody = m_iCharacterBody;
		m_nSkin = m_iCharacterSkin;
		SetBodygroup( 2, 2 );

		switch ( GetSecondary() )
		{
		case 0:
			GiveNamedItem( "weapon_pistol" );
			GiveAmmo( 48, "AMMO_PISTOL" );
			break;
		case 1:
			GiveNamedItem( "weapon_pistol2" );
			GiveAmmo( 48, "AMMO_9MM" );
			break;
		}
		SetSecondaryBool( true );

		switch ( GetEquipment() )
		{
		case 0:
			LaserTurnOn();
			break;
		case 3:
			GiveNamedItem( "weapon_sonic" );
			break;
		case 4:
			SetBoost( 3 );
			break;
		}
		SetEquipmentBool( true );

		// The support class always carries sonic grenades
		if ( GetClass() == 1 )
		{
			if ( GetEquipment() == 3 )
			{
				GiveAmmo( 3, "sonic" );
			}
			else
			{
				GiveNamedItem( "weapon_sonic" );
				GiveAmmo( 2, "sonic" );
			}
		}

		switch ( GetPrimary() )
		{
		case 0:
			GiveNamedItem( "weapon_fn2000" );
			GiveAmmo( 60, "AMMO_556" );
			break;
		case 1:
			GiveNamedItem( "weapon_p90" );
			GiveAmmo( 100, "AMMO_BULLETS" );
			break;
		case 2:
			GiveNamedItem( "weapon_shotgun" );
			GiveAmmo( 24, "AMMO_BUCKSHOT" );
			break;
		case 3:
			GiveNamedItem( "weapon_fn303" );
			GiveAmmo( 45, "XBowBolt" );
			break;
		}
		SetPrimaryBool( true );

		m_flStamina = 0;

		SetCollisionGroup( COLLISION_GROUP_PLAYER_MOVEMENT );
	}

	if ( GetTeamNumber() == TEAM_HIDDEN )
	{
		SetModel( "models/manor/mn_fixture1.mdl" );
		SetMaxSpeed( 220 );

		// No pipe bombs in Overrun
		if ( m_iGameType != 2 )
		{
			int iBombs = GetBombCount();
			if ( iBombs > 0 )
			{
				GiveNamedItem( "weapon_grenade" );
				GiveAmmo( iBombs - 1, "AMMO_GRENADE" );
			}
		}

		GiveNamedItem( "weapon_knife" );

		SetFOV( this, 110 );

		m_nBody = 1;
		m_nSkin = 1;

		m_flStamina = 100;

		SetCollisionGroup( COLLISION_GROUP_HIDDEN );
	}

	gEvilImpulse101 = false;

	m_Local.m_iHideHUD = 0;

	// Hidden: the aura trail the Hidden sees on marines
	EntityParticleTrailInfo_t info;
	info.m_strMaterialName = MAKE_STRING( "sprites/aura" );
	info.m_flLifetime = 0.75f;
	info.m_flStartSize = 14.0f;
	info.m_flEndSize = 0.5f;
	CEntityParticleTrail::Create( this, info, NULL );
}

void CSDKPlayer::Event_Killed( const CTakeDamageInfo &info )
{
	// Note: since we're dead, it won't draw us on the client, but we don't set EF_NODRAW
	// because we still want to transmit to the clients in our PVS.

	BaseClass::Event_Killed( info );

	if ( GetClass() >= 0 )
		CreateRagdollEntity( info );
}

void CSDKPlayer::CreateRagdollEntity( const CTakeDamageInfo &info )
{
	// If we already have a ragdoll, don't make another one.
	CSDKRagdoll *pRagdoll = dynamic_cast< CSDKRagdoll* >( m_hRagdoll.Get() );

	// Hidden: the Hidden leaves his own body behind
	if ( GetTeamNumber() == TEAM_HIDDEN )
	{
		SetModel( "models/player/hidden.mdl" );
		m_nBody = 0;
		m_nSkin = 0;
	}

	// Hidden: in Overrun the marines blow apart
	if ( m_iGameType == 2 )
	{
		CGib::SpawnSpecificGibs( this, 1, 750, 1500, "models/gibs/iris_gibs1.mdl", 5 );
		CGib::SpawnSpecificGibs( this, 1, 750, 1500, "models/gibs/iris_gibs2.mdl", 5 );
		CGib::SpawnSpecificGibs( this, 1, 750, 1500, "models/gibs/iris_gibs3.mdl", 5 );
		CGib::SpawnSpecificGibs( this, 1, 750, 1500, "models/gibs/iris_gibs4.mdl", 5 );
		CGib::SpawnSpecificGibs( this, 1, 750, 1500, "models/gibs/iris_gibs6.mdl", 5 );
		CGib::SpawnSpecificGibs( this, 1, 750, 1500, "models/gibs/iris_gibs7.mdl", 5 );
		return;
	}

	if ( !pRagdoll )
	{
		// create a new one
		pRagdoll = dynamic_cast< CSDKRagdoll* >( CreateServerCorpseRagdoll( this, m_nForceBone, info, COLLISION_GROUP_DEBRIS ) );
	}

	if ( pRagdoll )
	{
		pRagdoll->m_hPlayer = this;
		pRagdoll->m_vecRagdollOrigin = GetAbsOrigin();
		pRagdoll->m_vecRagdollVelocity = GetAbsVelocity();
		pRagdoll->m_nModelIndex = m_nModelIndex;
		pRagdoll->m_nForceBone = m_nForceBone;
		pRagdoll->m_nSkin = m_nSkin;
		pRagdoll->m_nBody = m_nBody;
		pRagdoll->m_vecForce = Vector(0,0,0);
	}

	if ( GetTeamNumber() == TEAM_HIDDEN )
	{
		CRagdollBoogie::Create( pRagdoll, 200, gpGlobals->curtime, 0.2f );
	}

	// ragdolls will be removed on round restart automatically
	m_hRagdoll = pRagdoll;
}

void CSDKPlayer::DoAnimationEvent( PlayerAnimEvent_t event )
{
	if ( event == PLAYERANIMEVENT_THROW_GRENADE )
	{
		// Grenade throwing has to synchronize exactly with the player's grenade weapon going away,
		// and events get delayed a bit, so we let CCSPlayerAnimState pickup the change to this
		// variable.
		m_iThrowGrenadeCounter = (m_iThrowGrenadeCounter+1) % (1<<THROWGRENADE_COUNTER_BITS);
	}
	else
	{
		m_PlayerAnimState->DoAnimationEvent( event );
		TE_PlayerAnimEvent( this, event );	// Send to any clients who can see this guy.
	}
}

CWeaponSDKBase* CSDKPlayer::GetActiveSDKWeapon() const
{
	return dynamic_cast< CWeaponSDKBase* >( GetActiveWeapon() );
}

void CSDKPlayer::CreateViewModel( int index /*=0*/ )
{
	Assert( index >= 0 && index < MAX_VIEWMODELS );

	if ( GetViewModel( index ) )
		return;

	CPredictedViewModel *vm = ( CPredictedViewModel * )CreateEntityByName( "predicted_viewmodel" );
	if ( vm )
	{
		vm->SetAbsOrigin( GetAbsOrigin() );
		vm->SetOwner( this );
		vm->SetIndex( index );
		DispatchSpawn( vm );
		vm->FollowEntity( this, false );
		m_hViewModel.Set( index, vm );
	}
}

void CSDKPlayer::CheatImpulseCommands( int iImpulse )
{
	// Hidden: impulse 101 is answered by closing the game
	if ( iImpulse == 101 )
	{
		engine->ClientCommand( edict(), "quit\n" );
	}

	if ( !sv_cheats->GetBool() )
		return;

	if ( iImpulse == 71 )
	{
		gEvilImpulse101 = true;

		EquipSuit();

		// Give weapons
		GiveNamedItem( "weapon_p90" );
		GiveAmmo( 100, "AMMO_BULLETS" );
		GiveNamedItem( "weapon_shotgun" );
		GiveAmmo( 24, "AMMO_BUCKSHOT" );
		GiveNamedItem( "weapon_knife" );
		GiveNamedItem( "weapon_grenade" );
		GiveAmmo( 5, "AMMO_GRENADE" );

		// Give more health
		if ( GetHealth() < 100 )
		{
			TakeHealth( 25, DMG_GENERIC );
		}

		gEvilImpulse101		= false;
	}
	else if ( iImpulse == 101 )
	{
		return;
	}

	BaseClass::CheatImpulseCommands( iImpulse );
}

//-----------------------------------------------------------------------------
// Purpose: Hidden: the flashlight is for the marines only
//-----------------------------------------------------------------------------
int CSDKPlayer::FlashlightIsOn( void )
{
	return IsEffectActive( EF_DIMLIGHT );
}

void CSDKPlayer::FlashlightTurnOn( void )
{
	if ( GetTeamNumber() != TEAM_IRIS )
		return;

	AddEffects( EF_DIMLIGHT );
}

void CSDKPlayer::FlashlightTurnOff( void )
{
	if ( GetTeamNumber() != TEAM_IRIS )
		return;

	RemoveEffects( EF_DIMLIGHT );

	if ( m_pFlashlightBeam )
		m_pFlashlightBeam->TurnOn();
}

//-----------------------------------------------------------------------------
// Purpose: Hidden: +walk slows a marine to 120 and the Hidden to 160
//-----------------------------------------------------------------------------
void CSDKPlayer::StartWalking( void )
{
	if ( GetTeamNumber() == TEAM_IRIS )
		SetMaxSpeed( 120 );
	else
		SetMaxSpeed( 160 );

	m_bIsWalking = true;
}

void CSDKPlayer::StopWalking( void )
{
	if ( GetTeamNumber() == TEAM_IRIS )
		SetMaxSpeed( 180 );
	else
		SetMaxSpeed( 220 );

	m_bIsWalking = false;
}

//-----------------------------------------------------------------------------
// Purpose: Hidden: CHL2_Player's +use, plus the support marine handing out ammo
//-----------------------------------------------------------------------------
void CSDKPlayer::PlayerUse( void )
{
	// Was use pressed or released?
	if ( ! ((m_nButtons | m_afButtonPressed | m_afButtonReleased) & IN_USE) )
		return;

	CBaseEntity *pUseEntity = FindUseEntity();

	bool usedSomething = false;

	// Found an object
	if ( pUseEntity )
	{
		//!!!UNDONE: traceline here to prevent +USEing buttons through walls
		int caps = pUseEntity->ObjectCaps();
		variant_t emptyVariant;

		if ( m_afButtonPressed & IN_USE )
		{
			// Robin: Don't play sounds for NPCs, because NPCs will allow respond with speech.
			if ( !pUseEntity->MyNPCPointer() )
			{
				EmitSound( "HL2Player.Use" );
			}
		}

		if ( ( (m_nButtons & IN_USE) && (caps & FCAP_CONTINUOUS_USE) ) ||
			 ( (m_afButtonPressed & IN_USE) && (caps & (FCAP_IMPULSE_USE|FCAP_ONOFF_USE)) ) )
		{
			if ( caps & FCAP_CONTINUOUS_USE )
				m_afPhysicsFlags |= PFLAG_USING;

			pUseEntity->AcceptInput( "Use", this, this, emptyVariant, USE_TOGGLE );

			usedSomething = true;
		}
		// UNDONE: Send different USE codes for ON/OFF.  Cache last ONOFF_USE object to send 'off' if you turn away
		else if ( (m_afButtonReleased & IN_USE) && (pUseEntity->ObjectCaps() & FCAP_ONOFF_USE) )	// BUGBUG This is an "off" use
		{
			pUseEntity->AcceptInput( "Use", this, this, emptyVariant, USE_TOGGLE );

			usedSomething = true;
		}
		else if ( (m_afButtonReleased & IN_USE) && pUseEntity->GetBaseAnimating() && pUseEntity->GetBaseAnimating()->IsRagdoll() )
		{
			pUseEntity->AcceptInput( "Use", this, this, emptyVariant, USE_TOGGLE );

			usedSomething = true;
		}
		// Hidden: a support marine hands ammo to a marine who asked for it
		else if ( (m_afButtonReleased & IN_USE) && pUseEntity->GetBaseAnimating() && pUseEntity->GetBaseAnimating()->IsPlayer() )
		{
			if ( GetClass() == 0 )
				return;

			if ( GetTeamNumber() == TEAM_HIDDEN )
				return;

			CBasePlayer *pPlayer = ToBasePlayer( pUseEntity );
			if ( pPlayer->GetTeamNumber() == TEAM_IRIS && pPlayer->RequestedAmmo() )
			{
				switch ( pPlayer->GetSecondary() )
				{
				case 0:
					pPlayer->GiveAmmo( 20, "AMMO_PISTOL" );
					break;
				case 1:
					pPlayer->GiveAmmo( 16, "AMMO_9MM" );
					break;
				}

				switch ( pPlayer->GetPrimary() )
				{
				case 0:
					pPlayer->GiveAmmo( 30, "AMMO_556" );
					break;
				case 1:
					pPlayer->GiveAmmo( 50, "AMMO_BULLETS" );
					break;
				case 2:
					pPlayer->GiveAmmo( 8, "AMMO_BUCKSHOT" );
					break;
				case 3:
					pPlayer->GiveAmmo( 15, "XBowBolt" );
					break;
				}

				pPlayer->SetAmmoRequest( false );
				pPlayer->SetAmmoRecieved();

				// Helping out makes you more likely to be picked as the Hidden
				m_iWeighting.GetForModify() += 20;
			}

			usedSomething = true;
		}
	}
	else if ( m_afButtonPressed & IN_USE )
	{
		// Signal that we want to play the deny sound, unless the user is +USEing on a ladder!
		// The sound is emitted in ItemPostFrame, since that occurs after GameMovement::ProcessMove which
		// lets the ladder code unset this flag.
		m_bPlayUseDenySound = true;
	}

	// Debounce the use key
	if ( usedSomething && pUseEntity )
	{
		m_Local.m_nOldButtons |= IN_USE;
		m_afButtonPressed &= ~IN_USE;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Hidden: only the Hidden picks up physics objects
//-----------------------------------------------------------------------------
void CSDKPlayer::PickupObject( CBaseEntity *pObject, bool bLimitMassAndSize )
{
	// can't pick up what you're standing on
	if ( GetGroundEntity() == pObject )
		return;

	if ( GetTeamNumber() != TEAM_HIDDEN )
		return;

	if ( bLimitMassAndSize == true && pObject->GetBaseAnimating() )
	{
		if ( !pObject->GetBaseAnimating()->IsRagdoll() && CBasePlayer::CanPickupObject( pObject, 45, 192 ) == false )
			 return;
	}

	if ( IsHoldingEntity( pObject ) )
	{
		ClearUseEntity();
		return;
	}

	// Can't be picked up if NPCs are on me
	if ( pObject->HasNPCsOnIt() )
		return;

	PlayerPickupObject( this, pObject );
}

bool CSDKPlayer::IsHoldingEntity( CBaseEntity *pEnt )
{
	return PlayerPickupControllerIsHoldingEntity( m_hUseEntity, pEnt );
}

void CSDKPlayer::ForceDropOfCarriedPhysObjects( CBaseEntity *pOnlyIfHoldingThis )
{
	// Drop any objects being handheld.
	ClearUseEntity();
}
