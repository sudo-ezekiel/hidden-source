//=============================================================================
//
// Purpose: IRIS FN303 less-lethal launcher (weapon_fn303). Built from the
//			HL2MP crossbow: CCrossbowBolt became CFN303Ball (fn303_bolt), with
//			no sprites, no reflection and no stuck bolts. A hit stuns the Hidden
//			(or a team mate with friendly fire on) and moves the shooter's
//			Hidden selection weighting.
//
//=============================================================================

#include "cbase.h"
#include "weapon_sdkbase.h"
#include "gamevars_shared.h"

#if defined( CLIENT_DLL )

	#define CWeaponFN303 C_WeaponFN303
	#include "c_sdk_player.h"
	#include "c_te_effect_dispatch.h"

#else

	#include "sdk_player.h"
	#include "te_effect_dispatch.h"

#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define BOLT_MODEL			"models/shells/pellet.mdl"

#define BOLT_AIR_VELOCITY	2000
#define BOLT_WATER_VELOCITY	1500

#define BOLT_DAMAGE			13	// REBUILD: name

#ifndef CLIENT_DLL

//-----------------------------------------------------------------------------
// FN303 ball
//-----------------------------------------------------------------------------
class CFN303Ball : public CBaseCombatCharacter
{
	DECLARE_CLASS( CFN303Ball, CBaseCombatCharacter );

public:
	CFN303Ball() { };
	~CFN303Ball();

	Class_T Classify( void ) { return CLASS_NONE; }

public:
	void Spawn( void );
	void Precache( void );
	void BubbleThink( void );
	void BoltTouch( CBaseEntity *pOther );
	bool CreateVPhysics( void );
	unsigned int PhysicsSolidMaskForEntity() const;
	static CFN303Ball *BoltCreate( const Vector &vecOrigin, const QAngle &angAngles, int iDamage, CBasePlayer *pentOwner = NULL );

protected:

	int		m_iDamage;

	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();
};
LINK_ENTITY_TO_CLASS( fn303_bolt, CFN303Ball );

BEGIN_DATADESC( CFN303Ball )
	// Function Pointers
	DEFINE_FUNCTION( BubbleThink ),
	DEFINE_FUNCTION( BoltTouch ),
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CFN303Ball, DT_CFN303Ball )
END_SEND_TABLE()

CFN303Ball *CFN303Ball::BoltCreate( const Vector &vecOrigin, const QAngle &angAngles, int iDamage, CBasePlayer *pentOwner )
{
	// Create a new entity with CFN303Ball private data
	CFN303Ball *pBolt = (CFN303Ball *)CreateEntityByName( "fn303_bolt" );
	UTIL_SetOrigin( pBolt, vecOrigin );
	pBolt->SetAbsAngles( angAngles );
	pBolt->Spawn();
	pBolt->SetOwnerEntity( pentOwner );

	pBolt->m_iDamage = iDamage;

	return pBolt;
}

CFN303Ball::~CFN303Ball( void )
{
}

bool CFN303Ball::CreateVPhysics( void )
{
	// Create the object in the physics system
	VPhysicsInitNormal( SOLID_BBOX, FSOLID_NOT_STANDABLE, false );

	return true;
}

unsigned int CFN303Ball::PhysicsSolidMaskForEntity() const
{
	return ( BaseClass::PhysicsSolidMaskForEntity() | CONTENTS_HITBOX ) & ~CONTENTS_GRATE;
}

void CFN303Ball::Spawn( void )
{
	Precache( );

	SetModel( BOLT_MODEL );
	SetMoveType( MOVETYPE_FLYGRAVITY, MOVECOLLIDE_FLY_CUSTOM );
	UTIL_SetSize( this, -Vector(1,1,1), Vector(1,1,1) );
	SetSolid( SOLID_BBOX );
	SetGravity( 0.5f );
	
	// Make sure we're updated if we're underwater
	UpdateWaterState();

	SetTouch( &CFN303Ball::BoltTouch );

	SetThink( &CFN303Ball::BubbleThink );
	SetNextThink( gpGlobals->curtime + 0.1f );
}

