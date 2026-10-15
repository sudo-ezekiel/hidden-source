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

// Hidden: collision groups past Valve's shared ones (CSDKGameRules::ShouldCollide)
enum
{
	COLLISION_GROUP_HIDDEN = LAST_SHARED_COLLISION_GROUP,	// REBUILD: name; the Hidden player
	COLLISION_GROUP_MARINE_CLIP,							// REBUILD: name; hdn_marine_clip brushes
};


#endif // SDK_SHAREDDEFS_H
