//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: The TF Game rules
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "sdk_gamerules.h"
#include "sdk_shareddefs.h"
#include "ammodef.h"
#include "KeyValues.h"
#include "weapon_sdkbase.h"


#ifdef CLIENT_DLL


#else

	#include "voice_gamemgr.h"
	#include "team.h"
	#include "mapentities.h"
	#include "hidden/hdn_mapfilter.h"
	#include "ilagcompensationmanager.h"
	#include "checksum_crc.h"

#endif


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


ConVar mp_roundtime( "mp_roundtime", "240", FCVAR_REPLICATED | FCVAR_NOTIFY, "length of a round in seconds" );
ConVar hdn_hiddenrounds( "hdn_hiddenrounds", "5", FCVAR_REPLICATED | FCVAR_NOTIFY, "0 is unlimited" );
ConVar hdn_radio_limit( "hdn_radio_limit", "5", FCVAR_REPLICATED | FCVAR_NOTIFY, "time limit, in seconds, between radio messages / taunts", true, 0, false, 0, NULL );
ConVar hdn_deathnotices( "hdn_deathnotices", "0", FCVAR_REPLICATED | FCVAR_NOTIFY, "display death notices" );
ConVar hdn_jointime( "hdn_jointime", "15", FCVAR_REPLICATED | FCVAR_NOTIFY, "delay, in seconds, before attempting to start the first round after a map change" );

#ifndef CLIENT_DLL

ConVar hdn_limitbombs( "hdn_limitbombs", "1", FCVAR_REPLICATED | FCVAR_NOTIFY, "limits the number of hidden bombs if enabled" );

extern ConVar mp_chattime;

LINK_ENTITY_TO_CLASS( info_marine_spawn, CPointEntity );
LINK_ENTITY_TO_CLASS( info_hidden_spawn, CPointEntity );
#endif

REGISTER_GAMERULES_CLASS( CSDKGameRules );


BEGIN_NETWORK_TABLE_NOBASE( CSDKGameRules, DT_SDKGameRules )
#ifdef CLIENT_DLL
	RecvPropFloat( RECVINFO( m_fStart ) ),
	RecvPropInt( RECVINFO( m_iDuration ) ),
	RecvPropArray3( RECVINFO_ARRAY( m_bPlayer ), RecvPropBool( RECVINFO( m_bPlayer[0] ) ) ),
#else
	SendPropFloat( SENDINFO( m_fStart ) ),
	SendPropInt( SENDINFO( m_iDuration ) ),
	SendPropArray( SendPropBool( SENDINFO_ARRAY( m_bPlayer ) ), m_bPlayer ),
#endif
END_NETWORK_TABLE()


LINK_ENTITY_TO_CLASS( sdk_gamerules, CSDKGameRulesProxy );
IMPLEMENT_NETWORKCLASS_ALIASED( SDKGameRulesProxy, DT_SDKGameRulesProxy )


#ifdef CLIENT_DLL
	void RecvProxy_SDKGameRules( const RecvProp *pProp, void **pOut, void *pData, int objectID )
	{
		CSDKGameRules *pRules = SDKGameRules();
		Assert( pRules );
		*pOut = pRules;
	}

	BEGIN_RECV_TABLE( CSDKGameRulesProxy, DT_SDKGameRulesProxy )
		RecvPropDataTable( "sdk_gamerules_data", 0, 0, &REFERENCE_RECV_TABLE( DT_SDKGameRules ), RecvProxy_SDKGameRules )
	END_RECV_TABLE()
#else
	void *SendProxy_SDKGameRules( const SendProp *pProp, const void *pStructBase, const void *pData, CSendProxyRecipients *pRecipients, int objectID )
	{
		CSDKGameRules *pRules = SDKGameRules();
		Assert( pRules );
		pRecipients->SetAllRecipients();
		return pRules;
	}

	BEGIN_SEND_TABLE( CSDKGameRulesProxy, DT_SDKGameRulesProxy )
		SendPropDataTable( "sdk_gamerules_data", 0, &REFERENCE_SEND_TABLE( DT_SDKGameRules ), SendProxy_SDKGameRules )
	END_SEND_TABLE()
#endif


#ifdef CLIENT_DLL


