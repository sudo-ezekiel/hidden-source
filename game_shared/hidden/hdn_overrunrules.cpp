//========= Hidden: Source ============//
//
// Purpose: OverRun game rules
//
//=====================================//

#include "cbase.h"
#include "hidden/hdn_overrunrules.h"
#include "sdk_shareddefs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


ConVar hdn_deathwait( "hdn_deathwait", "5", FCVAR_REPLICATED | FCVAR_NOTIFY, "Time between respawns in OverRun" );
ConVar hdn_survivaltime( "hdn_survivaltime", "15", FCVAR_REPLICATED | FCVAR_NOTIFY, "survival time limit" );

REGISTER_GAMERULES_CLASS( COverRunGameRules );


BEGIN_NETWORK_TABLE_NOBASE( COverRunGameRules, DT_OverRunGameRules )
END_NETWORK_TABLE()


LINK_ENTITY_TO_CLASS( overrun_gamerules, COverRunGameRulesProxy );
IMPLEMENT_NETWORKCLASS_ALIASED( OverRunGameRulesProxy, DT_OverRunGameRulesProxy )


#ifdef CLIENT_DLL
	void RecvProxy_OverRunGameRules( const RecvProp *pProp, void **pOut, void *pData, int objectID )
	{
		COverRunGameRules *pRules = OverRunGameRules();
		Assert( pRules );
		*pOut = pRules;
	}

	BEGIN_RECV_TABLE( COverRunGameRulesProxy, DT_OverRunGameRulesProxy )
		RecvPropDataTable( "overrun_gamerules_data", 0, 0, &REFERENCE_RECV_TABLE( DT_OverRunGameRules ), RecvProxy_OverRunGameRules )
	END_RECV_TABLE()
#else
	void *SendProxy_OverRunGameRules( const SendProp *pProp, const void *pStructBase, const void *pData, CSendProxyRecipients *pRecipients, int objectID )
	{
		COverRunGameRules *pRules = OverRunGameRules();
		Assert( pRules );
		pRecipients->SetAllRecipients();
		return pRules;
	}

	BEGIN_SEND_TABLE( COverRunGameRulesProxy, DT_OverRunGameRulesProxy )
		SendPropDataTable( "overrun_gamerules_data", 0, &REFERENCE_SEND_TABLE( DT_OverRunGameRules ), SendProxy_OverRunGameRules )
	END_SEND_TABLE()
#endif


#ifndef CLIENT_DLL

bool COverRunGameRules::ClientCommand( const char *pcmd, CBaseEntity *pEdict )
{
	return BaseClass::ClientCommand( pcmd, pEdict );
}

void COverRunGameRules::RadiusDamage( const CTakeDamageInfo &info, const Vector &vecSrcIn, float flRadius, int iClassIgnore )
{
	BaseClass::RadiusDamage( info, vecSrcIn, flRadius, iClassIgnore );
}

void COverRunGameRules::Think()
{
	BaseClass::Think();
}

//-----------------------------------------------------------------------------
// Purpose: One marine is left: tell everyone what the survival phase is about
//-----------------------------------------------------------------------------
void COverRunGameRules::StartSurvivalThink( void )
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

	GameThinkSet( static_cast<GAMETHINKPTR>( &COverRunGameRules::SurvivalThink ) );
}

//-----------------------------------------------------------------------------
// Purpose: The round as usual, plus a countdown for the survivor
//-----------------------------------------------------------------------------
void COverRunGameRules::SurvivalThink( void )
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

bool COverRunGameRules::HiddenWins( void )
{
	bool bWins = false;
	if ( m_iMarineCount <= 0 )
	{
		UTIL_ClientPrintAll( HUD_PRINTCENTER, "I.R.I.S. Eliminated" );
		bWins = true;
	}

	return bWins;
}

void COverRunGameRules::SetSurvivingMarine( void )
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

void COverRunGameRules::DeathNotice( CBasePlayer *pVictim, const CTakeDamageInfo &info )
{
	BaseClass::DeathNotice( pVictim, info );
}

bool COverRunGameRules::FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker )
{
	return BaseClass::FPlayerCanTakeDamage( pPlayer, pAttacker );
}

bool COverRunGameRules::FPlayerCanRespawn( CBasePlayer *pPlayer )
{
	return BaseClass::FPlayerCanRespawn( pPlayer );
}

void COverRunGameRules::HiddenDisconnected( CBasePlayer *pPlayer )
{
	BaseClass::HiddenDisconnected( pPlayer );
}

void COverRunGameRules::MarineDisconnected( CBasePlayer *pPlayer )
{
	BaseClass::MarineDisconnected( pPlayer );
}

