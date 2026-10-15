//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "hltvcamera.h"
#include "cdll_client_int.h"
#include "util_shared.h"
#include <KeyValues.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define CHASE_CAM_DISTANCE		96.0f
#define WALL_OFFSET				6.0f

static Vector WALL_MIN(-WALL_OFFSET,-WALL_OFFSET,-WALL_OFFSET);
static Vector WALL_MAX(WALL_OFFSET,WALL_OFFSET,WALL_OFFSET);

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

static C_HLTVCamera s_HLTVCamera;

C_HLTVCamera *HLTVCamera() { return &s_HLTVCamera; }

C_HLTVCamera::C_HLTVCamera()
{

}

C_HLTVCamera::~C_HLTVCamera()
{

}

void C_HLTVCamera::Init()
{
	gameeventmanager->AddListener( this, "game_newmap", false );
	gameeventmanager->AddListener( this, "hltv_cameraman", false );
	gameeventmanager->AddListener( this, "hltv_fixed", false );
	gameeventmanager->AddListener( this, "hltv_chase", false );
	gameeventmanager->AddListener( this, "hltv_fov", false );
	gameeventmanager->AddListener( this, "hltv_inertia", false );

	Reset();
}

void C_HLTVCamera::Shutdown()
{
	gameeventmanager->RemoveListener( this );
}

void C_HLTVCamera::Reset()
{
	m_nCameraMode = OBS_MODE_FIXED;
	m_iCameraMan  = 0;
	m_iTraget1 = m_iTraget2 = 0;
	m_flFOV = 90;
	m_flDistance = m_flLastDistance = 96.0f;
	m_flInertia = 10.0f;
	m_flPhi = 0;
	m_flTheta = 0;
	m_flOffset = 0;

	m_vCamOrigin.Init();
	m_aCamAngle.Init();
}

void C_HLTVCamera::CalcChaseCamView( Vector& eyeOrigin, QAngle& eyeAngles, float& fov )
{
	Vector targetOrigin1, cameraOrigin, forward;

	if ( m_iTraget1 == 0 )
		return;

	C_BaseEntity *target1 = GetPrimaryTarget();

	if ( !target1 )
		return;

	// get secondary target if set
	C_BaseEntity *target2 = NULL;

	if ( m_iTraget2 > 0 )
	{
		target2 = ClientEntityList().GetBaseEntity( m_iTraget2 );
	}

	targetOrigin1 = target1->GetAbsOrigin();
	targetOrigin1.z += m_flOffset; // add offset

	QAngle cameraAngles;

	if ( target2 )
	{
		// look into direction of second target
		forward = target2->GetAbsOrigin() - target1->GetAbsOrigin();
		VectorAngles( forward, cameraAngles );
		cameraAngles.z = 0; // no ROLL
	}
	else
	{
		// look into direction where primary target is looking
		cameraAngles = target1->EyeAngles();
		cameraAngles.x = 0; // no PITCH
		cameraAngles.z = 0; // no ROLL
	}

	// apply angle offset & smoothing
	QAngle angleOffset( m_flPhi, m_flTheta, 0 );
	cameraAngles += angleOffset;

	InterpolateAngles( m_aCamAngle, cameraAngles, cameraAngles, m_flInertia/100.0f );

	AngleVectors( cameraAngles, &forward );

	VectorNormalize( forward );

	if ( m_flLastDistance < m_flDistance )
	{
		// grow distance by 32 unit a second
		m_flLastDistance += gpGlobals->frametime * 32.0f; 

		// but not more than m_flDistance
		m_flLastDistance = min( m_flDistance, m_flLastDistance );
	}
	else
	{
		m_flLastDistance = m_flDistance;
	}

	// calc optimal camera position
	VectorMA(targetOrigin1, -m_flLastDistance, forward, cameraOrigin );

	// clip against walls
	trace_t trace;

	// Hidden: when a wall is closer than 48 units, the camera tries up to twice more, 24 units higher each time
	for ( int i = 3; i > 0; i-- ) // REBUILD: the loop counter's name is ours
	{
		C_BaseEntity::EnableAbsRecomputations( false ); // HACK don't recompute positions while doing RayTrace
		UTIL_TraceHull( targetOrigin1, cameraOrigin, WALL_MIN, WALL_MAX, MASK_SOLID, target1, COLLISION_GROUP_NONE, &trace );
		C_BaseEntity::EnableAbsRecomputations( true );

		if (trace.fraction < 1.0)
		{
			if ( VectorLength( trace.endpos - targetOrigin1 ) < 48 && i > 1 )
			{
				cameraOrigin.z += 24;
				continue;
			}

			// we hit a wall, correct poistion and distance
			cameraOrigin = trace.endpos;
			m_flLastDistance = VectorLength( cameraOrigin -  targetOrigin1 );
		}

		break;
	}

	if ( target2 )
	{
		// if we have 2 targets look at point between them
		forward = target2->GetAbsOrigin() - target1->GetAbsOrigin();
		VectorMA( target1->GetAbsOrigin(), 0.5f, forward, targetOrigin1 );
		forward = targetOrigin1 - cameraOrigin;
		VectorAngles( forward, cameraAngles );
		cameraAngles.z = 0; // no ROLL
	}

	VectorCopy( cameraAngles, m_aCamAngle );
	VectorCopy( cameraOrigin, m_vCamOrigin );

	VectorCopy( m_aCamAngle, eyeAngles );
	VectorCopy( m_vCamOrigin, eyeOrigin );
}