#else

	// --------------------------------------------------------------------------------------------------- //
	// Voice helper
	// --------------------------------------------------------------------------------------------------- //

	class CVoiceGameMgrHelper : public IVoiceGameMgrHelper
	{
	public:
		virtual bool		CanPlayerHearPlayer( CBasePlayer *pListener, CBasePlayer *pTalker )
		{
			// Dead players can only be heard by other dead team mates
			if ( pTalker->IsAlive() == false )
			{
				if ( pListener->IsAlive() == false )
					return ( pListener->InSameTeam( pTalker ) );

				return false;
			}

			return ( pListener->InSameTeam( pTalker ) );
		}
	};
	CVoiceGameMgrHelper g_VoiceGameMgrHelper;
	IVoiceGameMgrHelper *g_pVoiceGameMgrHelper = &g_VoiceGameMgrHelper;



	// --------------------------------------------------------------------------------------------------- //
	// Globals.
	// --------------------------------------------------------------------------------------------------- //

	// NOTE: the indices here must match TEAM_UNASSIGNED, TEAM_SPECTATOR, TEAM_IRIS and TEAM_HIDDEN.
	// REBUILD: the unassigned team really is called "IRIS" in the binary.
	char *sTeamNames[] =
	{
		"IRIS",
		"Spectator",
		"IRIS",
		"Hidden"
	};

	// Entities that survive a round restart; CleanUpMap removes and recreates everything else.
	static const char *s_PreserveEnts[] =
	{
		"ai_network",
		"ai_hint",
		"ambient_generic",
		"overrun_gamerules",
		"tutorial_gamerules",
		"evacuation_gamerules",
		"sdk_gamerules",
		"sdk_team_manager",
		"player_manager",
		"env_soundscape",
		"env_soundscape_proxy",
		"env_soundscape_triggerable",
		"env_sun",
		"env_wind",
		"env_fog_controller",
		"func_brush",
		"func_door",
		"func_wall",
		"func_illusionary",
		"func_rotating",
		"infodecal",
		"info_projecteddecal",
		"info_node",
		"info_target",
		"info_node_hint",
		"info_marine_spawn",
		"info_hidden_spawn",
		"info_spectator",
		"info_map_parameters",
		"keyframe_rope",
		"move_rope",
		"info_ladder",
		"player",
		"point_viewcontrol",
		"scene_manager",
		"shadow_control",
		"sky_camera",
		"soundent",
		"trigger_soundscape",
		"viewmodel",
		"predicted_viewmodel",
		"worldspawn",
		"point_devshot_camera",
		"", // END Marker
		NULL
	};


	// --------------------------------------------------------------------------------------------------- //
	// Global helper functions.
	// --------------------------------------------------------------------------------------------------- //

	// Helper function to parse arguments to player commands.
	const char* FindEngineArg( const char *pName )
	{
		int nArgs = engine->Cmd_Argc();
		for ( int i=1; i < nArgs; i++ )
		{
			if ( stricmp( engine->Cmd_Argv(i), pName ) == 0 )
				return (i+1) < nArgs ? engine->Cmd_Argv(i+1) : "";
		}
		return 0;
	}


	int FindEngineArgInt( const char *pName, int defaultVal )
	{
		const char *pVal = FindEngineArg( pName );
		if ( pVal )
			return atoi( pVal );
		else
			return defaultVal;
	}


	// CRC of a file the clients must not modify; clients report theirs through the
	// "material_check" event and are kicked (materialCRC command) when they differ.
	CRC32_t GetMaterialCRC( const char *filename )
	{
		CRC32_t crc;
		CRC32_Init( &crc );

		char lowercase[256];
		Q_strncpy( lowercase, filename, sizeof( lowercase ) );
		Q_strlower( lowercase );

		int fileSize;
		byte *pBuffer = UTIL_LoadFileForMe( lowercase, &fileSize );
		if ( !fileSize )
			return 0;

		CRC32_ProcessBuffer( &crc, pBuffer, fileSize );
		CRC32_Final( &crc );
		UTIL_FreeFile( pBuffer );
		return crc;
	}


	// World.cpp calls this but we don't use it in SDK.
	void InitBodyQue()
	{
	}


	// --------------------------------------------------------------------------------------------------- //
	// CSDKGameRules implementation.
	// --------------------------------------------------------------------------------------------------- //

	CSDKGameRules::GAMETHINKPTR CSDKGameRules::RestartThinkSet( GAMETHINKPTR func )
	{
		GAMETHINKPTR pOld = m_pfnThink;
		m_pfnRestartThink = func;
		return pOld;
	}

	//-----------------------------------------------------------------------------
	// Purpose: Hidden's player commands: class, loadout and character menus, radio
	//-----------------------------------------------------------------------------
	bool CSDKGameRules::ClientCommand( const char *pcmd, CBaseEntity *pEdict )
	{
		if ( pEdict->IsPlayer() )
		{
			CBasePlayer *pPlayer = static_cast<CBasePlayer *>( pEdict );

			if ( FStrEq( pcmd, "vguicancel" ) )
			{
				return true;
			}
			else if ( FStrEq( pcmd, "changeclass" ) )
			{
				if ( engine->Cmd_Argc() < 2 )
					return false;

				int iClass = atoi( engine->Cmd_Argv( 1 ) );
				if ( iClass < 0 || iClass > 1 )
					iClass = 99;

				pPlayer->ChangeClass( iClass );
				DevMsg( 1, "Selected Player Class [%i]\n", iClass );
			}
			else if ( FStrEq( pcmd, "choosehidden" ) )
			{
				if ( engine->Cmd_Argc() < 2 )
					return false;

				int iNoHidden = atoi( engine->Cmd_Argv( 1 ) );
				if ( iNoHidden < 0 || iNoHidden > 1 )
					iNoHidden = 1;

				if ( iNoHidden == 0 )
				{
					pPlayer->SetNoHidden( false );
					DevMsg( 1, "I Do Want to be the Hidden\n" );
				}
				else
				{
					pPlayer->SetNoHidden( true );
					DevMsg( 1, "I DON'T Want to be the Hidden\n" );
				}
				return true;
			}
			else if ( FStrEq( pcmd, "primary" ) )
			{
				if ( engine->Cmd_Argc() < 2 )
					return false;

				int iPrimary = atoi( engine->Cmd_Argv( 1 ) );
				if ( iPrimary >= 0 && iPrimary <= 3 )
					pPlayer->SetPrimary( iPrimary );

				// 9 is "random" in the menu
				if ( iPrimary == 9 && pPlayer->GetNewPrimary() == -1 )
					pPlayer->SetPrimary( random->RandomInt( 0, 3 ) );

				DevMsg( 1, "Player's Class [%i}\n", pPlayer->GetNewClass() );

				// Support class can't carry weapons 0 and 2
				if ( pPlayer->GetNewClass() == 1 && ( pPlayer->GetNewPrimary() == 0 || pPlayer->GetNewPrimary() == 2 ) )
					pPlayer->SetPrimary( 1 );
			}
			else if ( FStrEq( pcmd, "secondary" ) )
			{
				if ( engine->Cmd_Argc() < 2 )
					return false;

				int iSecondary = atoi( engine->Cmd_Argv( 1 ) );
				if ( iSecondary >= 0 && iSecondary <= 1 )
					pPlayer->SetSecondary( iSecondary );

				if ( iSecondary == 9 && pPlayer->GetNewSecondary() == -1 )
					pPlayer->SetSecondary( random->RandomInt( 0, 1 ) );
			}
			else if ( FStrEq( pcmd, "radio" ) )
			{
				if ( !pPlayer->IsAlive() )
					return false;

				if ( pPlayer->GetRadio() > gpGlobals->curtime )
					return false;

				if ( engine->Cmd_Argc() < 1 )
					return false;

				pPlayer->SetRadio( hdn_radio_limit.GetFloat() );

				int iMessage = atoi( engine->Cmd_Argv( 1 ) );

				// The Hidden taunts
				if ( pPlayer->GetTeamNumber() == TEAM_HIDDEN )
				{
					switch ( iMessage )
					{
					case 0:	pPlayer->EmitSound( "Hidden.BehindYou" );	break;
					case 1:	pPlayer->EmitSound( "Hidden.ImHere" );		break;
					case 2:	pPlayer->EmitSound( "Hidden.ISeeYou" );		break;
					case 3:	pPlayer->EmitSound( "Hidden.LookUp" );		break;
					case 4:	pPlayer->EmitSound( "Hidden.TurnAround" );	break;
					case 5:	pPlayer->EmitSound( "Hidden.OverHere" );	break;
					case 6:	pPlayer->EmitSound( "Hidden.FreshMeat" );	break;
					case 7:	pPlayer->EmitSound( "Hidden.YouAreNext" );	break;
					}
				}

				if ( pPlayer->GetTeamNumber() == TEAM_IRIS )
				{
					// IRIS taunts are spoken to everyone
					switch ( iMessage )
					{
					case 5:
						pPlayer->EmitSound( "IRIS.OneUglyMother" );
						engine->ClientCommand( pPlayer->edict(), "say You are one UGLY mother!\n" );
						return true;
					case 6:
						pPlayer->EmitSound( "IRIS.IfItBleeds" );
						engine->ClientCommand( pPlayer->edict(), "say If it bleeds, we can kill it!\n" );
						return true;
					case 7:
						pPlayer->EmitSound( "IRIS.BringIt" );
						engine->ClientCommand( pPlayer->edict(), "say Bring it!\n" );
						return true;
					}

					// The rest go to the team, and the clients play the radio sound
					IGameEvent *event = gameeventmanager->CreateEvent( "iris_radio" );
					int iReport = random->RandomInt( 0, 2 );

					switch ( iMessage )
					{
					case 0:
						engine->ClientCommand( pPlayer->edict(), "say_team Agent Down\n" );
						break;
					case 1:
						engine->ClientCommand( pPlayer->edict(), "say_team Subject Sighted\n" );
						break;
					case 2:
						engine->ClientCommand( pPlayer->edict(), "say_team Affirmative\n" );
						break;
					case 3:
						if ( pPlayer->GetAmmoRecieved() )
							return true;

						engine->ClientCommand( pPlayer->edict(), "say_team I need ammo!\n" );
						pPlayer->SetAmmoRequest( true );
						break;
					case 4:
						engine->ClientCommand( pPlayer->edict(), "say_team Report In\n" );
						break;
					case 10:
						// Answer to "Report In"
						switch ( iReport )
						{
						case 0:
							engine->ClientCommand( pPlayer->edict(), "say_team Reporting In\n" );
							break;
						case 1:
							engine->ClientCommand( pPlayer->edict(), "say_team Still Here!\n" );
							break;
						case 2:
							engine->ClientCommand( pPlayer->edict(), "say_team Checking In\n" );
							break;
						}
						break;
					}

					if ( event )
					{
						event->SetInt( "message", iMessage );
						event->SetInt( "userid", pPlayer->GetUserID() );
						gameeventmanager->FireEvent( event );
					}
				}

				return true;
			}
			else if ( FStrEq( pcmd, "equip" ) )
			{
				if ( engine->Cmd_Argc() < 2 )
					return false;

				int iEquipment = atoi( engine->Cmd_Argv( 1 ) );
				if ( iEquipment >= 0 && iEquipment <= 4 )
					pPlayer->SetEquipment( iEquipment );

				if ( iEquipment == 9 && pPlayer->GetNewEquipment() == -1 )
					pPlayer->SetEquipment( random->RandomInt( 0, 4 ) );
			}
			else if ( FStrEq( pcmd, "changemarine" ) )
			{
				if ( engine->Cmd_Argc() < 2 )
					return false;

				int iCharacter = atoi( engine->Cmd_Argv( 1 ) );

				// -2: pick one for a player who has none
				if ( iCharacter == -2 )
				{
					if ( pPlayer->m_nSkin_change < 0 )
						iCharacter = HDN_MAX_CHARACTERS;
					else
						return false;
				}

				// Give up the old character
				if ( pPlayer->m_nSkin_change >= 0 && pPlayer->m_nSkin_change < HDN_MAX_CHARACTERS )
					m_bPlayer.Set( pPlayer->m_nSkin_change, false );

				if ( iCharacter >= 0 && iCharacter < HDN_MAX_CHARACTERS )
				{
					if ( m_bPlayer[iCharacter] )
					{
						// Taken, pick another
						iCharacter = HDN_MAX_CHARACTERS;
					}
					else
					{
						pPlayer->m_nSkin_change = iCharacter;
						pPlayer->m_nBody_change = iCharacter;
						m_bPlayer.Set( iCharacter, true );
					}
				}

				// HDN_MAX_CHARACTERS is "any": take the first free one
				if ( iCharacter == HDN_MAX_CHARACTERS )
				{
					bool bFound = false;
					int iFree = -1;
					for ( int i = 0; i < HDN_MAX_CHARACTERS; i++ )
					{
						if ( !m_bPlayer[i] )
						{
							bFound = true;
							iFree = i;
							break;
						}
					}

					if ( bFound == true )
					{
						pPlayer->m_nSkin_change = iFree;
						pPlayer->m_nBody_change = iFree;
						m_bPlayer.Set( iFree, true );
					}
					else
					{
						// All characters are taken
						pPlayer->StartObserverMode( OBS_MODE_FIXED );
					}
				}
			}
			else if ( FStrEq( pcmd, "enter" ) )
			{
				if ( pPlayer->m_nSkin >= 0 && pPlayer->m_nSkin < HDN_MAX_CHARACTERS )
				{
					if ( pPlayer->GetTeamNumber() == TEAM_SPECTATOR )
						pPlayer->ChangeTeam( TEAM_IRIS );
				}
				else
				{
					pPlayer->SetReady( false );
					ClientPrint( pPlayer, HUD_PRINTCENTER, "You will not spawn, use classmenu to pick a valid player" );
				}
				return true;
			}
			else
			{
				if ( FStrEq( pcmd, "spectate" ) )
				{
					// Free the player's place before the base class moves him to spectator
					if ( pPlayer->GetTeamNumber() == TEAM_HIDDEN )
						m_HiddenSelector.ForfeitHidden( pPlayer );

					if ( pPlayer->GetTeamNumber() == TEAM_HIDDEN || pPlayer->GetTeamNumber() == TEAM_IRIS || pPlayer->GetTeamNumber() == TEAM_SPECTATOR )
					{
						if ( pPlayer->m_nSkin_change >= 0 && pPlayer->m_nSkin_change < HDN_MAX_CHARACTERS )
						{
							m_bPlayer.Set( pPlayer->m_nSkin_change, false );
							pPlayer->m_nSkin_change = -1;
						}
					}
				}

				if ( FStrEq( pcmd, "materialCRC" ) )
				{
					// The client's materials don't match the server's
					if ( pPlayer != UTIL_GetListenServerHost() )
						engine->ClientCommand( pPlayer->edict(), "disconnect\n" );
					return true;
				}

				if ( pPlayer->ClientCommand( pcmd ) )
					return true;

				return BaseClass::ClientCommand( pcmd, pEdict );
			}

			pPlayer->SetReady( true );
			return true;
		}

		return BaseClass::ClientCommand( pcmd, pEdict );
	}

	//-----------------------------------------------------------------------------
	// Purpose: Player has just spawned. Equip them.
	//-----------------------------------------------------------------------------

	void CSDKGameRules::RadiusDamage( const CTakeDamageInfo &info, const Vector &vecSrcIn, float flRadius, int iClassIgnore )
	{
		RadiusDamage( info, vecSrcIn, flRadius, iClassIgnore, false );
	}

	// Add the ability to ignore the world trace
	void CSDKGameRules::RadiusDamage( const CTakeDamageInfo &info, const Vector &vecSrcIn, float flRadius, int iClassIgnore, bool bIgnoreWorld )
	{
		CBaseEntity *pEntity = NULL;
		trace_t		tr;
		float		flAdjustedDamage, falloff;
		Vector		vecSpot;
		Vector		vecToTarget;
		Vector		vecEndPos;

		Vector vecSrc = vecSrcIn;

		if ( flRadius )
			falloff = info.GetDamage() / flRadius;
		else
			falloff = 1.0;

		int bInWater = (UTIL_PointContents ( vecSrc ) & MASK_WATER) ? true : false;

		vecSrc.z += 1;// in case grenade is lying on the ground

		// iterate on all entities in the vicinity.
		for ( CEntitySphereQuery sphere( vecSrc, flRadius ); ( pEntity = sphere.GetCurrentEntity() ) != NULL; sphere.NextEntity() )
		{
			if ( pEntity->m_takedamage != DAMAGE_NO )
			{
				// UNDONE: this should check a damage mask, not an ignore
				if ( iClassIgnore != CLASS_NONE && pEntity->Classify() == iClassIgnore )
				{// houndeyes don't hurt other houndeyes with their attack
					continue;
				}

				// blast's don't tavel into water
				if (!bInWater && pEntity->GetWaterLevel() == 3)
					continue;

				// radius damage can only be blocked by the world
				vecSpot = pEntity->BodyTarget( vecSrc );



				bool bHit = false;

				if( bIgnoreWorld )
				{
					vecEndPos = vecSpot;
					bHit = true;
				}
				else
				{
					UTIL_TraceLine( vecSrc, vecSpot, MASK_SOLID_BRUSHONLY, info.GetInflictor(), COLLISION_GROUP_NONE, &tr );

					if (tr.startsolid)
					{
						// if we're stuck inside them, fixup the position and distance
						tr.endpos = vecSrc;
						tr.fraction = 0.0;
					}

					vecEndPos = tr.endpos;

					if( tr.fraction == 1.0 || tr.m_pEnt == pEntity )
					{
						bHit = true;
					}
				}

				if ( bHit )
				{
					// the explosion can 'see' this entity, so hurt them!
					//vecToTarget = ( vecSrc - vecEndPos );
					vecToTarget = ( vecEndPos - vecSrc );

					// decrease damage for an ent that's farther from the bomb.
					flAdjustedDamage = vecToTarget.Length() * falloff;
					flAdjustedDamage = info.GetDamage() - flAdjustedDamage;

					if ( flAdjustedDamage > 0 )
					{
						CTakeDamageInfo adjustedInfo = info;
						adjustedInfo.SetDamage( flAdjustedDamage );

						Vector dir = vecToTarget;
						VectorNormalize( dir );

						// If we don't have a damage force, manufacture one
						if ( adjustedInfo.GetDamagePosition() == vec3_origin || adjustedInfo.GetDamageForce() == vec3_origin )
						{
							CalculateExplosiveDamageForce( &adjustedInfo, dir, vecSrc, 1.5	/* explosion scale! */ );
						}
						else
						{
							// Assume the force passed in is the maximum force. Decay it based on falloff.
							float flForce = adjustedInfo.GetDamageForce().Length() * falloff;
							adjustedInfo.SetDamageForce( dir * flForce );
							adjustedInfo.SetDamagePosition( vecSrc );
						}

						pEntity->TakeDamage( adjustedInfo );

						// Now hit all triggers along the way that respond to damage...
						pEntity->TraceAttackToTriggers( adjustedInfo, vecSrc, vecEndPos, dir );

						// Hidden: the blast blurs the player's view
						CBasePlayer *pPlayer = ToBasePlayer( pEntity );
						if ( pPlayer )
						{
							DevMsg( 1, "Shockwave - sdkgamerules\n" );
							pPlayer->Shockwave( flAdjustedDamage / 12.0f, 5.0f );
						}
					}
				}
			}
		}
	}

	CBasePlayer *CSDKGameRules::GetHidden( void )
	{
		return m_HiddenSelector.GetCurrentHidden();
	}

	void CSDKGameRules::Think()
	{
		if ( m_pfnThink )
			(this->*m_pfnThink)();

		BaseClass::Think();
	}

	void CSDKGameRules::GameOverThink( void )
	{
		if ( gpGlobals->curtime >= m_flIntermissionEndTime )
		{
			DevMsg( 1, "Intermission over, changing levels\n" );
			m_HiddenSelector.ClearHidden();
			ChangeLevel();
		}
	}

	CSDKGameRules::GAMETHINKPTR CSDKGameRules::GameThinkSet( GAMETHINKPTR func )
	{
		GAMETHINKPTR pOld = m_pfnThink;
		m_pfnThink = func;
		return pOld;
	}

	void CSDKGameRules::RoundStartThink( void )
	{
		IGameEvent *event = gameeventmanager->CreateEvent( "game_round_start" );
		if ( event )
			gameeventmanager->FireEvent( event );

		UTIL_RestartAmbientSounds();
		GameThinkSet( &CSDKGameRules::MaterialCheckThink );
	}

	void CSDKGameRules::RoundEndThink( void )
	{
		IGameEvent *event = gameeventmanager->CreateEvent( "game_round_end" );
		if ( event )
			gameeventmanager->FireEvent( event );

		GoToIntermission();
	}

	void CSDKGameRules::MaterialCheckThink( void )
	{
		DevMsg( 1, "Checking Material CRCS, who's been a bad person?\n" );

		IGameEvent *event = gameeventmanager->CreateEvent( "material_check" );
		if ( event )
		{
			event->SetInt( "vmt_CRC", m_vmtCRC );
			event->SetInt( "bump_CRC", m_bumpCRC );
			gameeventmanager->FireEvent( event );
		}

		GameThinkSet( &CSDKGameRules::GameThink );
	}

	void CSDKGameRules::GameThink( void )
	{
		float flTimeLimit = mp_timelimit.GetFloat() * 60;

		if ( !m_bLastRound && flTimeLimit != 0 && gpGlobals->curtime >= flTimeLimit )
		{
			UTIL_ClientPrintAll( HUD_PRINTCENTER, "Last Round" );
			m_bLastRound = true;
		}

		if ( IRISWins() || HiddenWins() )
			GameThinkSet( &CSDKGameRules::RoundEndThink );
	}

	bool CSDKGameRules::HiddenWins( void )
	{
		bool bWins = false;

		if ( m_iMarineCount <= 0 )
		{
			UTIL_ClientPrintAll( HUD_PRINTCENTER, "Hidden Wins" );
			bWins = true;
		}

		return bWins;
	}

	bool CSDKGameRules::CountMarines( void )
	{
		int iMarines = 0;
		for ( int i = 0; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
			if ( !pPlayer || pPlayer->IsDead() )
				continue;

			if ( pPlayer->GetTeamNumber() == TEAM_IRIS )
				iMarines++;
		}

		m_iMarineCount = iMarines;
		return m_iMarineCount > 0;
	}

	bool CSDKGameRules::TotalPlayers( void )
	{
		int iPlayers = 0;
		for ( int i = 0; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
			if ( !pPlayer || pPlayer->GetNewClass() < 0 )
				continue;

			if ( pPlayer->GetTeamNumber() == TEAM_HIDDEN || pPlayer->GetTeamNumber() == TEAM_IRIS || pPlayer->GetTeamNumber() == TEAM_SPECTATOR )
				iPlayers++;
		}

		m_iTotalPlayers = iPlayers;
		return m_iTotalPlayers > 0;
	}

	bool CSDKGameRules::CountHiddens( void )
	{
		int iHiddens = 0;
		for ( int i = 0; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
			if ( pPlayer && pPlayer->GetTeamNumber() == TEAM_HIDDEN )
				iHiddens++;
		}

		m_iHiddenCount = iHiddens;
		return m_iHiddenCount > 0;
	}

	// Spawns the Hidden and up to eight marines; everyone else watches.
	void CSDKGameRules::SetSpawns( void )
	{
		int iMarines = 0;
		for ( int i = 0; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
			if ( !pPlayer )
				continue;

			int iTeam = pPlayer->GetTeamNumber();
			if ( iTeam == TEAM_HIDDEN || ( iTeam == TEAM_IRIS && iMarines++ < 8 ) )
				pPlayer->Spawn();
			else
				pPlayer->StartObserverMode( OBS_MODE_FIXED );
		}
	}

	bool FindInList( const char **s_List, const char *compare )
	{
		int i = 0;
		while ( s_List[i] )
		{
			if ( Q_strcmp( s_List[i], compare ) == 0 )
				return true;

			i++;
		}

		return false;
	}

	void CSDKGameRules::CleanUpMap()
	{
		// Recreate all the map entities from the map data (preserving their indices),
		// then remove everything else except the players.

		// Get rid of all entities except players.
		CBaseEntity *pCur = gEntList.FirstEnt();
		while ( pCur )
		{
			// remove entities that has to be restored on roundrestart (breakables etc)
			if ( !FindInList( s_PreserveEnts, pCur->GetClassname() ) )
			{
				UTIL_Remove( pCur );
			}

			pCur = gEntList.NextEnt( pCur );
		}

		// Really remove the entities so we can have access to their slots below.
		gEntList.CleanupDeleteList();

		// Now reload the map entities.
		class CCSMapEntityFilter : public IMapEntityFilter
		{
		public:
			virtual bool ShouldCreateEntity( const char *pClassname )
			{
				// Don't recreate the preserved entities.
				if ( !FindInList( s_PreserveEnts, pClassname ) )
				{
					return true;
				}
				else
				{
					// Increment our iterator since it's not going to call CreateNextEntity for this ent.
					if ( m_iIterator != g_MapEntityRefs.InvalidIndex() )
						m_iIterator = g_MapEntityRefs.Next( m_iIterator );

					return false;
				}
			}


			virtual CBaseEntity* CreateNextEntity( const char *pClassname )
			{
				if ( m_iIterator == g_MapEntityRefs.InvalidIndex() )
				{
					// This shouldn't be possible. When we loaded the map, it should have used
					// CHDNMapLoadEntityFilter, which should have built the g_MapEntityRefs list
					// with the same list of entities we're referring to here.
					Assert( false );
					return NULL;
				}
				else
				{
					CMapEntityRef &ref = g_MapEntityRefs[m_iIterator];
					m_iIterator = g_MapEntityRefs.Next( m_iIterator );	// Seek to the next entity.

					if ( ref.m_iEdict == -1 || engine->PEntityOfEntIndex( ref.m_iEdict ) )
					{
						// Doh! The entity was delete and its slot was reused.
						// Just use any old edict slot. This case sucks because we lose the baseline.
						return CreateEntityByName( pClassname );
					}
					else
					{
						// Cool, the slot where this entity was is free again (most likely, the entity was
						// freed above). Now create an entity with this specific index.
						return CreateEntityByName( pClassname, ref.m_iEdict );
					}
				}
			}

		public:
			int m_iIterator; // Iterator into g_MapEntityRefs.
		};
		CCSMapEntityFilter filter;
		filter.m_iIterator = g_MapEntityRefs.Head();

		// DO NOT CALL SPAWN ON info_node ENTITIES!

		MapEntity_ParseAllEntities( engine->GetMapEntitiesString(), &filter, true );
	}

	void CSDKGameRules::GoToIntermission( void )
	{
		lagcompensation->StartCompensationReset();

		m_flIntermissionEndTime = gpGlobals->curtime + mp_chattime.GetInt();

		// Nobody can be hurt between rounds
		for ( int i = 0; i <= MAX_PLAYERS; i++ )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
			if ( !pPlayer || !pPlayer->IsAlive() )
				continue;

			pPlayer->SetSafe( true );

			CBaseCombatWeapon *pWeapon = pPlayer->GetActiveWeapon();
			if ( pWeapon )
				pWeapon->SetSafe();
		}

		float flTimeLimit = mp_timelimit.GetFloat() * 60;
		if ( flTimeLimit != 0 && gpGlobals->curtime >= flTimeLimit )
			GameThinkSet( &CSDKGameRules::GameOverThink );
		else
			GameThinkSet( &CSDKGameRules::IntermissionThink );
	}

	void CSDKGameRules::DeathNotice( CBasePlayer *pVictim, const CTakeDamageInfo &info )
	{
		// Work out what killed the player, and send a message to all clients about it
		const char *killer_weapon_name = "world";		// by default, the player is killed by the world
		int killer_ID = 0;

		// Find the killer & the scorer
		CBaseEntity *pInflictor = info.GetInflictor();
		CBaseEntity *pKiller = info.GetAttacker();
		CBasePlayer *pScorer = GetDeathScorer( pKiller, pInflictor );

		// Custom kill type?
		if ( info.GetCustomKill() )
		{
			killer_weapon_name = GetCustomKillString( info );
			if ( pScorer )
			{
				killer_ID = pScorer->GetUserID();
			}
		}
		else
		{
			// Is the killer a client?
			if ( pScorer )
			{
				killer_ID = pScorer->GetUserID();

				if ( pInflictor )
				{
					if ( pInflictor == pScorer )
					{
						// If the inflictor is the killer,  then it must be their current weapon doing the damage
						if ( pScorer->GetActiveWeapon() )
						{
							killer_weapon_name = STRING( pScorer->GetActiveWeapon()->m_iClassname );
						}
					}
					else
					{
						killer_weapon_name = STRING( pInflictor->m_iClassname );  // it's just that easy
					}
				}
			}
			else
			{
				killer_weapon_name = STRING( pInflictor->m_iClassname );
			}

			// strip the NPC_* or weapon_* from the inflictor's classname
			if ( strncmp( killer_weapon_name, "weapon_", 7 ) == 0 )
			{
				killer_weapon_name += 7;
			}
			else if ( strncmp( killer_weapon_name, "NPC_", 8 ) == 0 )
			{
				killer_weapon_name += 8;
			}
			else if ( strncmp( killer_weapon_name, "func_", 5 ) == 0 )
			{
				killer_weapon_name += 5;
			}
			else if ( strncmp( killer_weapon_name, "prop_", 5 ) == 0 )
			{
				killer_weapon_name += 5;
			}
		}

		DevMsg( 1, "svr - weapon : %s\n", killer_weapon_name );

		if ( pKiller->GetTeamNumber() == TEAM_HIDDEN && hdn_deathnotices.GetInt() != 1 )
		{
			// The Hidden's kills are only logged, so the marines don't learn where he is
			CTeam *pKillerTeam = pScorer->GetTeam();
			CTeam *pVictimTeam = pVictim->GetTeam();

			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" killed \"%s<%i><%s><%s>\" with \"%s\"\n",
							pScorer->GetPlayerName(),
							killer_ID,
							pScorer->GetNetworkIDString(),
							pKillerTeam ? pKillerTeam->GetName() : "",
							pVictim->GetPlayerName(),
							pVictim->GetUserID(),
							pVictim->GetNetworkIDString(),
							pVictimTeam ? pVictimTeam->GetName() : "",
							killer_weapon_name
							);
		}
		else
		{
			IGameEvent * event = gameeventmanager->CreateEvent( "player_death" );
			if ( event )
			{
				event->SetInt( "userid", pVictim->GetUserID() );
				event->SetInt( "attacker", killer_ID );
				event->SetString( "weapon", killer_weapon_name );
				gameeventmanager->FireEvent( event );
			}
		}
	}

	bool CSDKGameRules::FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker )
	{
		if ( pAttacker->IsPlayer() )
		{
			CBasePlayer *pPlayerAttacker = ToBasePlayer( pAttacker );

			// Between rounds nobody is hurt
			if ( pPlayer->GetSafe() )
				return false;

			if ( pPlayerAttacker->GetTeamNumber() == pPlayer->GetTeamNumber() && friendlyfire.GetInt() == 0 )
				return false;
		}

		return BaseClass::FPlayerCanTakeDamage( pPlayer, pAttacker );
	}

	// Nobody respawns during a round
	bool CSDKGameRules::FPlayerCanRespawn( CBasePlayer *pPlayer )
	{
		return false;
	}

	void CSDKGameRules::HiddenDisconnected( CBasePlayer *pPlayer )
	{
		CountHiddens();
		m_iHiddenCount--;

		if ( m_iHiddenCount <= 0 )
		{
			DevMsg( 1, "Hidden Disconnected!\n" );
			m_HiddenSelector.ForfeitHidden( pPlayer );
		}
	}

	void CSDKGameRules::MarineDisconnected( CBasePlayer *pPlayer )
	{
		if ( pPlayer->IsAlive() )
			m_iMarineCount--;
	}

	void CSDKGameRules::ClientDisconnected( edict_t *pClient )
	{
		if ( pClient )
		{
			CBasePlayer *pPlayer = (CBasePlayer *)CBaseEntity::Instance( pClient );
			if ( pPlayer )
			{
				switch ( pPlayer->GetTeamNumber() )
				{
				case TEAM_HIDDEN:
					HiddenDisconnected( pPlayer );
					break;
				case TEAM_IRIS:
					MarineDisconnected( pPlayer );
					break;
				}

				// Free the player's character
				if ( pPlayer->m_nBody_change >= 0 && pPlayer->m_nBody_change < HDN_MAX_CHARACTERS )
					m_bPlayer.Set( pPlayer->m_nBody_change, false );
			}
		}

		BaseClass::ClientDisconnected( pClient );
	}

	bool CSDKGameRules::ClientConnected( edict_t *pEntity, const char *pszName, const char *pszAddress, char *reject, int maxrejectlen )
	{
		return BaseClass::ClientConnected( pEntity, pszName, pszAddress, reject, maxrejectlen );
	}

