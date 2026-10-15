//=============================================================================
//
// Purpose: IRIS sonic alarm (weapon_sonic). Built from the HL2MP SLAM with the
//			satchel and detonator modes removed: it only attaches an npc_tripmine
//			to a wall.
//
//=============================================================================

#include "cbase.h"
#include "gamerules.h"
#include "npcevent.h"
#include "in_buttons.h"
#include "engine/IEngineSound.h"
#include "weapon_sdkbase.h"

#if defined( CLIENT_DLL )
	#include "c_sdk_player.h"
#else
	#include "sdk_player.h"
	#include "hl2_dll/grenade_tripmine.h"
	#include "entitylist.h"
	#include "eventqueue.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define	SLAM_PRIMARY_VOLUME		450

enum
{
	SLAM_TRIPMINE_READY,
	SLAM_SATCHEL_THROW,
	SLAM_SATCHEL_ATTACH,
};

#ifdef CLIENT_DLL
#define CWeapon_Sonic C_Weapon_Sonic
#endif

class CWeapon_Sonic : public CWeaponSDKBase
{
public:
	DECLARE_CLASS( CWeapon_Sonic, CWeaponSDKBase );

	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	CNetworkVar( int,	m_tSlamState );
	CNetworkVar( bool,				m_bDetonatorArmed );
	CNetworkVar( bool,				m_bNeedDetonatorDraw);
	CNetworkVar( bool,				m_bNeedDetonatorHolster);
	CNetworkVar( bool,				m_bNeedReload);
	CNetworkVar( bool,				m_bClearReload);
	CNetworkVar( bool,				m_bThrowSatchel);
	CNetworkVar( bool,				m_bAttachSatchel);
	CNetworkVar( bool,				m_bAttachTripmine);
	float				m_flWallSwitchTime;

	void				Spawn( void );
	void				Precache( void );

	void				PrimaryAttack( void );
	void				SecondaryAttack( void );
	void				WeaponIdle( void );
	void				SLAMThink( void );
	
	void				SetPickupTouch( void );
	void				SlamTouch( CBaseEntity *pOther );	// default weapon touch
	void				ItemPostFrame( void );	
	bool				Reload( void );
	void				SetSlamState( int newState );
	bool				CanAttachSLAM(void);		// In position where can attach SLAM?
	void				StartTripmineAttach( void );
	void				TripmineAttach( void );

	bool				Deploy( void );
	bool				Holster( CBaseCombatWeapon *pSwitchingTo = NULL );

