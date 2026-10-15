//========= Hidden: Source ============//
//
// Purpose: Evacuation game rules
//
//=====================================//

#include "cbase.h"
#include "hidden/hdn_evacuationrules.h"
#include "sdk_shareddefs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


REGISTER_GAMERULES_CLASS( CEvacuationGameRules );


BEGIN_NETWORK_TABLE_NOBASE( CEvacuationGameRules, DT_EvacuationGameRules )
END_NETWORK_TABLE()


LINK_ENTITY_TO_CLASS( evacuation_gamerules, CEvacuationGameRulesProxy );
IMPLEMENT_NETWORKCLASS_ALIASED( EvacuationGameRulesProxy, DT_EvacuationGameRulesProxy )


#ifdef CLIENT_DLL
	void RecvProxy_EvacuationGameRules( const RecvProp *pProp, void **pOut, void *pData, int objectID )
	{
		CEvacuationGameRules *pRules = EvacuationGameRules();
		Assert( pRules );
		*pOut = pRules;
	}

	BEGIN_RECV_TABLE( CEvacuationGameRulesProxy, DT_EvacuationGameRulesProxy )
		RecvPropDataTable( "evacuation_gamerules_data", 0, 0, &REFERENCE_RECV_TABLE( DT_EvacuationGameRules ), RecvProxy_EvacuationGameRules )
	END_RECV_TABLE()
#else
	void *SendProxy_EvacuationGameRules( const SendProp *pProp, const void *pStructBase, const void *pData, CSendProxyRecipients *pRecipients, int objectID )
	{
		CEvacuationGameRules *pRules = EvacuationGameRules();
		Assert( pRules );
		pRecipients->SetAllRecipients();
		return pRules;
	}

	BEGIN_SEND_TABLE( CEvacuationGameRulesProxy, DT_EvacuationGameRulesProxy )
		SendPropDataTable( "evacuation_gamerules_data", 0, &REFERENCE_SEND_TABLE( DT_EvacuationGameRules ), SendProxy_EvacuationGameRules )
	END_SEND_TABLE()
#endif


#ifndef CLIENT_DLL

bool CEvacuationGameRules::ClientCommand( const char *pcmd, CBaseEntity *pEdict )
{
	return BaseClass::ClientCommand( pcmd, pEdict );
}

void CEvacuationGameRules::RadiusDamage( const CTakeDamageInfo &info, const Vector &vecSrcIn, float flRadius, int iClassIgnore )
{
	BaseClass::RadiusDamage( info, vecSrcIn, flRadius, iClassIgnore );
}

void CEvacuationGameRules::Think()
{
	BaseClass::Think();
}

//-----------------------------------------------------------------------------
// Purpose: One marine is left: tell everyone what the survival phase is about
//-----------------------------------------------------------------------------
void CEvacuationGameRules::StartSurvivalThink( void )
{
	for ( int i = 0; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( !pPlayer || !pPlayer->IsAlive() )
			continue;

		switch ( pPlayer->GetTeamNumber() )
		{
		case TEAM_IRIS:
			ClientPrint( pPlayer, HUD_PRINTCENTER, "Survive" );
			break;
		case TEAM_HIDDEN:
			ClientPrint( pPlayer, HUD_PRINTCENTER, "Eliminate" );
			break;
		}
	}

	GameThinkSet( static_cast<GAMETHINKPTR>( &CEvacuationGameRules::SurvivalThink ) );
}

//-----------------------------------------------------------------------------
// Purpose: The round as usual, plus a countdown for the survivor
//-----------------------------------------------------------------------------
void CEvacuationGameRules::SurvivalThink( void )
{
	GameThink();

	int iTimeLeft = GetRoundTimerRemain() - m_pSurvivor->m_iTimer;
	if ( m_iSurvivalTime != iTimeLeft && m_pSurvivor->IsAlive() )
	{
		m_iSurvivalTime = iTimeLeft;

		// Every 5 seconds, then every second at the end
		if ( iTimeLeft < 6 || iTimeLeft % 5 == 0 )
		{
			char szMessage[256];
			Q_snprintf( szMessage, sizeof( szMessage ), "Survival/Hunt Time : %d Seconds(s)", iTimeLeft );
			UTIL_ClientPrintAll( HUD_PRINTTALK, szMessage );
		}
	}
}

bool CEvacuationGameRules::IRISWins( void )
{
	return BaseClass::IRISWins();
}

bool CEvacuationGameRules::HiddenWins( void )
{
	bool bWins = false;
	if ( m_iMarineCount <= 0 )
	{
		UTIL_ClientPrintAll( HUD_PRINTCENTER, "I.R.I.S. Eliminated" );
		bWins = true;
	}

	return bWins;
}

