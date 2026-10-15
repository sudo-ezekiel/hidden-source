//========= Hidden: Source ============//
//
// Purpose: hdn_ammocrate, an ammo box the I.R.I.S. players open with +use to top
//			up the ammo for their primary and secondary weapons. Built from the
//			HL2 item_ammo_crate.
//
//			The PDB puts the class, its entity link and its data description in
//			this header; only hdn_ammocrate.cpp includes it.
//
//=====================================//

#ifndef HDN_AMMOCRATE_H
#define HDN_AMMOCRATE_H
#ifdef _WIN32
#pragma once
#endif

#define HDN_AMMOCRATE_MODEL	"models/generic/iris_ammobox.mdl"

class CHDNAmmoCrate : public CBaseAnimating
{
public:
	DECLARE_CLASS( CHDNAmmoCrate, CBaseAnimating );
	DECLARE_DATADESC();

	void	Precache( void );
	void	Spawn( void );
	void	Think( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	virtual int	ObjectCaps( void ) { return ( BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION ) | FCAP_IMPULSE_USE | FCAP_DONT_SAVE; }

	float	m_flAmmoCrateCloseTime;	// the lid stays open until then
};

LINK_ENTITY_TO_CLASS( hdn_ammocrate, CHDNAmmoCrate );

BEGIN_DATADESC( CHDNAmmoCrate )
	DEFINE_THINKFUNC( Think ),
END_DATADESC()

#endif // HDN_AMMOCRATE_H