#endif

int CSDKGameRules::GetRoundTimerRemain( void )
{
	if ( m_fStart < 0 )
		return 0;

	return m_iDuration - (int)( gpGlobals->curtime - m_fStart );
}

#ifdef CLIENT_DLL

bool CSDKGameRules::IsCharacterTaken( int iCharacter )
{
	return m_bPlayer[iCharacter];
}

#else

	bool CSDKGameRules::IsRoundOver( void )
	{
		bool bRoundOver = false;

		if ( m_fStart < 0 || m_iDuration - (int)( gpGlobals->curtime - m_fStart ) <= 0 )
		{
			DevMsg( 1, "round timer ended\n" );
			bRoundOver = true;

			// The Hidden ran out of time: he loses his place, and the surviving marines score
			m_HiddenSelector.ForfeitHidden( m_HiddenSelector.GetCurrentHidden() );

			for ( int i = 0; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
				if ( !pPlayer )
					continue;

				if ( pPlayer->IsAlive() && pPlayer->GetTeamNumber() == TEAM_IRIS )
					pPlayer->IncrementFragCount( 1 );
			}
		}

		return bRoundOver;
	}

	bool CSDKGameRules::IRISWins( void )
	{
		bool bWins = false;

		if ( m_iHiddenCount <= 0 || IsRoundOver() )
		{
			UTIL_ClientPrintAll( HUD_PRINTCENTER, "I.R.I.S. Wins" );
			bWins = true;
		}

		return bWins;
	}

