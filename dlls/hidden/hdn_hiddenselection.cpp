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
	m_pHidden = NULL;
}

void CHiddenSelector::ForfeitHidden( CBasePlayer *pPlayer )
{
	if ( m_pHidden == pPlayer )
		ClearHidden();
}

void CHiddenSelector::NewHidden( CBasePlayer *pPlayer )
{
	ClearHidden();

	// In classic selection the player who killed the Hidden takes his place
	if ( hdn_selectmethod.GetInt() == 1 && !pPlayer->GetNoHidden() )
		m_pHidden = pPlayer;
}

CBasePlayer *CHiddenSelector::GetCurrentHidden( void )
{
	return m_pHidden;
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
			m_pHidden = pPlayer;
	}

	if ( !m_pHidden )
		UTIL_ClientPrintAll( HUD_PRINTNOTIFY, "No player is eligible for weighted selection." );
}

void CHiddenSelector::NotEnoughPlayers( CUtlVector<CBasePlayer *> &players, char *pszGameName, int iConnected, int iReady, int iNeeded )
{
	for ( int i = 0; i < iConnected; i++ )
		players[i]->StartObserverMode( OBS_MODE_FIXED );

	char szMessage[64];
	int iMore;
	char *pszWhat;
	if ( iConnected < iNeeded )
	{
		iMore = iNeeded - iConnected;
		pszWhat = "connected";
	}
	else
	{
		iMore = iNeeded - iReady;
		pszWhat = "ready";
	}
	Q_snprintf( szMessage, sizeof( szMessage ), "%s Game : Needs %d more %s player(s) to start", pszGameName, iMore, pszWhat );

	UTIL_ClientPrintAll( HUD_PRINTCENTER, szMessage );
}

CHiddenSelector::CHiddenSelector()
{
	m_pHidden = NULL;
	m_iHiddenRounds = 0;
}

CHiddenSelector::~CHiddenSelector()
{
}

void CHiddenSelector::CheckSelectFlags( CUtlVector<CBasePlayer *> &players, CUtlVector<CBasePlayer *> &selectable, bool bReset )
{
	int iCount = players.Count();
	selectable.Purge();

	for ( int i = 0; i < iCount; i++ )
	{
		if ( bReset )
			players[i]->SetHadHidden( false );

		if ( !players[i]->GetHadHidden() )
			selectable.AddToTail( players[i] );
	}

	// Everyone has had a turn: start over
	if ( selectable.Count() == 0 )
		CheckSelectFlags( players, selectable, true );
}

void CHiddenSelector::SelectRandomHidden( CUtlVector<CBasePlayer *> &players )
{
	CUtlVector<CBasePlayer *> selectable;
	CheckSelectFlags( players, selectable, false );

	int iRandom = random->RandomInt( 0, selectable.Count() - 1 );
	m_pHidden = selectable[iRandom];
}

void CHiddenSelector::SetSelectHidden( CUtlVector<CBasePlayer *> &players )
{
	CUtlVector<CBasePlayer *> marines;		// willing, not the Hidden now
	CUtlVector<CBasePlayer *> hiddens;		// willing, the Hidden now
	CUtlVector<CBasePlayer *> unwilling;	// asked not to be the Hidden

	for ( int i = 0; i < players.Count(); i++ )
	{
		CBasePlayer *pPlayer = players[i];
		if ( pPlayer->GetNoHidden() )
			unwilling.AddToTail( pPlayer );
		else if ( pPlayer->GetTeamNumber() == TEAM_HIDDEN )
			hiddens.AddToTail( pPlayer );
		else
			marines.AddToTail( pPlayer );
	}

	// Prefer someone who is not the Hidden yet, and anyone over a player who opted out
	if ( marines.Count() > 0 )
		SelectRandomHidden( marines );
	else if ( hiddens.Count() > 0 )
		SelectRandomHidden( hiddens );
	else
		SelectRandomHidden( unwilling );
}

void CHiddenSelector::FillPlayerVectors( void )
{
	m_EligiblePlayers.Purge();
	m_PlayingPlayers.Purge();
	m_AllPlayers.Purge();

	for ( int i = 0; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( !pPlayer )
			continue;

		int iTeam = pPlayer->GetTeamNumber();
		if ( iTeam != TEAM_SPECTATOR )
		{
			if ( iTeam == TEAM_IRIS && pPlayer->IsAlive() && !pPlayer->GetNoHidden() )
				m_EligiblePlayers.AddToTail( pPlayer );

			m_PlayingPlayers.AddToTail( pPlayer );
		}

		m_AllPlayers.AddToTail( pPlayer );
	}
}

CBasePlayer *CHiddenSelector::SelectHidden( int iGameType, int iMaxRounds )
{
	int iNeeded;
	char *pszGameName;

	if ( iGameType == 1 )
	{
		iNeeded = 2;
		pszGameName = "Hidden";
	}
	else if ( iGameType == 2 )
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

	int iConnected = m_AllPlayers.Count();
	int iEligible = m_EligiblePlayers.Count();
	int iPlaying = m_PlayingPlayers.Count();

	if ( iPlaying < iNeeded )
	{
		ClearHidden();
		NotEnoughPlayers( m_AllPlayers, pszGameName, iConnected, iPlaying, iNeeded );
		return m_pHidden;
	}

	int iMethod = hdn_selectmethod.GetInt();

	if ( iGameType == 3 && !m_pHidden )
	{
		// The marine tutorial plays against a bot
		m_pHidden = TutorialBotPutInServer();
	}
	else if ( iMethod == 2 )
	{
		UTIL_ClientPrintAll( HUD_PRINTNOTIFY, "Hidden Selected Randomly" );
		ClearHidden();
	}
	else if ( m_pHidden && m_pHidden->GetNoHidden() )
	{
		UTIL_ClientPrintAll( HUD_PRINTNOTIFY, "Hidden Forfeited Selection" );
		ClearHidden();
	}
	else if ( iMaxRounds > 0 && m_iHiddenRounds >= iMaxRounds )
	{
		UTIL_ClientPrintAll( HUD_PRINTNOTIFY, "Hidden Round Limit Reached" );
		ClearHidden();
	}

	m_iHiddenRounds++;

	if ( m_pHidden )
		return m_pHidden;

	// Classic and random selection pick at random among the players who have not been the
	// Hidden yet; classic prefers the living IRIS players
	if ( iMethod == 1 || iMethod == 2 )
	{
		if ( iMethod == 1 && iEligible > 0 )
			SetSelectHidden( m_EligiblePlayers );
	}
	else
	{
		SelectWeightedHidden( m_PlayingPlayers );
	}

	if ( !m_pHidden )
		SetSelectHidden( m_PlayingPlayers );

	m_pHidden->SetHadHidden( true );
	return m_pHidden;
}
