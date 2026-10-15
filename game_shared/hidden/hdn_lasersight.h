#ifndef HDN_LASERSIGHT_H__
#define HDN_LASERSIGHT_H__

#ifdef _WIN32
#pragma once
#endif // _WIN32

#include "cbase.h"

#ifdef CLIENT_DLL
	#include "model_types.h"
	#include "beamdraw.h"
	#include "c_sdk_player.h"
	#include "fx_line.h"
	#include "view.h"
#else
	#include "player.h"
	#include "sdk_player.h"
	#include "rope.h"
	#include "vstdlib/random.h"
#endif // CLIENT_DLL

#ifdef CLIENT_DLL
#define CLaserDot C_LaserDot
#endif // CLIENT_DLL

#define	RPG_BEAM_SPRITE		"effects/laser1.vmt"
#define	RPG_BEAM_SPRITE_NOZ	"effects/laser1_noz.vmt"
#define	RPG_LASER_SPRITE	"sprites/laserglow1"
//-----------------------------------------------------------------------------
// Laser Dot
//-----------------------------------------------------------------------------
class CLaserDot : public CBaseEntity
{
	DECLARE_CLASS( CLaserDot, CBaseEntity );
public:

	CLaserDot( void );
	~CLaserDot( void );

	static CLaserDot *Create( const Vector &origin, CBaseEntity *pOwner = NULL, bool bVisibleDot = true );

	void	SetTargetEntity( CBaseEntity *pTarget ) { m_hTargetEnt = pTarget; }
	CBaseEntity *GetTargetEntity( void ) { return m_hTargetEnt; }

	void	SetLaserPosition( const Vector &origin, const Vector &normal );
	Vector	GetChasePosition();
	void	TurnOn( void );
	void	TurnOff( void );
	bool	IsOn() const { return m_bIsOn; }

	void	Toggle( void );

	int		ObjectCaps() { return (BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_DONT_SAVE; }

	void	MakeInvisible( void );

#ifdef CLIENT_DLL

	virtual bool			IsTransparent( void ) { return true; }
	virtual RenderGroup_t	GetRenderGroup( void ) { return RENDER_GROUP_TRANSLUCENT_ENTITY; }
	virtual int				DrawModel( int flags );
	virtual void			OnDataChanged( DataUpdateType_t updateType );
	virtual bool			ShouldDraw( void ) { return (IsEffectActive(EF_NODRAW)==false); }

	CMaterialReference	m_hSpriteMaterial;
#endif // CLIENT_DLL

protected:
	Vector				m_vecSurfaceNormal;
	EHANDLE				m_hTargetEnt;
	bool				m_bVisibleLaserDot;
	bool				m_bIsOn;

	DECLARE_NETWORKCLASS();
	DECLARE_DATADESC();
public:
	CLaserDot			*m_pNext;
};
#endif //HDN_LASERSIGHT_H__