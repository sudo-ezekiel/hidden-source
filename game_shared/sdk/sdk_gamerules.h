//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: The TF Game rules object
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#ifndef SDK_GAMERULES_H
#define SDK_GAMERULES_H

#ifdef _WIN32
#pragma once
#endif


#include "teamplay_gamerules.h"
#include "convar.h"
#include "gamevars_shared.h"

#ifdef CLIENT_DLL
	#include "c_baseplayer.h"
#else
	#include "player.h"
	#include "checksum_crc.h"
	#include "hidden/hdn_hiddenselection.h"
#endif


#ifdef CLIENT_DLL
	#define CSDKGameRules C_SDKGameRules
	#define CSDKGameRulesProxy C_SDKGameRulesProxy
#endif


// Hidden: the game type, picked from the map name prefix in InstallGameRules.
// REBUILD: the names are ours, the original code uses the numbers.
enum
{
	HDN_GAMETYPE_HIDDEN = 1,
	HDN_GAMETYPE_OVERRUN,			// ovr_ maps
	HDN_GAMETYPE_MARINE_TUTORIAL,	// mtr_ maps
	HDN_GAMETYPE_HIDDEN_TUTORIAL,	// htr_ maps
	HDN_GAMETYPE_EVACUATION,		// obj_ maps
};

// Hidden: one player model per IRIS character, each taken by at most one player.
#define HDN_MAX_CHARACTERS		9	// REBUILD: name


class CSDKGameRulesProxy : public CGameRulesProxy
{
public:
	DECLARE_CLASS( CSDKGameRulesProxy, CGameRulesProxy );
	DECLARE_NETWORKCLASS();
};


class CSDKGameRules : public CTeamplayRules
{
public:
	DECLARE_CLASS( CSDKGameRules, CTeamplayRules );

	virtual bool	ShouldCollide( int collisionGroup0, int collisionGroup1 );

	virtual int		PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget );
	virtual bool	IsTeamplay( void ) { return false;	}

	// Seconds left in the round (0 before the first round); the round timer shows it
	int				GetRoundTimerRemain( void );

#ifdef CLIENT_DLL

	DECLARE_CLIENTCLASS_NOBASE(); // This makes datatables able to access our private vars.

	// The team menu disables the IRIS characters other players have taken
	bool			IsCharacterTaken( int iCharacter );	// REBUILD: name

#else

	DECLARE_SERVERCLASS_NOBASE(); // This makes datatables able to access our private vars.

	CSDKGameRules();
	virtual ~CSDKGameRules();

	virtual const char *GetGameDescription( void ) { return "Hidden : Source B4"; }

	virtual bool ClientCommand( const char *pcmd, CBaseEntity *pEdict );
	virtual void Think();

	virtual bool ClientConnected( edict_t *pEntity, const char *pszName, const char *pszAddress, char *reject, int maxrejectlen );
	virtual void ClientDisconnected( edict_t *pClient );

	virtual float FlPlayerFallDamage( CBasePlayer *pPlayer );
	virtual bool  FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker );
	virtual bool FPlayerCanRespawn( CBasePlayer *pPlayer );

	virtual void PlayerKilled( CBasePlayer *pVictim, const CTakeDamageInfo &info );
	virtual void DeathNotice( CBasePlayer *pVictim, const CTakeDamageInfo &info );

	virtual const char *GetChatPrefix( bool bTeamOnly, CBasePlayer *pPlayer );
	virtual const char *GetDeadChatPrefix( bool bTeamOnly, CBasePlayer *pPlayer );
	virtual const char *GetChatLocation( bool bTeamOnly, CBasePlayer *pPlayer );

	virtual void GoToIntermission( void );

	// Hidden: round end conditions
	virtual void HiddenDisconnected( CBasePlayer *pPlayer );
	virtual void MarineDisconnected( CBasePlayer *pPlayer );
	virtual bool IRISWins( void );
	virtual bool HiddenWins( void );

	virtual void RadiusDamage( const CTakeDamageInfo &info, const Vector &vecSrcIn, float flRadius, int iClassIgnore );

	// The game rules think in stages (intermission, round start, the round itself, round
	// end); m_pfnThink is the current stage and m_pfnRestartThink the one a new round starts with.
	typedef void (CSDKGameRules::*GAMETHINKPTR)( void );
	GAMETHINKPTR	GameThinkSet( GAMETHINKPTR func );
	GAMETHINKPTR	RestartThinkSet( GAMETHINKPTR func );

	void			GameThink( void );
	void			RoundStartThink( void );
	void			RoundEndThink( void );
	void			MaterialCheckThink( void );
	void			IntermissionThink( void );
	void			GameOverThink( void );

	void			RestartRound( void );
	void			CleanUpMap( void );
	void			SetSpawns( void );
	bool			IsRoundOver( void );

	// Count the players left; each also stores the count in its member
	bool			CountMarines( void );
	bool			CountHiddens( void );
	bool			TotalPlayers( void );

	CBasePlayer		*GetHidden( void );
	void			SetGameType( int iGameType );

private:

	void RadiusDamage( const CTakeDamageInfo &info, const Vector &vecSrcIn, float flRadius, int iClassIgnore, bool bIgnoreWorld );

#endif

public:
	CNetworkVar( float, m_fStart );			// when the round started, -1 before the first round
	CNetworkVar( int, m_iDuration );		// round length in seconds
	CNetworkArray( bool, m_bPlayer, HDN_MAX_CHARACTERS );	// which IRIS characters are taken

#ifndef CLIENT_DLL
protected:
	// REBUILD: the names below are ours.
	GAMETHINKPTR		m_pfnThink;
	GAMETHINKPTR		m_pfnRestartThink;
	CHiddenSelector		m_HiddenSelector;
	float				m_flIntermissionEnd;	// CMultiplayRules has its own m_flIntermissionEndTime
	bool				m_bLastRound;			// mp_timelimit has passed, this is the last round
	int					m_iGameType;
	int					m_iUnknown284;			// REBUILD: set to 1 in the constructor, no use found
	CRC32_t				m_VmtCRC;				// the server's copies of the materials clients must not change
	CRC32_t				m_BumpCRC;
	bool				m_bUnknown290;			// REBUILD: cleared in the constructor, no use found
	int					m_iMarines;				// living IRIS players
	int					m_iHiddens;
	int					m_iTotalPlayers;		// players with a class on a team
#endif
};

//-----------------------------------------------------------------------------
// Gets us at the team fortress game rules
//-----------------------------------------------------------------------------

inline CSDKGameRules* SDKGameRules()
{
	return static_cast<CSDKGameRules*>(g_pGameRules);
}


#endif // SDK_GAMERULES_H
