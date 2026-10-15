//-----------------------------------------------------------------------------
// Purpose: The personnel selection menu: the marine (one of the nine IRIS
//			members), the class, the Hidden opt out, or spectate
//-----------------------------------------------------------------------------
#ifndef HDN_TEAMMENU_H
#define HDN_TEAMMENU_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/ImagePanel.h>
#include "utlvector.h"

class C_BasePlayer;

//-----------------------------------------------------------------------------
// Purpose: Counter-Strike's class image panel, kept for the .res file's
//			"classimage" control; scales its settings from 640x480 units
//-----------------------------------------------------------------------------
class CCSClassImagePanel : public vgui::ImagePanel
{
public:
	typedef vgui::ImagePanel BaseClass;

	CCSClassImagePanel( vgui::Panel *pParent, const char *pName );
	virtual ~CCSClassImagePanel();

	virtual void ApplySettings( KeyValues *inResourceData );
	virtual void Paint();

public:
	float	m_flWide;		// REBUILD: names of the four floats
	float	m_flTall;
	float	m_flScaleX;
	float	m_flScaleY;

	char	m_ModelName[128];
};

extern CUtlVector< CCSClassImagePanel * > g_ClassImagePanels;

class HDNTeamMenu : public vgui::Frame
{
private:
	DECLARE_CLASS_SIMPLE( HDNTeamMenu, vgui::Frame );

public:
	HDNTeamMenu( vgui::Panel *pParent );
	virtual ~HDNTeamMenu() {}

	// both vgui::Frame and the menus define these, so explicitly define them here as passthroughs to vgui
	vgui::VPANEL GetVPanel( void ) { return BaseClass::GetVPanel(); }
	virtual bool IsVisible() { return BaseClass::IsVisible(); }
	virtual void SetParent( vgui::VPANEL parent ) { BaseClass::SetParent( parent ); }

protected:
	virtual void OnThink();
	virtual void OnCommand( const char *command );
	virtual vgui::Panel *CreateControlByName( const char *controlName );

	// REBUILD: member names
	// client.dll has 14 bytes here: "changemarine N" and "changemarine -2"
	// run on into m_flWide by one and two bytes, as they do in client.dll
	char				m_szMarineCommand[14];
	float				m_flWide;
	float				m_flTall;
	float				m_flScaleX;		// the parent's width over 640
	float				m_flScaleY;		// the parent's height over 480
	int					m_iX;
	int					m_iY;
	bool				m_bClassChosen;
	bool				m_bMarineChosen;

	vgui::Button		*m_pAssaultButton;
	vgui::Button		*m_pSupportButton;
	vgui::Button		*m_pRandomButton;
	vgui::Button		*m_pSpectateButton;
	vgui::Button		*m_pMem1Button;
	vgui::Button		*m_pMem2Button;
	vgui::Button		*m_pMem3Button;
	vgui::Button		*m_pMem4Button;
	vgui::Button		*m_pMem5Button;
	C_BasePlayer		*m_pPlayer;
	vgui::Button		*m_pMem6Button;
	vgui::Button		*m_pMem7Button;
	vgui::Button		*m_pMem8Button;
	vgui::Button		*m_pMem9Button;
	CCSClassImagePanel	*m_pClassImage;		// never set
	vgui::Button		*m_pEnterButton;

	char				m_szClassCommand[16];	// "changeclass N"

	vgui::ImagePanel	*m_pSelectionBG;
	vgui::ImagePanel	*m_pMarinePic;
	vgui::ImagePanel	*m_pMarineInfo;
	vgui::ImagePanel	*m_pClassTick;			// "TICK"
	vgui::ImagePanel	*m_pMarineTick;			// "Tick_02"
	vgui::ImagePanel	*m_pEnterCross;			// "CROSS": a tick once both choices are made
	vgui::ImagePanel	*m_pHiddenCross;		// "CROSS1": the Hidden opt out
	vgui::ImagePanel	*m_pStrike[9];			// "STRIKE_01" to "STRIKE_09": the member is taken
};

#endif // HDN_TEAMMENU_H