// The weapon precaches the model
void CFN303Ball::Precache( void )
{
}

void CFN303Ball::BubbleThink( void )
{
	QAngle angNewAngles;

	VectorAngles( GetAbsVelocity(), angNewAngles );
	SetAbsAngles( angNewAngles );

	SetNextThink( gpGlobals->curtime + 0.1f );

	if ( GetWaterLevel()  == 0 )
		return;

	UTIL_BubbleTrail( GetAbsOrigin() - GetAbsVelocity() * 0.1f, GetAbsOrigin(), 5 );
}

#else

//-----------------------------------------------------------------------------
// FN303 ball: HL2MP's C_CrossbowBolt without the motion blur trail
//-----------------------------------------------------------------------------
class C_CFN303Ball : public C_BaseCombatCharacter
{
	DECLARE_CLASS( C_CFN303Ball, C_BaseCombatCharacter );
	DECLARE_CLIENTCLASS();
public:

	C_CFN303Ball( void );

	virtual RenderGroup_t GetRenderGroup( void )
	{
		// We want to draw translucent bits as well as our main model
		return RENDER_GROUP_TWOPASS;
	}

	virtual void	ClientThink( void );

	virtual void	OnDataChanged( DataUpdateType_t updateType );
	virtual int		DrawModel( int flags );

private:

	C_CFN303Ball( const C_CFN303Ball & ); // not defined, not accessible

	Vector	m_vecLastOrigin;
	bool	m_bUpdated;
};

IMPLEMENT_CLIENTCLASS_DT( C_CFN303Ball, DT_CFN303Ball, CFN303Ball )
END_RECV_TABLE()

C_CFN303Ball::C_CFN303Ball( void )
{
}

void C_CFN303Ball::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		m_bUpdated = false;
		m_vecLastOrigin = GetAbsOrigin();
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}
}

// REBUILD: Hidden removed the crossbow's motion blur trail; only the normal model draws
int C_CFN303Ball::DrawModel( int flags )
{
	return BaseClass::DrawModel( flags );
}

void C_CFN303Ball::ClientThink( void )
{
	m_bUpdated = false;
}

#endif

//-----------------------------------------------------------------------------
// CWeaponFN303
//-----------------------------------------------------------------------------
class CWeaponFN303 : public CWeaponSDKBase
{
public:
	DECLARE_CLASS( CWeaponFN303, CWeaponSDKBase );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	CWeaponFN303( void );
	
	virtual void	Precache( void );
	virtual void	PrimaryAttack( void );
	virtual void	SecondaryAttack( void );
	virtual bool	Deploy( void );
	virtual bool	Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
	virtual bool	Reload( void );
	virtual void	WeaponIdle( void );

	// REBUILD: Hidden returns the FN2000's ID here
	virtual SDKWeaponID GetWeaponID( void ) const		{ return WEAPON_FN2000; }

private:
	
	void	FireBolt( void );

	CWeaponFN303( const CWeaponFN303 & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponFN303, DT_WeaponFN303 )

BEGIN_NETWORK_TABLE( CWeaponFN303, DT_WeaponFN303 )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponFN303 )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_fn303, CWeaponFN303 );
PRECACHE_WEAPON_REGISTER( weapon_fn303 );

CWeaponFN303::CWeaponFN303( void )
{
}

void CWeaponFN303::Precache( void )
{
	PrecacheModel( BOLT_MODEL );

	CBaseCombatWeapon::Precache();
}

bool CWeaponFN303::Reload( void )
{
	CSDKPlayer *pPlayer = GetPlayerOwner();

	if ( pPlayer->GetAmmoCount( GetPrimaryAmmoType() ) <= 0 )
		return false;

	if ( DefaultReload( GetMaxClip1(), GetMaxClip2(), ACT_VM_RELOAD ) )
	{
		// Drop the empty magazine
		CEffectData data;
		data.m_vOrigin = pPlayer->GetLocalOrigin();
		data.m_vAngles = pPlayer->GetLocalAngles();
		DispatchEffect( "EjectFN303Clip", data );

		return CBaseCombatWeapon::Reload();
	}

	return false;
}