#endif


bool CSDKGameRules::ShouldCollide( int collisionGroup0, int collisionGroup1 )
{
	if ( collisionGroup0 > collisionGroup1 )
	{
		// swap so that lowest is always first
		swap(collisionGroup0,collisionGroup1);
	}

	//Don't stand on COLLISION_GROUP_WEAPON
	if( collisionGroup0 == COLLISION_GROUP_PLAYER_MOVEMENT &&
		collisionGroup1 == COLLISION_GROUP_WEAPON )
	{
		return false;
	}

	// The Hidden doesn't bump into weapons
	if ( collisionGroup0 == COLLISION_GROUP_WEAPON &&
		collisionGroup1 == COLLISION_GROUP_HIDDEN )
	{
		return false;
	}

	// Marine clips only block players
	if ( collisionGroup0 == COLLISION_GROUP_NONE &&
		collisionGroup1 == COLLISION_GROUP_MARINECLIP )
	{
		return false;
	}

	return BaseClass::ShouldCollide( collisionGroup0, collisionGroup1 );
}


//-----------------------------------------------------------------------------
// Purpose: Init CS ammo definitions
//-----------------------------------------------------------------------------

// shared ammo definition
// JAY: Trying to make a more physical bullet response
#define BULLET_MASS_GRAINS_TO_LB(grains)	(0.002285*(grains)/16.0f)
#define BULLET_MASS_GRAINS_TO_KG(grains)	lbs2kg(BULLET_MASS_GRAINS_TO_LB(grains))

