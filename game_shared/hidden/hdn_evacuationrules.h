//========= Hidden: Source ============//
//
// Purpose: Evacuation game rules (obj_ maps). A copy of the OverRun rules with the
//			OverRun kill and win handling taken out; the round plays like a Hidden round.
//
//=====================================//

#ifndef HDN_EVACUATIONRULES_H
#define HDN_EVACUATIONRULES_H

#ifdef _WIN32
#pragma once
#endif


#include "sdk_gamerules.h"


#ifdef CLIENT_DLL
	#define CEvacuationGameRules C_EvacuationGameRules
	#define CEvacuationGameRulesProxy C_EvacuationGameRulesProxy
#endif


class CEvacuationGameRulesProxy : public CSDKGameRulesProxy
{
public:
	DECLARE_CLASS( CEvacuationGameRulesProxy, CSDKGameRulesProxy );
	DECLARE_NETWORKCLASS();
};


class CEvacuationGameRules : public CSDKGameRules
{
public:
	DECLARE_CLASS( CEvacuationGameRules, CSDKGameRules );

	virtual bool	ShouldCollide( int collisionGroup0, int collisionGroup1 );

	virtual int		PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget );
	virtual bool	IsTeamplay( void ) { return false;	}

#ifdef CLIENT_DLL

	DECLARE_CLIENTCLASS_NOBASE(); // This makes datatables able to access our private vars.

#else

	DECLARE_SERVERCLASS_NOBASE(); // This makes datatables able to access our private vars.

	CEvacuationGameRules();
	virtual ~CEvacuationGameRules();

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
// Gets us at the Evacuation game rules
//-----------------------------------------------------------------------------

inline CEvacuationGameRules* EvacuationGameRules()
{
	return static_cast<CEvacuationGameRules*>(g_pGameRules);
}


#endif // HDN_EVACUATIONRULES_H
