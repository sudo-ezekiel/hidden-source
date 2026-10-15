//========= Hidden: Source ============//
//
// Purpose: Picks the player who plays the Hidden each round.
//
//=====================================//

#include "cbase.h"
#include "player.h"
#include "hidden/hdn_hiddenselection.h"
#include "sdk_bot_temp.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar hdn_selectmethod( "hdn_selectmethod", "0", FCVAR_REPLICATED | FCVAR_NOTIFY, "0 = weighted, 1 = classic, 2 = random - defaults to weighted" );

void CHiddenSelector::ClearHidden( void )
{
	m_iHiddenRounds = 0;
	m_pCurrentHidden = NULL;
}

void CHiddenSelector::ForfeitHidden( CBasePlayer *hidden )
{
	if ( m_pCurrentHidden == hidden )
		ClearHidden();
}

void CHiddenSelector::NewHidden( CBasePlayer *hidden )
{
	ClearHidden();

	// In classic selection the player who killed the Hidden takes his place
	if ( hdn_selectmethod.GetInt() == 1 && !hidden->GetNoHidden() )
		m_pCurrentHidden = hidden;
}

CBasePlayer *CHiddenSelector::GetCurrentHidden( void )
{
	return m_pCurrentHidden;
}

void CHiddenSelector::ClientPrintPercentage( CBasePlayer *pPlayer, int iTotalWeight )
{
	int iWeighting = pPlayer->m_iWeighting;
	if ( iWeighting > 0 && iTotalWeight > 0 )
	{
		char szMessage[256];
		Q_snprintf( szMessage, sizeof( szMessage ), "%s has a %.2f percent chance of weighted selection", pPlayer->GetPlayerName(),
			( (float)pPlayer->m_iWeighting / (float)iTotalWeight ) * 100.0f );
		UTIL_ClientPrintAll( HUD_PRINTNOTIFY, szMessage );
	}
}

void CHiddenSelector::SelectWeightedHidden( CUtlVector<CBasePlayer *> &players )
{
	int iTotalWeight = 0;
	int iCount = players.Count();
	int i;

	// Give every candidate a range of numbers as wide as his weighting
	for ( i = 0; i < iCount; i++ )
	{
		CBasePlayer *pPlayer = players[i];
		int iWeight = pPlayer->m_iWeighting;

		if ( iWeight > 0 && pPlayer->GetTeamNumber() != TEAM_HIDDEN && !pPlayer->GetNoHidden() )
		{
			pPlayer->m_iWeightMin = iTotalWeight;
			iTotalWeight += iWeight;
			pPlayer->m_iWeightMax = iTotalWeight - 1;
		}
		else
		{
			pPlayer->m_iWeightMin = -1;
			pPlayer->m_iWeightMax = -1;
		}
	}

	int iPick = random->RandomInt( 0, iTotalWeight - 1 );

	for ( i = 0; i < iCount; i++ )
	{
		CBasePlayer *pPlayer = players[i];
		int iMin = pPlayer->m_iWeightMin;
		int iMax = pPlayer->m_iWeightMax;

		if ( iMin < 0 )
			continue;

		ClientPrintPercentage( pPlayer, iTotalWeight );

		if ( iMin <= iPick && iMax >= iPick )
			m_pCurrentHidden = pPlayer;
	}

	if ( !m_pCurrentHidden )
		UTIL_ClientPrintAll( HUD_PRINTNOTIFY, "No player is eligible for weighted selection." );
}

void CHiddenSelector::NotEnoughPlayers( CUtlVector<CBasePlayer *> &players, char *type, int total, int ready, int required )
{
	for ( int i = 0; i < total; i++ )
		players[i]->StartObserverMode( OBS_MODE_FIXED );

	char cbuffer[64];
	int iMore;
	char *pszWhat;
	if ( total < required )
	{
		iMore = required - total;
		pszWhat = "connected";
	}
	else
	{
		iMore = required - ready;
		pszWhat = "ready";
	}
	Q_snprintf( cbuffer, sizeof( cbuffer ), "%s Game : Needs %d more %s player(s) to start", type, iMore, pszWhat );

	UTIL_ClientPrintAll( HUD_PRINTCENTER, cbuffer );
}

CHiddenSelector::CHiddenSelector()
{
	m_pCurrentHidden = NULL;
	m_iHiddenRounds = 0;
}

CHiddenSelector::~CHiddenSelector()
{
}

void CHiddenSelector::CheckSelectFlags( CUtlVector<CBasePlayer *> &from, CUtlVector<CBasePlayer *> &to, bool reset )
{
	int count = from.Count();
	to.Purge();

	for ( int i = 0; i < count; i++ )
	{
		if ( reset )
			from[i]->SetHadHidden( false );

		if ( !from[i]->GetHadHidden() )
			to.AddToTail( from[i] );
	}

	// Everyone has had a turn: start over
	if ( to.Count() == 0 )
		CheckSelectFlags( from, to, true );
}

