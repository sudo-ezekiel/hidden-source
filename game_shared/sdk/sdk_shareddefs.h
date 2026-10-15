//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef SDK_SHAREDDEFS_H
#define SDK_SHAREDDEFS_H
#ifdef _WIN32
#pragma once
#endif


#define SDK_PLAYER_VIEW_OFFSET	Vector( 0, 0, 53.5 )

// Hidden's collision groups. REBUILD: the names are ours.
enum
{
	COLLISION_GROUP_HDN_PLAYER = LAST_SHARED_COLLISION_GROUP,	// players (CSDKPlayer::Spawn), don't collide with weapons
	COLLISION_GROUP_MARINE_CLIP,								// hdn_marine_clip brushes
};


#endif // SDK_SHAREDDEFS_H
