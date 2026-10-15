//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef SDK_BOT_TEMP_H
#define SDK_BOT_TEMP_H
#ifdef _WIN32
#pragma once
#endif


CBasePlayer *GenericBotPutInServer( bool bFrozen, const char *botname, int team );
CBasePlayer *BotPutInServer( bool bFrozen );
CBasePlayer *TutorialBotPutInServer( void );

void Bot_RunAll();


#endif // SDK_BOT_TEMP_H