// exaggerate all of the forces, but use real numbers to keep them consistent
#define BULLET_IMPULSE_EXAGGERATION			1

// convert a velocity in ft/sec and a mass in grains to an impulse in kg in/s
#define BULLET_IMPULSE(grains, ftpersec)	((ftpersec)*12*BULLET_MASS_GRAINS_TO_KG(grains)*BULLET_IMPULSE_EXAGGERATION)


CAmmoDef* GetAmmoDef()
{
	static CAmmoDef def;
	static bool bInitted = false;

	if ( !bInitted )
	{
		bInitted = true;

		def.AddAmmoType( AMMO_GRENADE,		DMG_BLAST,					TRACER_LINE,			0, 0, 10, 75, 0 );
		def.AddAmmoType( AMMO_BULLETS,		DMG_BULLET,					TRACER_LINE_AND_WHIZ,	0, 0, 2, 2345, 0 );
		def.AddAmmoType( "AMMO_556",		DMG_BULLET,					TRACER_LINE_AND_WHIZ,	0, 0, 2, 2345, 0 );
		def.AddAmmoType( "AMMO_PISTOL",		DMG_BULLET,					TRACER_LINE,			0, 0, 3, 2000, 0 );
		def.AddAmmoType( "AMMO_9MM",		DMG_BULLET,					TRACER_LINE,			0, 0, 3, 2000, 0 );
		def.AddAmmoType( "AMMO_BUCKSHOT",	DMG_BULLET | DMG_BUCKSHOT,	TRACER_LINE,			0, 0, 38, 2600, 0 );
		def.AddAmmoType( "XBowBolt",		DMG_BULLET | DMG_POISON,	TRACER_LINE,			0, 0, 4, 75, 0 );
		def.AddAmmoType( "sonic",			DMG_BLAST,					TRACER_LINE,			0, 0, 10, 75, 0 );
	}

	return &def;
}


