//-----------------------------------------------------------------------------
// Purpose: The radio menu: eight voice commands, the IRIS or the Hidden set
//			by the local player's team
//-----------------------------------------------------------------------------
#ifndef HDN_RADIOMENU_H
#define HDN_RADIOMENU_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/ImagePanel.h>

#include <cl_dll/iviewport.h>

class C_BasePlayer;

#define PANEL_RADIO		"radio"		// REBUILD: name; the string is client.dll's

class HDNRadioMenu : public vgui::Frame, public IViewPortPanel
{
private:
	DECLARE_CLASS_SIMPLE( HDNRadioMenu, vgui::Frame );

public:
	HDNRadioMenu( IViewPort *pViewPort );
	virtual ~HDNRadioMenu() {}

	virtual const char *GetName( void ) { return PANEL_RADIO; }
	virtual void SetData( KeyValues *data ) {}
	virtual void Reset() {}
	virtual void Update() {}
	virtual bool NeedsUpdate( void ) { return true; }
	virtual bool HasInputElements( void ) { return true; }
	virtual void ShowPanel( bool bShow );

	// both vgui::Frame and IViewPortPanel define these, so explicitly define them here as passthroughs to vgui
	vgui::VPANEL GetVPanel( void ) { return BaseClass::GetVPanel(); }
	virtual bool IsVisible() { return BaseClass::IsVisible(); }
	virtual void SetParent( vgui::VPANEL parent ) { BaseClass::SetParent( parent ); }

protected:
	virtual void OnThink() {}
	virtual void OnCommand( const char *command );

	C_BasePlayer	*m_pPlayer;
	int				m_iTeam;		// team the button texts were set for

	vgui::Button	*m_pMessages[8];
	vgui::ImagePanel *m_pImage;
};

#endif // HDN_RADIOMENU_H