void COverRunGameRules::ClientDisconnected( edict_t *pClient )
{
	BaseClass::ClientDisconnected( pClient );
}

bool COverRunGameRules::ClientConnected( edict_t *pEntity, const char *pszName, const char *pszAddress, char *reject, int maxrejectlen )
{
	return BaseClass::ClientConnected( pEntity, pszName, pszAddress, reject, maxrejectlen );
}

float COverRunGameRules::FlPlayerFallDamage( CBasePlayer *pPlayer )
{
	return BaseClass::FlPlayerFallDamage( pPlayer );
}

#endif

bool COverRunGameRules::ShouldCollide( int collisionGroup0, int collisionGroup1 )
{
	return BaseClass::ShouldCollide( collisionGroup0, collisionGroup1 );
}

#ifndef CLIENT_DLL

const char *COverRunGameRules::GetDeadChatPrefix( bool bTeamOnly, CBasePlayer *pPlayer )
{
	return BaseClass::GetDeadChatPrefix( bTeamOnly, pPlayer );
}

const char *COverRunGameRules::GetChatLocation( bool bTeamOnly, CBasePlayer *pPlayer )
{
	return BaseClass::GetChatLocation( bTeamOnly, pPlayer );
}

const char *COverRunGameRules::GetChatPrefix( bool bTeamOnly, CBasePlayer *pPlayer )
{
	return BaseClass::GetChatPrefix( bTeamOnly, pPlayer );
}

#endif

int COverRunGameRules::PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget )
{
	return BaseClass::PlayerRelationship( pPlayer, pTarget );
}

#ifndef CLIENT_DLL

void COverRunGameRules::RespawnHiddens( void )
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

bool COverRunGameRules::IRISWins( void )
{
	if ( m_pSurvivor && m_pSurvivor->IsAlive() && m_iSurvivalTime <= 0 )
	{
		char szMessage[256];
		Q_snprintf( szMessage, sizeof( szMessage ), "%s Survives!", m_pSurvivor->GetPlayerName() );
		UTIL_ClientPrintAll( HUD_PRINTCENTER, szMessage );

		m_HiddenSelector.NewHidden( m_pSurvivor );
		m_pSurvivor->IncrementFragCount( 2 );
		m_pSurvivor->m_iWeighting.GetForModify() += 4900;
		return true;
	}

	if ( !m_pSurvivor )
		RespawnHiddens();

	return BaseClass::IRISWins();
}

void COverRunGameRules::OverRestartThink( void )
{
	m_DeadPlayers.Purge();
	m_pSurvivor = NULL;

	GameThinkSet( &CSDKGameRules::RoundStartThink );
}

COverRunGameRules::~COverRunGameRules()
{
}

COverRunGameRules::COverRunGameRules()
{
	RestartThinkSet( static_cast<GAMETHINKPTR>( &COverRunGameRules::OverRestartThink ) );
}

void COverRunGameRules::PlayerKilled( CBasePlayer *pVictim, const CTakeDamageInfo &info )
{
	BaseClass::PlayerKilled( pVictim, info );

	CBaseEntity *pInflictor = info.GetInflictor();
	CBaseEntity *pKiller = info.GetAttacker();
	CBasePlayer *pScorer = GetDeathScorer( pKiller, pInflictor );

	// Hiddens keep coming back, the base class counted this one out
	int iTeam = pVictim->GetTeamNumber();
	if ( iTeam == TEAM_HIDDEN )
		m_iHiddenCount++;

	if ( !m_pSurvivor )
	{
		// Marines who kill themselves wait four times as long
		int iWait = hdn_deathwait.GetInt();
		if ( pVictim == pScorer )
		{
			if ( iTeam == TEAM_IRIS )
				iWait *= 4;
		}

		pVictim->SetSpawnTimer( GetRoundTimerRemain(), iWait );
		m_DeadPlayers.AddToTail( pVictim );

		if ( m_iMarineCount == 1 )
		{
			int iSurvivalTime = hdn_survivaltime.GetInt();
			SetSurvivingMarine();

			if ( m_pSurvivor )
			{
				m_iSurvivalTime = GetRoundTimerRemain() - iSurvivalTime;
				m_pSurvivor->SetSpawnTimer( GetRoundTimerRemain(), iSurvivalTime );
				m_pSurvivor->IncrementFragCount( 1 );
				m_pSurvivor->m_iWeighting.GetForModify() += 100;

				GameThinkSet( static_cast<GAMETHINKPTR>( &COverRunGameRules::StartSurvivalThink ) );
			}
		}
	}
}

#endif