#ifndef CLIENT_DLL

// Chat prefixes name the speaker's team; dead players' chat says so.
const char *CSDKGameRules::GetDeadChatPrefix( bool bTeamOnly, CBasePlayer *pPlayer )
{
	const char *pszPrefix = NULL;

	if ( !pPlayer )	// dedicated server output
		pszPrefix = "";
	else if ( pPlayer->GetTeamNumber() == TEAM_HIDDEN )
		pszPrefix = "(Hidden)";
	else if ( pPlayer->GetTeamNumber() == TEAM_IRIS )
		pszPrefix = "(IRIS)";
	else
		pszPrefix = "";

	return pszPrefix;
}

// Marines' chat says where they are
const char *CSDKGameRules::GetChatLocation( bool bTeamOnly, CBasePlayer *pPlayer )
{
	if ( pPlayer && pPlayer->GetTeamNumber() == TEAM_IRIS )
		return pPlayer->GetCurrentLocation();

	return "";
}

const char *CSDKGameRules::GetChatPrefix( bool bTeamOnly, CBasePlayer *pPlayer )
{
	if ( !pPlayer )
		return "";

	if ( !pPlayer->IsAlive() )
		return "(Dead)";

	if ( pPlayer->GetTeamNumber() == TEAM_HIDDEN )
		return "(Hidden)";

	if ( pPlayer->GetTeamNumber() == TEAM_IRIS )
		return "(IRIS)";

	return "";
}

