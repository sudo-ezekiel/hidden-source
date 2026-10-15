//========= Hidden: Source ============//
//
// Purpose: Picks the player who plays the Hidden each round.
//
//			hdn_selectmethod chooses how: 0 weighted (players gain weighting
//			for killing the Hidden and lose it when they die), 1 classic (the
//			player who killed the Hidden takes over), 2 random.
//
//=====================================//

#ifndef HDN_HIDDENSELECTION_H
#define HDN_HIDDENSELECTION_H
#ifdef _WIN32
#pragma once
#endif

#include "utlvector.h"

class CBasePlayer;

class CHiddenSelector
{
public:
	CHiddenSelector();
	~CHiddenSelector();

	// Chooses the Hidden for the next round, or returns NULL (and moves everyone to
	// observer mode) when there are not enough players for this game type.
	CBasePlayer		*SelectHidden( int gametype, int maxrounds );

	// The player who killed the Hidden (classic selection only).
	void			NewHidden( CBasePlayer *hidden );

	// The current Hidden gives up his place (died, left or went to spectate).
	void			ForfeitHidden( CBasePlayer *hidden );

	void			ClearHidden( void );
	CBasePlayer		*GetCurrentHidden( void );

private:
	void			FillPlayerVectors( void );
	void			NotEnoughPlayers( CUtlVector<CBasePlayer *> &players, char *type, int total, int ready, int required );
	void			ClientPrintPercentage( CBasePlayer *pPlayer, int iTotalWeight );

	// Copies the players who have not been the Hidden yet; when everyone has been, the flags
	// are reset and everyone is selectable again.
	void			CheckSelectFlags( CUtlVector<CBasePlayer *> &from, CUtlVector<CBasePlayer *> &to, bool reset );

	void			SelectRandomHidden( CUtlVector<CBasePlayer *> &fromthese );
	void			SelectWeightedHidden( CUtlVector<CBasePlayer *> &players );
	void			SetSelectHidden( CUtlVector<CBasePlayer *> &fromthese );

	CUtlVector<CBasePlayer *>	m_prefplayers;	// living IRIS players willing to be the Hidden
	CUtlVector<CBasePlayer *>	m_readyplayers;	// everyone on a team
	CUtlVector<CBasePlayer *>	m_allplayers;		// everyone connected, spectators included
	CBasePlayer					*m_pCurrentHidden;
	int							m_iHiddenRounds;	// rounds the current Hidden has played in a row
};

#endif // HDN_HIDDENSELECTION_H
