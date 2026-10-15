//========= Hidden: Source ============//
//
// Purpose: hdn_ammocrate, an ammo box the I.R.I.S. players open with +use to top
//			up the ammo for their primary and secondary weapons. Built from the
//			HL2 item_ammo_crate.
//
//=====================================//

#include "cbase.h"
#include "player.h"
#include "sdk_shareddefs.h"
#include "hidden/hdn_ammocrate.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


void CHDNAmmoCrate::Precache( void )
{
	PrecacheModel( HDN_AMMOCRATE_MODEL );
}

void CHDNAmmoCrate::Spawn( void )
{
	Precache();
	SetModel( HDN_AMMOCRATE_MODEL );
	SetSolid( SOLID_VPHYSICS );
}

void CHDNAmmoCrate::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBasePlayer *pPlayer = ToBasePlayer( pActivator );
	if ( !pPlayer )
		return;

	// Already opening or closing
	if ( GetSequence() == LookupSequence( "open" ) )
		return;

	if ( GetSequence() == LookupSequence( "close" ) )
		return;

	m_flAnimTime = gpGlobals->curtime;
	m_flPlaybackRate = 0.0;
	SetCycle( 0 );
	ResetSequence( LookupSequence( "open" ) );

	SetNextThink( gpGlobals->curtime + 0.1f );
	SetThink( &CHDNAmmoCrate::Think );

	if ( pPlayer->GetTeamNumber() == TEAM_IRIS )
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
	}

	m_flCloseTime = gpGlobals->curtime + 2.0f;
}

void CHDNAmmoCrate::Think( void )
{
	StudioFrameAdvance();
	DispatchAnimEvents( this );

	SetNextThink( gpGlobals->curtime + 0.1f );

	if ( m_flCloseTime < gpGlobals->curtime && GetSequence() == LookupSequence( "open" ) )
	{
		// Time to close
		m_flAnimTime = gpGlobals->curtime;
		m_flPlaybackRate = 0.0;
		SetCycle( 0 );
		ResetSequence( LookupSequence( "close" ) );
	}
	else if ( GetSequence() == LookupSequence( "close" ) && IsSequenceFinished() )
	{
		// Fully closed: stop thinking
		m_flAnimTime = gpGlobals->curtime;
		m_flPlaybackRate = 0.0;
		SetCycle( 0 );
		ResetSequence( LookupSequence( "idle" ) );
		SetThink( NULL );
	}
}