void CHiddenSelector::SelectRandomHidden( CUtlVector<CBasePlayer *> &fromthese )
{
	CUtlVector<CBasePlayer *> realfrom;
	CheckSelectFlags( fromthese, realfrom, false );

	int iRandom = random->RandomInt( 0, realfrom.Count() - 1 );
	m_pCurrentHidden = realfrom[iRandom];
}

void CHiddenSelector::SetSelectHidden( CUtlVector<CBasePlayer *> &fromthese )
{
	CUtlVector<CBasePlayer *> marines;		// willing, not the Hidden now
	CUtlVector<CBasePlayer *> hiddens;		// willing, the Hidden now
	CUtlVector<CBasePlayer *> forfeits;	// asked not to be the Hidden

	for ( int i = 0; i < fromthese.Count(); i++ )
	{
		CBasePlayer *candidate = fromthese[i];
		if ( candidate->GetNoHidden() )
			forfeits.AddToTail( candidate );
		else if ( candidate->GetTeamNumber() == TEAM_HIDDEN )
			hiddens.AddToTail( candidate );
		else
			marines.AddToTail( candidate );
	}

	// Prefer someone who is not the Hidden yet, and anyone over a player who opted out
	if ( marines.Count() > 0 )
		SelectRandomHidden( marines );
	else if ( hiddens.Count() > 0 )
		SelectRandomHidden( hiddens );
	else
		SelectRandomHidden( forfeits );
}

void CHiddenSelector::FillPlayerVectors( void )
{
	m_prefplayers.Purge();
	m_readyplayers.Purge();
	m_allplayers.Purge();

	for ( int i = 0; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *player = UTIL_PlayerByIndex( i );
		if ( !player )
			continue;

		int iTeam = player->GetTeamNumber();
		if ( iTeam != TEAM_SPECTATOR )
		{
			if ( iTeam == TEAM_IRIS && player->IsAlive() && !player->GetNoHidden() )
				m_prefplayers.AddToTail( player );

			m_readyplayers.AddToTail( player );
		}

		m_allplayers.AddToTail( player );
	}
}

CBasePlayer *CHiddenSelector::SelectHidden( int gametype, int maxrounds )
{
	int iNeeded;
	char *pszGameName;

	if ( gametype == 1 )
	{
		iNeeded = 2;
		pszGameName = "Hidden";
	}
	else if ( gametype == 2 )
	{
		iNeeded = 3;
		pszGameName = "Overrun";
	}
	else
	{
		iNeeded = 1;
		pszGameName = "Tutorial";
	}

	FillPlayerVectors();

	int iConnected = m_allplayers.Count();
	int prefcount = m_prefplayers.Count();
	int iPlaying = m_readyplayers.Count();

	if ( iPlaying < iNeeded )
	{
		ClearHidden();
		NotEnoughPlayers( m_allplayers, pszGameName, iConnected, iPlaying, iNeeded );
		return m_pCurrentHidden;
	}

	int iMethod = hdn_selectmethod.GetInt();

	if ( gametype == 3 && !m_pCurrentHidden )
	{
		// The marine tutorial plays against a bot
		m_pCurrentHidden = TutorialBotPutInServer();
	}
	else if ( iMethod == 2 )
	{
		UTIL_ClientPrintAll( HUD_PRINTNOTIFY, "Hidden Selected Randomly" );
		ClearHidden();
	}
	else if ( m_pCurrentHidden && m_pCurrentHidden->GetNoHidden() )
	{
		UTIL_ClientPrintAll( HUD_PRINTNOTIFY, "Hidden Forfeited Selection" );
		ClearHidden();
	}
	else if ( maxrounds > 0 && m_iHiddenRounds >= maxrounds )
	{
		UTIL_ClientPrintAll( HUD_PRINTNOTIFY, "Hidden Round Limit Reached" );
		ClearHidden();
	}

	m_iHiddenRounds++;

	if ( m_pCurrentHidden )
		return m_pCurrentHidden;

	// Classic and random selection pick at random among the players who have not been the
	// Hidden yet; classic prefers the living IRIS players
	if ( iMethod == 1 || iMethod == 2 )
	{
		if ( iMethod == 1 && prefcount > 0 )
			SetSelectHidden( m_prefplayers );
	}
	else
	{
		SelectWeightedHidden( m_readyplayers );
	}

	if ( !m_pCurrentHidden )
		SetSelectHidden( m_readyplayers );

	m_pCurrentHidden->SetHadHidden( true );
	return m_pCurrentHidden;
}
