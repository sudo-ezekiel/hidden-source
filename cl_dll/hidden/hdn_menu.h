//-----------------------------------------------------------------------------
// Purpose: Base of the selection menus: a full screen frame laid out in
//			640x480 units, with lists of its images, buttons and the info
//			images shown while a button is under the cursor
//-----------------------------------------------------------------------------
#ifndef HDN_MENU_H
#define HDN_MENU_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/ImagePanel.h>
#include "utlvector.h"

class C_BasePlayer;

class HDNMenu : public vgui::Frame
{
private:
	DECLARE_CLASS_SIMPLE( HDNMenu, vgui::Frame );

public:
	HDNMenu( vgui::Panel *pParent );

	// both vgui::Frame and the menus define these, so explicitly define them here as passthroughs to vgui
	vgui::VPANEL GetVPanel( void ) { return BaseClass::GetVPanel(); }
	virtual bool IsVisible() { return BaseClass::IsVisible(); }
	virtual void SetParent( vgui::VPANEL parent ) { BaseClass::SetParent( parent ); }

	virtual void OnCommand( const char *command ) { BaseClass::OnCommand( command ); }
	virtual void OnThink();

	// REBUILD: names of the four virtuals (client.dll slots 194 to 197)
	virtual void InitPanel( vgui::Panel *pParent );
	virtual void InitImages( void );
	virtual void InitButtons( void );
	virtual void InitInfoImages( void );

protected:
	// REBUILD: names of the non-virtual helpers
	void Init( vgui::Panel *pParent );
	void LayoutPanels( void );
	void ScaleButton( vgui::Button *pButton );
	void SetImagePos( vgui::ImagePanel *pImage, int x, int y );
	void SendCommand( const char *command );
	int GetCommandValue( const char *command );

	float			m_flWide;
	float			m_flTall;
	float			m_flScaleX;		// the parent's width over 640
	float			m_flScaleY;		// the parent's height over 480

	int				m_iImageCount;
	int				m_iButtonCount;
	int				m_iInfoCount;

	C_BasePlayer	*m_pPlayer;

	CUtlVector< vgui::ImagePanel * >	m_Images;		// [0] the background, [1] the info image
	CUtlVector< const char * >			m_InfoImages;	// shown in m_Images[1] while m_Buttons[i] is under the cursor
	CUtlVector< vgui::Button * >		m_Buttons;
};

#endif // HDN_MENU_H
