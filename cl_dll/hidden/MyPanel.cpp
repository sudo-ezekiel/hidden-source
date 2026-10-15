#include "cbase.h"
#include "hidden/IMyPanel.h"

#include <vgui/IVGui.h>
#include <vgui/ISurface.h>
#include <vgui_controls/Frame.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: The tutorial menu: loads the Hidden or the marine tutorial map
//-----------------------------------------------------------------------------
class CMyPanel : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CMyPanel, vgui::Frame );

public:
	CMyPanel( vgui::VPANEL parent );
	~CMyPanel() {}

protected:
	// VGUI overrides
	virtual void OnTick();
	virtual void OnCommand( const char *pcCommand );
};

ConVar cl_showmypanel( "cl_showmypanel", "0", FCVAR_CLIENTDLL, "Sets the state of myPanel <state>" );

CMyPanel::CMyPanel( vgui::VPANEL parent ) : BaseClass( NULL, "MyPanel" )
{
	SetParent( parent );

	SetKeyBoardInputEnabled( true );
	SetMouseInputEnabled( true );

	SetProportional( true );
	SetTitleBarVisible( true );
	SetMinimizeButtonVisible( false );
	SetMaximizeButtonVisible( false );
	SetCloseButtonVisible( true );
	SetSizeable( false );
	SetMoveable( true );
	SetVisible( true );

	SetScheme( vgui::scheme()->LoadSchemeFromFile( "resource/SourceScheme.res", "SourceScheme" ) );

	LoadControlSettings( "resource/UI/MyPanel.res" );

	vgui::ivgui()->AddTickSignal( GetVPanel(), 100 );

	DevMsg( "MyPanel has been constructed\n" );
}

void CMyPanel::OnTick()
{
	BaseClass::OnTick();
	SetVisible( cl_showmypanel.GetBool() );
}

void CMyPanel::OnCommand( const char *pcCommand )
{
	if ( !Q_stricmp( pcCommand, "turnoff" ) )
		cl_showmypanel.SetValue( 0 );

	if ( !Q_stricmp( pcCommand, "loadhiddentut" ) )
		engine->ClientCmd( "map htr_tutorial" );

	if ( !Q_stricmp( pcCommand, "loadmarinetut" ) )
		engine->ClientCmd( "map mtr_tutorial" );
}

//-----------------------------------------------------------------------------
// Purpose: Makes and destroys the panel for vgui_int.cpp
//-----------------------------------------------------------------------------
class CMyPanelInterface : public IMyPanel
{
private:
	CMyPanel *MyPanel;

public:
	CMyPanelInterface()
	{
		MyPanel = NULL;
	}

	void Create( vgui::VPANEL parent )
	{
		MyPanel = new CMyPanel( parent );
	}

	void Destroy()
	{
		if ( MyPanel )
		{
			MyPanel->SetParent( (vgui::Panel *)NULL );
			delete MyPanel;
		}
	}

	void Activate( void )
	{
		if ( MyPanel )
		{
			MyPanel->Activate();
		}
	}
};

static CMyPanelInterface g_MyPanel;
IMyPanel *mypanel = (IMyPanel *)&g_MyPanel;

CON_COMMAND( ToggleMyPanel, "Toggles myPanel on or off" )
{
	cl_showmypanel.SetValue( !cl_showmypanel.GetBool() );
}