int C_HLTVCamera::GetMode()
{
	if ( m_iCameraMan > 0 )
	{
		C_BasePlayer *pCameraMan = UTIL_PlayerByIndex( m_iCameraMan );

		if ( pCameraMan )
			return pCameraMan->GetObserverMode();
	}

	return m_nCameraMode;	
}

C_BaseEntity* C_HLTVCamera::GetPrimaryTarget()
{
	if ( m_iCameraMan > 0 )
	{
		C_BasePlayer *pCameraMan = UTIL_PlayerByIndex( m_iCameraMan );
		
		if ( pCameraMan )
		{
			return pCameraMan->GetObserverTarget();
		}
	}

	C_BaseEntity* target = ClientEntityList().GetEnt( m_iTraget1 );

	// REBUILD: client.dll asks the target IsPlayer() here and does nothing with the answer
	if ( target && target->IsPlayer() )
	{
	}

	return target;
}

C_BaseEntity *C_HLTVCamera::GetCameraMan()
{
	return ClientEntityList().GetEnt( m_iCameraMan );
}

void C_HLTVCamera::CalcInEyeCamView( Vector& eyeOrigin, QAngle& eyeAngles, float& fov )
{
	C_BasePlayer *pPlayer = UTIL_PlayerByIndex( m_iTraget1 );

	if ( !pPlayer )
		return;

	m_aCamAngle	= pPlayer->EyeAngles();
	m_vCamOrigin = pPlayer->GetAbsOrigin();
	fov = m_flFOV =	pPlayer->GetFOV();

	if ( pPlayer->GetFlags() & FL_DUCKING )
	{
		m_vCamOrigin += GameRules()->GetViewVectors()->m_vDuckView;
	}
	else
	{
		m_vCamOrigin += GameRules()->GetViewVectors()->m_vView;
	}

	eyeOrigin = m_vCamOrigin;
	eyeAngles = m_aCamAngle;
	
}

void C_HLTVCamera::CalcRoamingView(Vector& eyeOrigin, QAngle& eyeAngles, float& fov)
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();

	if ( !pPlayer )
		return;

	eyeOrigin = m_vCamOrigin = pPlayer->EyePosition();
	eyeAngles = m_aCamAngle = pPlayer->EyeAngles();
	fov = m_flFOV = pPlayer->GetFOV();
}

void C_HLTVCamera::CalcFixedView(Vector& eyeOrigin, QAngle& eyeAngles, float& fov)
{
	eyeOrigin = m_vCamOrigin;
	eyeAngles = m_aCamAngle;
	fov = m_flFOV;

	if ( m_iTraget1 == 0 )
		return;

	C_BaseEntity * traget = ClientEntityList().GetBaseEntity( m_iTraget1 );
	
	if ( traget )
	{
		// if we're chasing a target, change viewangles
		VectorAngles( traget->GetAbsOrigin() - m_vCamOrigin, m_aCamAngle );
	}
}