	CWeapon_Sonic();

#ifndef CLIENT_DLL
	DECLARE_DATADESC();
#endif

private:
	CWeapon_Sonic( const CWeapon_Sonic & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( Weapon_Sonic, DT_Weapon_Sonic )

BEGIN_NETWORK_TABLE( CWeapon_Sonic, DT_Weapon_Sonic )
#ifdef CLIENT_DLL
	RecvPropInt( RECVINFO( m_tSlamState ) ),
	RecvPropBool( RECVINFO( m_bDetonatorArmed ) ),
	RecvPropBool( RECVINFO( m_bNeedDetonatorDraw ) ),
	RecvPropBool( RECVINFO( m_bNeedDetonatorHolster ) ),
	RecvPropBool( RECVINFO( m_bNeedReload ) ),
	RecvPropBool( RECVINFO( m_bClearReload ) ),
	RecvPropBool( RECVINFO( m_bThrowSatchel ) ),
	RecvPropBool( RECVINFO( m_bAttachSatchel ) ),
	RecvPropBool( RECVINFO( m_bAttachTripmine ) ),
#else
	SendPropInt( SENDINFO( m_tSlamState ) ),
	SendPropBool( SENDINFO( m_bDetonatorArmed ) ),
	SendPropBool( SENDINFO( m_bNeedDetonatorDraw ) ),
	SendPropBool( SENDINFO( m_bNeedDetonatorHolster ) ),
	SendPropBool( SENDINFO( m_bNeedReload ) ),
	SendPropBool( SENDINFO( m_bClearReload ) ),
	SendPropBool( SENDINFO( m_bThrowSatchel ) ),
	SendPropBool( SENDINFO( m_bAttachSatchel ) ),
	SendPropBool( SENDINFO( m_bAttachTripmine ) ),
#endif
END_NETWORK_TABLE()

// REBUILD: the server binary has all 9 fields, so Hidden did not keep the HL2MP #ifdef CLIENT_DLL here
BEGIN_PREDICTION_DATA( CWeapon_Sonic )
	DEFINE_PRED_FIELD( m_tSlamState, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bDetonatorArmed, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bNeedDetonatorDraw, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bNeedDetonatorHolster, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bNeedReload, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bClearReload, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bThrowSatchel, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bAttachSatchel, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bAttachTripmine, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_sonic, CWeapon_Sonic );
PRECACHE_WEAPON_REGISTER(weapon_sonic);

#ifndef CLIENT_DLL

BEGIN_DATADESC( CWeapon_Sonic )

	DEFINE_FIELD( m_tSlamState, FIELD_INTEGER ),
	DEFINE_FIELD( m_bDetonatorArmed, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bNeedDetonatorDraw, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bNeedDetonatorHolster, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bNeedReload, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bClearReload, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bThrowSatchel, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bAttachSatchel, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bAttachTripmine, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flWallSwitchTime, FIELD_TIME ),

	// Function Pointers
	DEFINE_FUNCTION( SlamTouch ),

END_DATADESC()

#endif

void CWeapon_Sonic::Spawn( )
{
	BaseClass::Spawn();

	Precache( );

	m_tSlamState		= (int)SLAM_SATCHEL_THROW;
	m_flWallSwitchTime	= 0;

	// Give 1 piece of default ammo when first picked up
	m_iClip2 = 1;

	m_bJustDeployed = false;
}

void CWeapon_Sonic::Precache( void )
{
	BaseClass::Precache();

#ifndef CLIENT_DLL
	UTIL_PrecacheOther( "npc_tripmine" );
#endif

	PrecacheScriptSound( "Weapon_Sonic.TripMineMode" );
	PrecacheScriptSound( "Weapon_Sonic.SatchelDetonate" );
	PrecacheScriptSound( "Weapon_Sonic.SatchelThrow" );
}

//------------------------------------------------------------------------------
// Purpose : Override to use slam's pickup touch function
//------------------------------------------------------------------------------
void CWeapon_Sonic::SetPickupTouch( void )
{
	SetTouch(&CWeapon_Sonic::SlamTouch);
}

//-----------------------------------------------------------------------------
// Purpose: Override so give correct ammo
//-----------------------------------------------------------------------------
void CWeapon_Sonic::SlamTouch( CBaseEntity *pOther )
{
	// Can I even pick stuff up?
	if ( pOther->IsEFlagSet( EFL_NO_WEAPON_PICKUP ) )
		return;

	// ---------------------------------------------------
	//  First give weapon to touching entity if allowed
	// ---------------------------------------------------
	BaseClass::DefaultTouch(pOther);
}

bool CWeapon_Sonic::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	SetThink(NULL);
	return BaseClass::Holster(pSwitchingTo);
}

//-----------------------------------------------------------------------------
// Purpose: SLAM has no reload, but must call weapon idle to update state
//-----------------------------------------------------------------------------
bool CWeapon_Sonic::Reload( void )
{
	WeaponIdle( );
	return true;
}

void CWeapon_Sonic::PrimaryAttack( void )
{
	CBaseCombatCharacter *pOwner  = GetOwner();
	if (!pOwner)
	{ 
		return;
	}

	if (pOwner->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
	{
		return;
	}

	StartTripmineAttach();
}

// REBUILD: Hidden removed the satchel detonator, so this does nothing
void CWeapon_Sonic::SecondaryAttack( void )
{
	CBaseCombatCharacter *pOwner  = GetOwner();
	if (!pOwner)
	{
		return;
	}
}

void CWeapon_Sonic::TripmineAttach( void )
{
	CBasePlayer *pOwner  = ToBasePlayer( GetOwner() );
	if (!pOwner)
	{
		return;
	}

	m_bAttachTripmine = false;

	Vector vecSrc, vecAiming;

	// Take the eye position and direction
	vecSrc = pOwner->EyePosition();
	
	QAngle angles = pOwner->GetLocalAngles();

	AngleVectors( angles, &vecAiming );

	trace_t tr;

	UTIL_TraceLine( vecSrc, vecSrc + (vecAiming * 64), MASK_SOLID_BRUSHONLY, pOwner, COLLISION_GROUP_NONE, &tr );
	
	if (tr.fraction < 1.0)
	{
		CBaseEntity *pEntity = tr.m_pEnt;
		if (pEntity && !(pEntity->GetFlags() & FL_CONVEYOR))
		{

#ifndef CLIENT_DLL
			QAngle angles;
			VectorAngles(tr.plane.normal, angles);

			angles.x += 90;

			CBaseEntity *pEnt = CBaseEntity::Create( "npc_tripmine", tr.endpos, angles, NULL );

			CTripmineGrenade *pMine = (CTripmineGrenade *)pEnt;
			pMine->m_hOwner = GetOwner();

#endif

			pOwner->RemoveAmmo( 1, m_iPrimaryAmmoType );
		}
	}
}

void CWeapon_Sonic::StartTripmineAttach( void )
{
	// Only the player fires this way so we can cast
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if (!pPlayer)
	{
		return;
	}

	Vector vecSrc, vecAiming;

	// Take the eye position and direction
	vecSrc = pPlayer->EyePosition();
	
	QAngle angles = pPlayer->GetLocalAngles();

	AngleVectors( angles, &vecAiming );

	trace_t tr;

	UTIL_TraceLine( vecSrc, vecSrc + (vecAiming * 64), MASK_SOLID_BRUSHONLY, pPlayer, COLLISION_GROUP_NONE, &tr );
	
	if (tr.fraction < 1.0)
	{
		CBaseEntity *pEntity = tr.m_pEnt;
		if (pEntity && !(pEntity->GetFlags() & FL_CONVEYOR))
		{
			// player "shoot" animation
			pPlayer->SetAnimation( PLAYER_ATTACK1 );

			// -----------------------------------------
			//  Play attach animation
			// -----------------------------------------
			SendWeaponAnim(ACT_SLAM_TRIPMINE_ATTACH);

			m_bNeedReload		= true;
			m_bAttachTripmine	= true;

			pPlayer->EmitSound( "IRIS.DeploySonicAlarm" );
		}
	}
	
	m_flNextPrimaryAttack	= gpGlobals->curtime + SequenceDuration();
	m_flNextSecondaryAttack	= gpGlobals->curtime + SequenceDuration();
}

void CWeapon_Sonic::SetSlamState( int newState )
{
	// Set set and set idle time so animation gets updated with state change
	m_tSlamState = newState;
	SetWeaponIdleTime( gpGlobals->curtime );
}

void CWeapon_Sonic::SLAMThink( void )
{
	if ( m_flWallSwitchTime > gpGlobals->curtime )
		 return;

	CBaseCombatCharacter *pOwner  = GetOwner();

	if ( (pOwner && pOwner->GetAmmoCount(m_iPrimaryAmmoType) > 0))
	{	
		if (m_tSlamState == SLAM_SATCHEL_THROW)
		{
			SetSlamState(SLAM_TRIPMINE_READY);
			SendWeaponAnim( ACT_SLAM_THROW_TO_TRIPMINE_ND );
			m_flWallSwitchTime = gpGlobals->curtime + SequenceDuration();
			m_bNeedReload = false;
		}
	}
}

bool CWeapon_Sonic::CanAttachSLAM( void )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );

	if (!pOwner)
	{
		return false;
	}

	Vector vecSrc, vecAiming;

	// Take the eye position and direction
	vecSrc = pOwner->EyePosition();
	
	QAngle angles = pOwner->GetLocalAngles();

	AngleVectors( angles, &vecAiming );

	trace_t tr;

	Vector	vecEnd = vecSrc + (vecAiming * 42);
	UTIL_TraceLine( vecSrc, vecEnd, MASK_SOLID, pOwner, COLLISION_GROUP_NONE, &tr );
	
	if (tr.fraction < 1.0)
	{
		// Don't attach to a living creature
		if (tr.m_pEnt)
		{
			CBaseEntity *pEntity = tr.m_pEnt;
			CBaseCombatCharacter *pBCC		= ToBaseCombatCharacter( pEntity );
			if (pBCC)
			{
				return false;
			}
		}
		return true;
	}
	else
	{
		return false;
	}
}

void CWeapon_Sonic::ItemPostFrame( void )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if (!pOwner)
	{
		return;
	}

	SLAMThink();

	if ((pOwner->m_nButtons & IN_ATTACK2) && (m_flNextSecondaryAttack <= gpGlobals->curtime))
	{
		SecondaryAttack();
	}
	else if (!m_bNeedReload && (pOwner->m_nButtons & IN_ATTACK) && (m_flNextPrimaryAttack <= gpGlobals->curtime))
	{
		PrimaryAttack();
	}

	// -----------------------
	//  No buttons down
	// -----------------------
	else 
	{
		WeaponIdle( );
		return;
	}
}

