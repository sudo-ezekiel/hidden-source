//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: The map entities as they were created at map load, so a round restart can
//			recreate them in the same edict slots (CSDKGameRules::CleanUpMap).
//			The list is filled by CHDNMapLoadEntityFilter in sdk_gameinterface.cpp.
//
//=============================================================================//

#ifndef HDN_MAPFILTER_H
#define HDN_MAPFILTER_H
#ifdef _WIN32
#pragma once
#endif

#include "utllinkedlist.h"


// These are created for map entities in order as the map entities are spawned.
class CMapEntityRef
{
public:
	int		m_iEdict;			// Which edict slot this entity got. -1 if CreateEntityByName failed.
	int		m_iSerialNumber;	// The edict serial number. TODO used anywhere ?
};

extern CUtlLinkedList<CMapEntityRef, unsigned short> g_MapEntityRefs;

#endif // HDN_MAPFILTER_H