void CSDKGameRules::SetGameType( int iGameType )
{
	m_iGameType = iGameType;
}

#endif

//-----------------------------------------------------------------------------
// Purpose: Find the relationship between players (teamplay vs. deathmatch)
//-----------------------------------------------------------------------------
int CSDKGameRules::PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget )
{
#ifndef CLIENT_DLL
	// half life multiplay has a simple concept of Player Relationships.
	// you are either on another player's team, or you are not.
	if ( !pPlayer || !pTarget || !pTarget->IsPlayer() || IsTeamplay() == false )
		return GR_NOTTEAMMATE;

	if ( (*GetTeamID(pPlayer) != '\0') && (*GetTeamID(pTarget) != '\0') && !stricmp( GetTeamID(pPlayer), GetTeamID(pTarget) ) )
		return GR_TEAMMATE;

#endif

	return GR_NOTTEAMMATE;
}

#ifndef CLIENT_DLL

CSDKGameRules::CSDKGameRules()
{
	// Create the team managers
	for ( int i = 0; i < ARRAYSIZE( sTeamNames ); i++ )
	{
		CTeam *pTeam = static_cast<CTeam*>(CreateEntityByName( "sdk_team_manager" ));
		pTeam->Init( sTeamNames[i], i );

		g_Teams.AddToTail( pTeam );
	}

	char szMapName[256];
	Q_FileBase( STRING( gpGlobals->mapname ), szMapName, sizeof( szMapName ) - 1 );	// REBUILD: unused

	m_iGameType = HDN_GAMETYPE_HIDDEN;
	m_fStart = -1;
	m_iDuration = 0;

	CreateEntityByName( "sdk_gamerules" );

	m_bLastRound = false;
	m_iMarineCount = -1;
	m_iHiddenCount = -1;
	m_iTotalPlayers = 0;
	m_flIntermissionEndTime = -1;
	m_bTimeout = false;
	m_iHiddenRounds = 1;

	for ( int i = 0; i < HDN_MAX_CHARACTERS; i++ )
		m_bPlayer.Set( i, false );

	m_vmtCRC = GetMaterialCRC( "materials/models/manor/mn_tapestry.vmt" );
	m_bumpCRC = GetMaterialCRC( "materials/models/manor/mn_tapestry_dx7.vmt" );

	// Wait for players before the first round
	RestartThinkSet( &CSDKGameRules::RoundStartThink );
	GoToIntermission();
	m_flIntermissionEndTime = gpGlobals->curtime + hdn_jointime.GetInt();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CSDKGameRules::~CSDKGameRules()
{
	// Note, don't delete each team since they are in the gEntList and will
	// automatically be deleted from there, instead.
	g_Teams.Purge();
}

void CSDKGameRules::RestartRound( void )
{
	m_iMarineCount = 0;

	CBaseEntity *pEntity = gEntList.FirstEnt();	// REBUILD: unused, but the call is in the binary

	CBasePlayer *pHidden = m_HiddenSelector.SelectHidden( m_iGameType, hdn_hiddenrounds.GetInt() );
	if ( !pHidden )
	{
		// Not enough players yet, try again after another intermission
		GoToIntermission();
		return;
	}

	CleanUpMap();
	UTIL_ClientPrintAll( HUD_PRINTCENTER, "Round restarting..." );

	for ( int i = 0; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( !pPlayer )
			continue;

		pPlayer->UpdatePlayerInfo();

		if ( pPlayer->GetTeamNumber() == TEAM_SPECTATOR )
			continue;

		if ( pPlayer == pHidden )
			pPlayer->ChangeTeam( TEAM_HIDDEN );
		else
			pPlayer->ChangeTeam( TEAM_IRIS );

		pPlayer->SetSpawnBool( false );
		pPlayer->RemoveAllItems( true );
		pPlayer->ShowViewPortPanel( "scores", false );
	}

	lagcompensation->FinishCompensationReset();

	SetSpawns();
	CountMarines();
	CountHiddens();
	TotalPlayers();

	IGameEvent *event = gameeventmanager->CreateEvent( "game_round_restart" );
	if ( event )
	{
		DevMsg( 1, "Fired roundrestart event\n" );
		gameeventmanager->FireEvent( event );
	}

	m_pfnThink = m_pfnRestartThink;
	m_iDuration = mp_roundtime.GetInt();
	m_fStart = gpGlobals->curtime;
}

void CommandRestartRound( void )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	SDKGameRules()->RestartRound();
}