void C_HLTVCamera::GetCurrentView(Vector& origin, QAngle& angles, float& fov)
{
	if ( m_iCameraMan > 0 )
	{
		C_BasePlayer *pCameraMan = UTIL_PlayerByIndex( m_iCameraMan );
		if ( pCameraMan )
		{
			float zNear,zFar;
			pCameraMan->CalcView( origin, angles, zNear, zFar, fov );
			pCameraMan->CalcViewModelView( origin, angles );
			return;
		}
	}

	// Hidden: no roaming or chase views; mode 3 (the cameraman's) shows the fixed view
	// REBUILD: client.dll switches on the values 1, 2 and 3; the name of 3 is shareddefs.h's guess
	switch ( m_nCameraMode )
	{
		case OBS_MODE_FIXED		:
		case OBS_MODE_DEATHCAM	:	CalcFixedView( origin, angles, fov );
									break;

		case OBS_MODE_IN_EYE	:	CalcInEyeCamView( origin, angles, fov );
									break;
	}
}

void C_HLTVCamera::SetMode(int iMode)
{
	Assert( iMode > OBS_MODE_NONE && iMode <= OBS_MODE_ROAMING );
	m_nCameraMode = iMode;
}

void C_HLTVCamera::SpecNextPlayer( bool bInverse )
{
	int start = 1;

	if ( m_iTraget1 > 0 && m_iTraget1 <= gpGlobals->maxClients )
		start = m_iTraget1;

	int index = start;

	while ( true )
	{	
		// got next/prev player
		if ( bInverse )
			index--;
		else
			index++;

		// check bounds
		if ( index < 1 )
			index = gpGlobals->maxClients;
		else if ( index > gpGlobals->maxClients )
			index = 1;

		if ( index == start )
			break; // couldn't find a new valid player

		C_BasePlayer *pPlayer =	UTIL_PlayerByIndex( index );

		if ( !pPlayer )
			continue;

		// only follow living players or dedicated spectators
		if ( pPlayer->IsObserver() && pPlayer->GetTeamNumber() != TEAM_SPECTATOR )
			continue;

		break; // found a new player
	}

	m_iTraget1 = index;
}

void C_HLTVCamera::SpecNamedPlayer( const char *szPlayerName )
{
	// REBUILD: client.dll's loop stops before index maxClients
	for ( int index = 1; index < gpGlobals->maxClients; ++index )
	{
		C_BasePlayer *pPlayer =	UTIL_PlayerByIndex( index );

		if ( !pPlayer )
			continue;

		if ( !FStrEq( szPlayerName, pPlayer->GetPlayerName() ) )
			continue;

		// only follow living players or dedicated spectators
		if ( pPlayer->IsObserver() && pPlayer->GetTeamNumber() != TEAM_SPECTATOR )
			continue;

		m_iTraget1 = index;
		return;
	}
}

void C_HLTVCamera::FireGameEvent( IGameEvent *event )
{
	if ( !engine->IsHLTV() )
		return;	// not in HLTV mode

	const char *type = event->GetName();

	if ( Q_strcmp( "game_newmap", type ) == 0 )
	{
		Reset();	// reset all camera settings
		return;
	}

	else if ( Q_strcmp( "hltv_cameraman", type ) == 0 )
	{
		Reset();

		// REBUILD: client.dll sets mode 3 here, where Valve's code has OBS_MODE_ROAMING
		m_nCameraMode = OBS_MODE_DEATHCAM;
		
		if ( event->GetBool( "active" ) )
		{
			m_iCameraMan = event->GetInt( "index" );
		}
		else 
		{
			m_iCameraMan = 0;
		}

		return;
	}

	else if ( Q_strcmp( "hltv_fixed", type ) == 0 )
	{
		m_nCameraMode = OBS_MODE_FIXED;
		m_iCameraMan = 0;
		
		m_vCamOrigin.x = event->GetInt( "posx" );
		m_vCamOrigin.y = event->GetInt( "posy" );
		m_vCamOrigin.z = event->GetInt( "posz" );

		m_aCamAngle.x = event->GetInt( "theta" );
		m_aCamAngle.y = event->GetInt( "phi" );
		m_aCamAngle.z = 0; // no roll yet

		m_iTraget1 = event->GetInt( "target" );
		
		return;
	}
}