void CEvacuationGameRules::PlayerKilled( CBasePlayer *pVictim, const CTakeDamageInfo &info )
{
	BaseClass::PlayerKilled( pVictim, info );
}

void CEvacuationGameRules::DeathNotice( CBasePlayer *pVictim, const CTakeDamageInfo &info )
{
	BaseClass::DeathNotice( pVictim, info );
}

bool CEvacuationGameRules::FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker )
{
	return BaseClass::FPlayerCanTakeDamage( pPlayer, pAttacker );
}

bool CEvacuationGameRules::FPlayerCanRespawn( CBasePlayer *pPlayer )
{
	return BaseClass::FPlayerCanRespawn( pPlayer );
}

void CEvacuationGameRules::HiddenDisconnected( CBasePlayer *pPlayer )
{
	BaseClass::HiddenDisconnected( pPlayer );
}

void CEvacuationGameRules::MarineDisconnected( CBasePlayer *pPlayer )
{
	BaseClass::MarineDisconnected( pPlayer );
}

void CEvacuationGameRules::ClientDisconnected( edict_t *pClient )
{
	BaseClass::ClientDisconnected( pClient );
}

bool CEvacuationGameRules::ClientConnected( edict_t *pEntity, const char *pszName, const char *pszAddress, char *reject, int maxrejectlen )
{
	return BaseClass::ClientConnected( pEntity, pszName, pszAddress, reject, maxrejectlen );
}

float CEvacuationGameRules::FlPlayerFallDamage( CBasePlayer *pPlayer )
{
	return BaseClass::FlPlayerFallDamage( pPlayer );
}

void CEvacuationGameRules::SetSurvivingMarine( void )
{
	for ( int i = 0; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( pPlayer && pPlayer->IsAlive() && pPlayer->GetTeamNumber() == TEAM_IRIS && !pPlayer->m_bSpawn )
		{
			m_pSurvivor = pPlayer;
			return;
		}
	}
}

#endif

bool CEvacuationGameRules::ShouldCollide( int collisionGroup0, int collisionGroup1 )
{
	return BaseClass::ShouldCollide( collisionGroup0, collisionGroup1 );
}

#ifndef CLIENT_DLL

const char *CEvacuationGameRules::GetDeadChatPrefix( bool bTeamOnly, CBasePlayer *pPlayer )
{
	return BaseClass::GetDeadChatPrefix( bTeamOnly, pPlayer );
}

const char *CEvacuationGameRules::GetChatLocation( bool bTeamOnly, CBasePlayer *pPlayer )
{
	return BaseClass::GetChatLocation( bTeamOnly, pPlayer );
}

const char *CEvacuationGameRules::GetChatPrefix( bool bTeamOnly, CBasePlayer *pPlayer )
{
	return BaseClass::GetChatPrefix( bTeamOnly, pPlayer );
}

#endif

int CEvacuationGameRules::PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget )
{
	return BaseClass::PlayerRelationship( pPlayer, pTarget );
}

#ifndef CLIENT_DLL

void CEvacuationGameRules::RespawnHiddens( void )
{
	bool bWaiting = false;
	int iCount = m_DeadPlayers.Count();

	for ( int i = 0; i < iCount; i++ )
	{
		if ( !m_DeadPlayers[i] || !m_DeadPlayers[i]->m_bSpawn )
			continue;

		int iTimeLeft = GetRoundTimerRemain() - m_DeadPlayers[i]->m_iTimer;
		if ( iTimeLeft < 0 )
			iTimeLeft = 0;

		char szMessage[256];
		Q_snprintf( szMessage, sizeof( szMessage ), "Spawning In : %d Second(s)", iTimeLeft );
		ClientPrint( m_DeadPlayers[i], HUD_PRINTCENTER, szMessage );

		if ( iTimeLeft < 1 )
		{
			// Dead marines come back on the Hidden team
			m_DeadPlayers[i]->SetSpawnBool( false );
			m_DeadPlayers[i]->ChangeTeam( TEAM_HIDDEN );
			m_DeadPlayers[i]->Spawn();
		}
		else
		{
			bWaiting = true;
		}
	}

	if ( !bWaiting )
		m_DeadPlayers.Purge();
}

void CEvacuationGameRules::OverRestartThink( void )
{
	m_DeadPlayers.Purge();
	m_pSurvivor = NULL;

	GameThinkSet( &CSDKGameRules::RoundStartThink );
}

CEvacuationGameRules::~CEvacuationGameRules()
{
}

CEvacuationGameRules::CEvacuationGameRules()
{
	RestartThinkSet( static_cast<GAMETHINKPTR>( &CEvacuationGameRules::OverRestartThink ) );
}

#endif
