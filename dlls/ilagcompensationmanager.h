//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef ILAGCOMPENSATIONMANAGER_H
#define ILAGCOMPENSATIONMANAGER_H
#ifdef _WIN32
#pragma once
#endif

class CBasePlayer;
class CUserCmd;

//-----------------------------------------------------------------------------
// Purpose: This is also an IServerSystem
//-----------------------------------------------------------------------------
class ILagCompensationManager
{
public:
	// Called during player movement to set up/restore after lag compensation
	virtual void	StartLagCompensation( CBasePlayer *player, CUserCmd *cmd ) = 0;
	virtual void	FinishLagCompensation( CBasePlayer *player ) = 0;

	// Hidden: the game rules bracket a round restart with these.
	// REBUILD: pure virtual in the original; empty here until CLagCompensationManager implements them.
	virtual void	StartCompensationReset( void ) {}
	virtual void	FinishCompensationReset( void ) {}
};

extern ILagCompensationManager *lagcompensation;

#endif // ILAGCOMPENSATIONMANAGER_H
