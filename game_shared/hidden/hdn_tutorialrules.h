//========= Hidden: Source ============//
//
// Purpose: Tutorial game rules (mtr_ and htr_ maps). A single player server that runs
//			tutorial.cfg for the player and then plays like a Hidden round.
//
//=====================================//

#ifndef HDN_TUTORIALRULES_H
#define HDN_TUTORIALRULES_H

#ifdef _WIN32
#pragma once
#endif


#include "sdk_gamerules.h"


#ifdef CLIENT_DLL
	#define CTutorialGameRules C_TutorialGameRules
	#define CTutorialGameRulesProxy C_TutorialGameRulesProxy
#endif


class CTutorialGameRulesProxy : public CSDKGameRulesProxy
{
public:
	DECLARE_CLASS( CTutorialGameRulesProxy, CSDKGameRulesProxy );
	DECLARE_NETWORKCLASS();
};


class CTutorialGameRules : public CSDKGameRules
{
public:
	DECLARE_CLASS( CTutorialGameRules, CSDKGameRules );

	virtual bool	ShouldCollide( int collisionGroup0, int collisionGroup1 );

	virtual int		PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget );
	virtual bool	IsTeamplay( void ) { return false;	}

#ifdef CLIENT_DLL

	DECLARE_CLIENTCLASS_NOBASE(); // This makes datatables able to access our private vars.

#else

	DECLARE_SERVERCLASS_NOBASE(); // This makes datatables able to access our private vars.

	CTutorialGameRules();
	virtual ~CTutorialGameRules();

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

	void			ConfigThink( void );
	void			NullThink( void );
#endif
};

//-----------------------------------------------------------------------------
// Gets us at the Tutorial game rules
//-----------------------------------------------------------------------------

inline CTutorialGameRules* TutorialGameRules()
{
	return static_cast<CTutorialGameRules*>(g_pGameRules);
}


#endif // HDN_TUTORIALRULES_H
