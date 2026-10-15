#include "cbase.h"
#include "hdn_menu.h"
#include "c_baseplayer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

HDNMenu::HDNMenu( Panel *pParent ) : Frame( NULL, "team" )
{
	SetParent( pParent->GetVPanel() );

	m_pPlayer = C_BasePlayer::GetLocalPlayer();
}

//-----------------------------------------------------------------------------
// Purpose: Sets the frame up and takes the parent's size; the menus load
//			their .res file first
//-----------------------------------------------------------------------------
void HDNMenu::InitPanel( Panel *pParent )
{
	SetKeyBoardInputEnabled( true );
	SetMouseInputEnabled( true );
	SetTitleBarVisible( true );
	SetMinimizeButtonVisible( false );
	SetMaximizeButtonVisible( false );
	SetCloseButtonVisible( false );
	SetSizeable( false );
	SetVisible( true );

	m_flWide = pParent->GetWide();
	SetWide( m_flWide );
	m_flTall = pParent->GetTall();
	SetTall( m_flTall );

	m_flScaleX = m_flWide * 0.0015625f;
	m_flScaleY = m_flTall * 0.0020833334f;
}

//-----------------------------------------------------------------------------
// Purpose: Builds the lists through the virtuals and lays the panels out
//-----------------------------------------------------------------------------
void HDNMenu::Init( Panel *pParent )
{
	InitPanel( pParent );
	InitImages();
	InitButtons();
	InitInfoImages();

	LayoutPanels();
}

void HDNMenu::LayoutPanels( void )
{
	int i;
	for ( i = 0; i < m_iButtonCount; i++ )
	{
		ScaleButton( m_Buttons[i] );
	}

	// the background covers the whole screen
	m_Images[0]->SetWide( m_flWide );
	m_Images[0]->SetTall( m_flTall );

	for ( i = 1; i < 7; i++ )
	{
		ImagePanel *pImage = m_Images[i];

		pImage->SetWide( pImage->GetWide() * m_flScaleX );
		pImage->SetTall( pImage->GetTall() * m_flScaleY );

		int x, y;
		pImage->GetPos( x, y );
		pImage->SetPos( x * m_flScaleX, y * m_flScaleY );

		pImage->SetVisible( true );
	}
}

void HDNMenu::ScaleButton( Button *pButton )
{
	pButton->SetWide( pButton->GetWide() * m_flScaleX );
	pButton->SetTall( pButton->GetTall() * m_flScaleY );

	int x, y;
	pButton->GetPos( x, y );
	pButton->SetPos( x * m_flScaleX, y * m_flScaleY );

	pButton->SetPaintBorderEnabled( false );
}

//-----------------------------------------------------------------------------
// Purpose: Puts an image at a position in 640x480 units and shows it
//-----------------------------------------------------------------------------
void HDNMenu::SetImagePos( ImagePanel *pImage, int x, int y )
{
	pImage->SetPos( x * m_flScaleX, y * m_flScaleY );
	pImage->SetVisible( true );
}

void HDNMenu::SendCommand( const char *command )
{
	engine->ClientCmd( command );
}

//-----------------------------------------------------------------------------
// Purpose: The number after the command word, or -1
//-----------------------------------------------------------------------------
int HDNMenu::GetCommandValue( const char *command )
{
	char token[64];

	const char *p = nexttoken( token, command, ' ' );
	nexttoken( token, p, ' ' );

	if ( token[0] )
		return atol( token );

	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: The derived menus fill the lists, then chain here to count them
//			(out of line: client.dll calls these from the derived menus)
//-----------------------------------------------------------------------------
void HDNMenu::InitImages( void )
{
	m_iImageCount = m_Images.Count();
}

void HDNMenu::InitInfoImages( void )
{
	m_iInfoCount = m_InfoImages.Count();
}

void HDNMenu::InitButtons( void )
{
	m_iButtonCount = m_Buttons.Count();
}

//-----------------------------------------------------------------------------
// Purpose: Shows the info image of the button under the cursor
//-----------------------------------------------------------------------------
void HDNMenu::OnThink()
{
	for ( int i = 0; i < m_iInfoCount; i++ )
	{
		if ( m_Buttons[i]->IsCursorOver() )
		{
			m_Images[1]->SetImage( m_InfoImages[i] );
			break;
		}
	}
}
