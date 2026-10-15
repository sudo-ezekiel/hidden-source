//========= Hidden: Source ============//
//
// Purpose: Tutorial game rules
//
//=====================================//

#include "cbase.h"
#include "hidden/hdn_tutorialrules.h"
#include "sdk_shareddefs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


REGISTER_GAMERULES_CLASS( CTutorialGameRules );


BEGIN_NETWORK_TABLE_NOBASE( CTutorialGameRules, DT_TutorialGameRules )
END_NETWORK_TABLE()


LINK_ENTITY_TO_CLASS( tutorial_gamerules, CTutorialGameRulesProxy );
IMPLEMENT_NETWORKCLASS_ALIASED( TutorialGameRulesProxy, DT_TutorialGameRulesProxy )


#ifdef CLIENT_DLL
	void RecvProxy_TutorialGameRules( const RecvProp *pProp, void **pOut, void *pData, int objectID )
	{
		CTutorialGameRules *pRules = TutorialGameRules();
		Assert( pRules );
		*pOut = pRules;
	}

	BEGIN_RECV_TABLE( CTutorialGameRulesProxy, DT_TutorialGameRulesProxy )
		RecvPropDataTable( "tutorial_gamerules_data", 0, 0, &REFERENCE_RECV_TABLE( DT_TutorialGameRules ), RecvProxy_TutorialGameRules )
	END_RECV_TABLE()
#else
	void *SendProxy_TutorialGameRules( const SendProp *pProp, const void *pStructBase, const void *pData, CSendProxyRecipients *pRecipients, int objectID )
	{
		CTutorialGameRules *pRules = TutorialGameRules();
		Assert( pRules );
		pRecipients->SetAllRecipients();
		return pRules;
	}

	BEGIN_SEND_TABLE( CTutorialGameRulesProxy, DT_TutorialGameRulesProxy )
		SendPropDataTable( "tutorial_gamerules_data", 0, &REFERENCE_SEND_TABLE( DT_TutorialGameRules ), SendProxy_TutorialGameRules )
	END_SEND_TABLE()
#endif


#ifndef CLIENT_DLL

CTutorialGameRules::CTutorialGameRules()
{
	// The tutorial is single player
	gpGlobals->maxClients = 1;

	RestartThinkSet( static_cast<GAMETHINKPTR>( &CTutorialGameRules::NullThink ) );
	GameThinkSet( static_cast<GAMETHINKPTR>( &CTutorialGameRules::ConfigThink ) );
}

CTutorialGameRules::~CTutorialGameRules()
{
}

bool CTutorialGameRules::ClientCommand( const char *pcmd, CBaseEntity *pEdict )
{
	// Start the round as soon as the player picks a character
	if ( pEdict->IsPlayer() && FStrEq( pcmd, "enter" ) )
		m_flIntermissionEndTime = gpGlobals->curtime;

	return BaseClass::ClientCommand( pcmd, pEdict );
}

void CTutorialGameRules::RadiusDamage( const CTakeDamageInfo &info, const Vector &vecSrcIn, float flRadius, int iClassIgnore )
{
	BaseClass::RadiusDamage( info, vecSrcIn, flRadius, iClassIgnore );
}

void CTutorialGameRules::Think()
{
	BaseClass::Think();
}

//-----------------------------------------------------------------------------
// Purpose: Run the tutorial settings for the player, then wait for him to join
//-----------------------------------------------------------------------------
void CTutorialGameRules::ConfigThink( void )
{
	CBasePlayer *pPlayer = UTIL_PlayerByIndex( 1 );
	if ( pPlayer )
		engine->ClientCommand( pPlayer->edict(), "exec tutorial.cfg" );

	GoToIntermission();
}

void CTutorialGameRules::NullThink( void )
{
}

bool CTutorialGameRules::IRISWins( void )
{
	return BaseClass::IRISWins();
}

bool CTutorialGameRules::HiddenWins( void )
{
	return BaseClass::HiddenWins();
}

void CTutorialGameRules::PlayerKilled( CBasePlayer *pVictim, const CTakeDamageInfo &info )
{
	BaseClass::PlayerKilled( pVictim, info );
}

void CTutorialGameRules::DeathNotice( CBasePlayer *pVictim, const CTakeDamageInfo &info )
{
	BaseClass::DeathNotice( pVictim, info );
}

bool CTutorialGameRules::FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker )
{
	return BaseClass::FPlayerCanTakeDamage( pPlayer, pAttacker );
}

bool CTutorialGameRules::FPlayerCanRespawn( CBasePlayer *pPlayer )
{
	return BaseClass::FPlayerCanRespawn( pPlayer );
}

void CTutorialGameRules::HiddenDisconnected( CBasePlayer *pPlayer )
{
	BaseClass::HiddenDisconnected( pPlayer );
}

void CTutorialGameRules::MarineDisconnected( CBasePlayer *pPlayer )
{
	BaseClass::MarineDisconnected( pPlayer );
}

void CTutorialGameRules::ClientDisconnected( edict_t *pClient )
{
	BaseClass::ClientDisconnected( pClient );
}

bool CTutorialGameRules::ClientConnected( edict_t *pEntity, const char *pszName, const char *pszAddress, char *reject, int maxrejectlen )
{
	return BaseClass::ClientConnected( pEntity, pszName, pszAddress, reject, maxrejectlen );
}

float CTutorialGameRules::FlPlayerFallDamage( CBasePlayer *pPlayer )
{
	return BaseClass::FlPlayerFallDamage( pPlayer );
}

#endif

bool CTutorialGameRules::ShouldCollide( int collisionGroup0, int collisionGroup1 )
{
	return BaseClass::ShouldCollide( collisionGroup0, collisionGroup1 );
}

#ifndef CLIENT_DLL

const char *CTutorialGameRules::GetDeadChatPrefix( bool bTeamOnly, CBasePlayer *pPlayer )
{
	return BaseClass::GetDeadChatPrefix( bTeamOnly, pPlayer );
}

const char *CTutorialGameRules::GetChatLocation( bool bTeamOnly, CBasePlayer *pPlayer )
{
	return BaseClass::GetChatLocation( bTeamOnly, pPlayer );
}

const char *CTutorialGameRules::GetChatPrefix( bool bTeamOnly, CBasePlayer *pPlayer )
{
	return BaseClass::GetChatPrefix( bTeamOnly, pPlayer );
}

#endif

int CTutorialGameRules::PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget )
{
	return BaseClass::PlayerRelationship( pPlayer, pTarget );
}