static ConCommand hdn_restartround( "hdn_restartround", CommandRestartRound, "Restarts the round", FCVAR_GAMEDLL );

void CSDKGameRules::PlayerKilled( CBasePlayer *pVictim, const CTakeDamageInfo &info )
{
	DeathNotice( pVictim, info );

	// Find the killer & the scorer
	CBaseEntity *pInflictor = info.GetInflictor();
	CBaseEntity *pKiller = info.GetAttacker();
	CBasePlayer *pScorer = GetDeathScorer( pKiller, pInflictor );

	pVictim->IncrementDeathCount( 1 );

	// Dying makes a player less likely to be picked as the Hidden
	pVictim->m_iWeighting.GetForModify() -= 20;

	FireTargets( "game_playerdie", pVictim, pVictim, USE_TOGGLE, 0 );

	if ( pVictim->GetTeamNumber() == TEAM_IRIS )
		m_iMarineCount--;
	else
		m_iHiddenCount--;

	// Did the player kill himself?
	if ( pVictim == pScorer )
	{
		// Players lose a frag for killing themselves
		pVictim->IncrementFragCount( -1 );

		if ( pVictim->GetTeamNumber() == TEAM_HIDDEN )
		{
			DevMsg( 1, "Hidden Suicided\n" );
			m_HiddenSelector.ForfeitHidden( pVictim );
		}
	}
	else if ( pScorer )
	{
		if ( pVictim->GetTeamNumber() == TEAM_HIDDEN )
		{
			// Killing the Hidden is worth two frags and a better chance to be the next one
			DevMsg( 1, "Hidden killed\n" );
			pScorer->IncrementFragCount( 2 );
			pScorer->m_iWeighting.GetForModify() += 50;
			m_HiddenSelector.NewHidden( pScorer );
		}
		else
		{
			// The Hidden scores for kills, marines lose a frag for team kills
			if ( pScorer->GetTeamNumber() == TEAM_HIDDEN )
				pScorer->IncrementFragCount( 1 );
			else
				pScorer->IncrementFragCount( -1 );
		}

		// Allow the scorer to immediately paint a decal
		pScorer->AllowImmediateDecalPainting();

		FireTargets( "game_playerkill", pScorer, pScorer, USE_TOGGLE, 0 );
	}
	else
	{
		// Players lose a frag for letting the world kill them
		pVictim->IncrementFragCount( -1 );

		if ( pVictim->GetTeamNumber() == TEAM_HIDDEN )
		{
			DevMsg( 1, "Hidden killed by world\n" );
			m_HiddenSelector.ForfeitHidden( pVictim );
		}
	}

	DevMsg( 1, "Marines Left:%i\n", m_iMarineCount );
	DevMsg( 1, "Hiddens Left:%i\n", m_iHiddenCount );
	DevMsg( 1, "Total Players Left:%i\n", m_iTotalPlayers );
}

void CSDKGameRules::IntermissionThink( void )
{
	if ( gpGlobals->curtime >= m_flIntermissionEndTime )
	{
		DevMsg( 1, "Intermission over, restarting round\n" );
		RestartRound();
	}
}

float CSDKGameRules::FlPlayerFallDamage( CBasePlayer *pPlayer )
{
	// The Hidden takes no fall damage
	if ( pPlayer->GetTeamNumber() == TEAM_HIDDEN )
		return 0;

	pPlayer->m_Local.m_flFallVelocity -= PLAYER_MAX_SAFE_FALL_SPEED;
	return pPlayer->m_Local.m_flFallVelocity * DAMAGE_FOR_FALL_SPEED;
}

#endif