void CWeaponFN303::SecondaryAttack( void )
{
}

bool CWeaponFN303::Deploy( void )
{
	return BaseClass::Deploy();
}

bool CWeaponFN303::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	return BaseClass::Holster( pSwitchingTo );
}

void CWeaponFN303::WeaponIdle()
{
	if (m_flTimeWeaponIdle > gpGlobals->curtime)
		return;

	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( !pPlayer )
		return;

	if ( pPlayer->GetSafe() )
	{
		SendWeaponAnim( ACT_VM_LOWERED );
		SetWeaponIdleTime( gpGlobals->curtime + 5.0 );
	}
	// only idle if the slid isn't back
	else if ( m_iClip1 != 0 )
	{
		SetWeaponIdleTime( gpGlobals->curtime + 5.0 );
		SendWeaponAnim( ACT_VM_IDLE );
	}
}

#ifndef CLIENT_DLL

void CFN303Ball::BoltTouch( CBaseEntity *pOther )
{
	if ( !pOther->IsSolid() || pOther->IsSolidFlagSet(FSOLID_VOLUME_CONTENTS) )
		return;

	if ( pOther->m_takedamage != DAMAGE_NO )
	{
		trace_t	tr, tr2;
		tr = BaseClass::GetTouchTrace();
		Vector	vecNormalizedVel = GetAbsVelocity();

		ClearMultiDamage();
		VectorNormalize( vecNormalizedVel );

		CBasePlayer *pAttacker = ToBasePlayer( GetOwnerEntity() );	// REBUILD: name

		if( GetOwnerEntity() && GetOwnerEntity()->IsPlayer() && pOther->IsNPC() )
		{
			CTakeDamageInfo	dmgInfo( this, GetOwnerEntity(), m_iDamage, DMG_NEVERGIB );
			dmgInfo.AdjustPlayerDamageInflictedForSkillLevel();
			CalculateMeleeDamageForce( &dmgInfo, vecNormalizedVel, tr.endpos, 0.7f );
			dmgInfo.SetDamagePosition( tr.endpos );
			pOther->DispatchTraceAttack( dmgInfo, vecNormalizedVel, &tr );
		}
		else
		{
			CBasePlayer *pPlayer = ToBasePlayer( pOther );	// REBUILD: name

			CTakeDamageInfo	dmgInfo( this, GetOwnerEntity(), 3, DMG_BULLET );
			CalculateMeleeDamageForce( &dmgInfo, vecNormalizedVel, tr.endpos, 0.7f );
			dmgInfo.SetDamagePosition( tr.endpos );
			pOther->DispatchTraceAttack( dmgInfo, vecNormalizedVel, &tr );

			if ( !pPlayer )
			{
				UTIL_ImpactTrace( &tr, DMG_PARALYZE );	// REBUILD: 0x8000 in the binary
			}
			else
			{
				// Stun the Hidden, or a team mate when friendly fire is on
				if ( ( pPlayer->GetTeamNumber() == pAttacker->GetTeamNumber() && friendlyfire.GetInt() > 0 ) || pPlayer->GetTeamNumber() == TEAM_HIDDEN )
					pPlayer->Stun( ToBasePlayer( GetOwnerEntity() ) );

				// Hitting the Hidden makes the shooter more likely to be picked as the next Hidden
				CBasePlayer *pOwner = ToBasePlayer( GetOwnerEntity() );	// REBUILD: name
				if ( pPlayer->GetTeamNumber() == pOwner->GetTeamNumber() )
				{
					pOwner->m_iWeighting.GetForModify() -= 25;
					DevMsg( 1, "303 FF hit, remove 25 from weighting\n" );
				}
				else
				{
					pOwner->m_iWeighting.GetForModify() += 25;
					DevMsg( 1, "303 hit, add 25 to weighting\n" );
				}
			}
		}

		ApplyMultiDamage();

		SetAbsVelocity( Vector( 0, 0, 0 ) );

		// play body "thwack" sound
		EmitSound( "Weapon_FN303.BoltHitBody" );

		Vector vForward;

		AngleVectors( GetAbsAngles(), &vForward );
		VectorNormalize ( vForward );

		SetTouch( NULL );
		SetThink( NULL );

		UTIL_Remove( this );
	}
	else
	{
		trace_t	tr;
		tr = BaseClass::GetTouchTrace();

		// See if we struck the world
		if ( pOther->GetMoveType() == MOVETYPE_NONE && !( tr.surface.flags & SURF_SKY ) )
		{
			EmitSound( "Weapon_FN303.BoltHitWorld" );

			Vector vecDir = GetAbsVelocity();

			SetThink( &CFN303Ball::SUB_Remove );
			SetNextThink( gpGlobals->curtime + 2.0f );

			SetMoveType( MOVETYPE_NONE );
		
			Vector vForward;

			AngleVectors( GetAbsAngles(), &vForward );
			VectorNormalize ( vForward );

			// REBUILD: Hidden kept filling the effect data after dropping the BoltImpact effect
			CEffectData	data;

			data.m_vOrigin = tr.endpos;
			data.m_vNormal = vForward;
			data.m_nEntIndex = 0;

			UTIL_ImpactTrace( &tr, DMG_PARALYZE );	// REBUILD: 0x8000 in the binary

			AddEffects( EF_NODRAW );
			SetTouch( NULL );
			SetThink( &CFN303Ball::SUB_Remove );
			SetNextThink( gpGlobals->curtime + 2.0f );
		}
		else
		{
			// Put a mark unless we've hit the sky
			if ( ( tr.surface.flags & SURF_SKY ) == false )
			{
				UTIL_ImpactTrace( &tr, DMG_PARALYZE );	// REBUILD: 0x8000 in the binary
			}

			UTIL_Remove( this );
		}
	}
}

