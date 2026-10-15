//========= Hidden: Source ============//
//
// Purpose: OverRun game rules (ovr_ maps). Dead marines come back as Hiddens after
//			hdn_deathwait seconds; the last marine standing must survive hdn_survivaltime
//			seconds to win the round for I.R.I.S.
//
//=====================================//

#ifndef HDN_OVERRUNRULES_H
#define HDN_OVERRUNRULES_H

#ifdef _WIN32
#pragma once
#endif


#include "sdk_gamerules.h"


#ifdef CLIENT_DLL
	#define COverRunGameRules C_OverRunGameRules
	#define COverRunGameRulesProxy C_OverRunGameRulesProxy
#endif


class COverRunGameRulesProxy : public CSDKGameRulesProxy
{
public:
	DECLARE_CLASS( COverRunGameRulesProxy, CSDKGameRulesProxy );
	DECLARE_NETWORKCLASS();
};


class COverRunGameRules : public CSDKGameRules
{
public:
	DECLARE_CLASS( COverRunGameRules, CSDKGameRules );

	virtual bool	ShouldCollide( int collisionGroup0, int collisionGroup1 );

	virtual int		PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget );
	virtual bool	IsTeamplay( void ) { return false;	}

#ifdef CLIENT_DLL

	DECLARE_CLIENTCLASS_NOBASE(); // This makes datatables able to access our private vars.

#else

	DECLARE_SERVERCLASS_NOBASE(); // This makes datatables able to access our private vars.

	COverRunGameRules();
	virtual ~COverRunGameRules();

	virtual const char *GetGameDescription( void ) { return "Hidden : Source B3"; }

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

	virtual void HiddenDisconnected( CBasePlayer *pPlayer );
	virtual void MarineDisconnected( CBasePlayer *pPlayer );
	virtual bool IRISWins( void );
	virtual bool HiddenWins( void );

	virtual void RadiusDamage( const CTakeDamageInfo &info, const Vector &vecSrcIn, float flRadius, int iClassIgnore );

	void			OverRestartThink( void );
	void			StartSurvivalThink( void );
	void			SurvivalThink( void );

	// Brings back the dead marines whose wait is over, as Hiddens
	void			RespawnHiddens( void );
	// Picks the last living marine as the survivor
	void			SetSurvivingMarine( void );

protected:
	// REBUILD: the names below are ours.
	CBasePlayer					*m_pSurvivor;		// the last marine, NULL until only one is left
	CUtlVector<CBasePlayer *>	m_DeadPlayers;		// waiting to respawn as Hiddens
	int							m_iSurvivalTime;	// seconds the survivor still has to hold out
#endif
};

//-----------------------------------------------------------------------------
// Gets us at the OverRun game rules
//-----------------------------------------------------------------------------

inline COverRunGameRules* OverRunGameRules()
{
	return static_cast<COverRunGameRules*>(g_pGameRules);
}


#endif // HDN_OVERRUNRULES_H
