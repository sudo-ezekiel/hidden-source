//-----------------------------------------------------------------------------
// Purpose: Interface to the tutorial menu panel (CMyPanel), made under the
//			GameUI panel by VGui_CreateGlobalPanels
//-----------------------------------------------------------------------------
#ifndef IMYPANEL_H
#define IMYPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>

namespace vgui
{
	class Panel;
}

class IMyPanel
{
public:
	virtual void		Create( vgui::VPANEL parent ) = 0;
	virtual void		Destroy( void ) = 0;
	virtual void		Activate( void ) = 0;
};

extern IMyPanel *mypanel;

#endif // IMYPANEL_H