#endif

void CWeaponFN303::FireBolt( void )
{
	if ( m_iClip1 <= 0 )
	{
		if ( !m_bFireOnEmpty )
		{
			Reload();
		}
		else
		{
			WeaponSound( EMPTY );
			m_flNextPrimaryAttack = 0.15;
		}

		return;
	}

	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	
	if ( pOwner == NULL )
		return;

#ifndef CLIENT_DLL
	Vector vecAiming	= pOwner->GetAutoaimVector( 0 );	
	Vector vecSrc		= pOwner->Weapon_ShootPosition();

	QAngle angAiming;
	VectorAngles( vecAiming, angAiming );

	CFN303Ball *pBolt = CFN303Ball::BoltCreate( vecSrc, angAiming, BOLT_DAMAGE, pOwner );

	if ( pOwner->GetWaterLevel() == 3 )
	{
		pBolt->SetAbsVelocity( vecAiming * BOLT_WATER_VELOCITY );
	}
	else
	{
		pBolt->SetAbsVelocity( vecAiming * BOLT_AIR_VELOCITY );
	}
#endif

	m_iClip1--;

	pOwner->ViewPunch( QAngle( -2, 0, 0 ) );

	WeaponSound( SINGLE );
	WeaponSound( SPECIAL2 );

	SendWeaponAnim( ACT_VM_PRIMARYATTACK );

	if ( !m_iClip1 && pOwner->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 )
	{
		// HEV suit - indicate out of ammo condition
		pOwner->SetSuitUpdate("!HEV_AMO0", FALSE, 0);
	}

	m_flNextPrimaryAttack = m_flNextSecondaryAttack	= gpGlobals->curtime + 0.4;
}

void CWeaponFN303::PrimaryAttack( void )
{
#ifndef CLIENT_DLL
	// REBUILD: client.dll (two months older than the server) has neither of these checks here
	if ( !m_bDeployed )
		return;

	// No firing while safe
	if ( GetPlayerOwner()->GetSafe() )
	{
		m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
		return;
	}
#endif

	FireBolt();

	SetWeaponIdleTime( gpGlobals->curtime + SequenceDuration( ACT_VM_PRIMARYATTACK ) );
}
