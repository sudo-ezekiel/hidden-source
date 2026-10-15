//========= Hidden: Source ============//
//
// Purpose: The list of map entities created at map load.
//
//=====================================//

#include "cbase.h"
#include "hidden/hdn_mapfilter.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CUtlLinkedList<CMapEntityRef, unsigned short> g_MapEntityRefs;
