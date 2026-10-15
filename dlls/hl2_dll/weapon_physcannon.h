//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef WEAPON_PHYSCANNON_H
#define WEAPON_PHYSCANNON_H
#ifdef _WIN32
#pragma once
#endif



//-----------------------------------------------------------------------------
// Do we have the super-phys gun?
//-----------------------------------------------------------------------------
bool PlayerHasMegaPhysCannon();

// Hidden has no weapon_physcannon, so the PhysCannon* functions are gone; only the
// player pickup controller remains.
bool PlayerPickupControllerIsHoldingEntity( CBaseEntity *pPickupController, CBaseEntity *pHeldEntity );
float PlayerPickupGetHeldObjectMass( CBaseEntity *pPickupControllerEntity, IPhysicsObject *pHeldObject );

#endif // WEAPON_PHYSCANNON_H