void CWeapon_Sonic::WeaponIdle( void )
{
	// Ready to switch animations?
 	if ( HasWeaponIdleTimeElapsed() )
	{
		// Don't allow throw to attach switch unless in idle
		if (m_bClearReload)
		{
			m_bNeedReload  = false;
			m_bClearReload = false;
		}
		CBaseCombatCharacter *pOwner  = GetOwner();
		if (!pOwner)
		{
			return;
		}

		int iAnim = 0;

		if (m_bAttachTripmine)
		{
			TripmineAttach();
			iAnim = m_bNeedDetonatorDraw ? ACT_SLAM_STICKWALL_ATTACH2 : ACT_SLAM_TRIPMINE_ATTACH2;
		}	
		else if ( m_bNeedReload )
		{	
			// If owner had ammo draw the correct SLAM type
			if (pOwner->GetAmmoCount(m_iPrimaryAmmoType) > 0)
			{
				switch( m_tSlamState)
				{
					case SLAM_TRIPMINE_READY:
						{
							iAnim = ACT_SLAM_TRIPMINE_DRAW;
						}
						break;
					case SLAM_SATCHEL_ATTACH:
						{
							iAnim =	ACT_SLAM_STICKWALL_ND_DRAW;
						}
						break;
					case SLAM_SATCHEL_THROW:
						{
							iAnim =	ACT_SLAM_THROW_ND_DRAW;
						}
						break;
				}
				m_bClearReload			= true;
			}
			else
			{
#ifndef CLIENT_DLL
				pOwner->Weapon_Drop( this );
				UTIL_Remove(this);
#endif
			}
		}
		else if (pOwner->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
		{
#ifndef CLIENT_DLL
			pOwner->Weapon_Drop( this );
			UTIL_Remove(this);
#endif
		}

		// If I don't need to reload just do the appropriate idle
		else
		{
			switch( m_tSlamState)
			{
				case SLAM_TRIPMINE_READY:
					{
						iAnim = m_bDetonatorArmed ? ACT_SLAM_STICKWALL_IDLE : ACT_SLAM_TRIPMINE_IDLE;
						m_flWallSwitchTime = 0;
					}
					break;
			}
		}
		SendWeaponAnim( iAnim );
	}
}

bool CWeapon_Sonic::Deploy( void )
{
	CBaseCombatCharacter *pOwner  = GetOwner();
	if (!pOwner)
	{
		return false;
	}

	SetModel( GetViewModel() );

	m_tSlamState		= (int)SLAM_SATCHEL_THROW;

	// ------------------------------
	// Pick the right draw animation
	// ------------------------------
	int iActivity;

	if (CanAttachSLAM())
	{
		iActivity = ACT_SLAM_TRIPMINE_DRAW; 
		SetSlamState(SLAM_TRIPMINE_READY);
	}
	else
	{
		iActivity = ACT_SLAM_THROW_ND_DRAW; 
		SetSlamState(SLAM_SATCHEL_THROW);
	}

	m_bJustDeployed = false;

	return DefaultDeploy( (char*)GetViewModel(), (char*)GetWorldModel(), iActivity, (char*)GetAnimPrefix() );
}

CWeapon_Sonic::CWeapon_Sonic(void)
{
	m_tSlamState			= (int)SLAM_SATCHEL_THROW;
	m_bDetonatorArmed		= false;
	m_bNeedReload			= true;
	m_bClearReload			= false;
	m_bThrowSatchel			= false;
	m_bAttachSatchel		= false;
	m_bAttachTripmine		= false;
	m_bNeedDetonatorDraw	= false;
	m_bNeedDetonatorHolster	= false;
}
