#include "cbase.h"
#include "hdn_teammenu.h"
#include "c_baseplayer.h"
#include "sdk_gamerules.h"
#include "iclientmode.h"
#include <KeyValues.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

// ----------------------------------------------------------------------------- //
// Class image panels. These maintain a list of the class image panels so
// it can render 3D images into them.
// ----------------------------------------------------------------------------- //

CUtlVector< CCSClassImagePanel * > g_ClassImagePanels;

CCSClassImagePanel::CCSClassImagePanel( Panel *pParent, const char *pName )
	: ImagePanel( pParent, pName )
{
	g_ClassImagePanels.AddToTail( this );
	m_ModelName[0] = 0;

	Panel *pViewport = g_pClientMode->GetViewport();
	if ( pViewport )
	{
		m_flWide = pViewport->GetWide();
		m_flTall = pViewport->GetTall();
		m_flScaleX = m_flWide * 0.0015625f;
		m_flScaleY = m_flTall * 0.0020833334f;
	}
}

CCSClassImagePanel::~CCSClassImagePanel()
{
	g_ClassImagePanels.FindAndRemove( this );
}

void CCSClassImagePanel::ApplySettings( KeyValues *inResourceData )
{
	const char *pName = inResourceData->GetString( "3DModel" );
	if ( pName )
	{
		Q_strncpy( m_ModelName, pName, sizeof( m_ModelName ) );
	}

	// the .res file is in 640x480 units
	inResourceData->SetFloat( "wide", inResourceData->GetFloat( "wide" ) * m_flScaleX );
	inResourceData->SetFloat( "tall", inResourceData->GetFloat( "tall" ) * m_flScaleY );
	inResourceData->SetFloat( "xpos", inResourceData->GetFloat( "xpos" ) * m_flScaleX );
	inResourceData->SetFloat( "ypos", inResourceData->GetFloat( "ypos" ) * m_flScaleY );

	BaseClass::ApplySettings( inResourceData );
}

void CCSClassImagePanel::Paint()
{
	BaseClass::Paint();
}

HDNTeamMenu::HDNTeamMenu( Panel *pParent ) : Frame( NULL, "team_menu" )
{
	SetParent( pParent->GetVPanel() );

	m_pPlayer = C_BasePlayer::GetLocalPlayer();

	SetKeyBoardInputEnabled( true );
	SetMouseInputEnabled( true );
	SetTitleBarVisible( true );
	SetMinimizeButtonVisible( false );
	SetMaximizeButtonVisible( false );
	SetCloseButtonVisible( false );
	SetSizeable( false );
	SetVisible( true );

	SetTitle( "Weapon Selection", true );
	LoadControlSettings( "Resource/UI/Teammenu.res" );

	m_flWide = pParent->GetWide();
	m_flTall = pParent->GetTall();
	m_flScaleX = m_flWide * 0.0015625f;
	m_flScaleY = m_flTall * 0.0020833334f;

	SetWide( m_flWide );
	SetTall( m_flTall );
	m_pAssaultButton = dynamic_cast< Button * >( FindChildByName( "AssaultButton" ) );
	m_pSupportButton = dynamic_cast< Button * >( FindChildByName( "SupportButton" ) );
	m_pRandomButton = dynamic_cast< Button * >( FindChildByName( "randbutton" ) );
	m_pSpectateButton = dynamic_cast< Button * >( FindChildByName( "specbutton" ) );
	m_pMem1Button = dynamic_cast< Button * >( FindChildByName( "MEM1BUTTON" ) );
	m_pMem2Button = dynamic_cast< Button * >( FindChildByName( "MEM2BUTTON" ) );
	m_pMem3Button = dynamic_cast< Button * >( FindChildByName( "MEM3BUTTON" ) );
	m_pMem4Button = dynamic_cast< Button * >( FindChildByName( "MEM4BUTTON" ) );
	m_pMem5Button = dynamic_cast< Button * >( FindChildByName( "MEM5BUTTON" ) );
	m_pMem6Button = dynamic_cast< Button * >( FindChildByName( "MEM6BUTTON" ) );
	m_pMem7Button = dynamic_cast< Button * >( FindChildByName( "MEM7BUTTON" ) );
	m_pMem8Button = dynamic_cast< Button * >( FindChildByName( "MEM8BUTTON" ) );
	m_pMem9Button = dynamic_cast< Button * >( FindChildByName( "MEM9BUTTON" ) );
	m_pEnterButton = dynamic_cast< Button * >( FindChildByName( "ENTER" ) );

	m_pSelectionBG = dynamic_cast< ImagePanel * >( FindChildByName( "SelectionBG" ) );
	m_pMarinePic = dynamic_cast< ImagePanel * >( FindChildByName( "Marine_Pic" ) );
	m_pMarineInfo = dynamic_cast< ImagePanel * >( FindChildByName( "Marine_Info" ) );
	m_pClassTick = dynamic_cast< ImagePanel * >( FindChildByName( "TICK" ) );
	m_pMarineTick = dynamic_cast< ImagePanel * >( FindChildByName( "Tick_02" ) );
	m_pEnterCross = dynamic_cast< ImagePanel * >( FindChildByName( "CROSS" ) );
	m_pHiddenCross = dynamic_cast< ImagePanel * >( FindChildByName( "CROSS1" ) );
	m_pStrike[0] = dynamic_cast< ImagePanel * >( FindChildByName( "STRIKE_01" ) );
	m_pStrike[1] = dynamic_cast< ImagePanel * >( FindChildByName( "STRIKE_02" ) );
	m_pStrike[2] = dynamic_cast< ImagePanel * >( FindChildByName( "STRIKE_03" ) );
	m_pStrike[3] = dynamic_cast< ImagePanel * >( FindChildByName( "STRIKE_04" ) );
	m_pStrike[4] = dynamic_cast< ImagePanel * >( FindChildByName( "STRIKE_05" ) );
	m_pStrike[5] = dynamic_cast< ImagePanel * >( FindChildByName( "STRIKE_06" ) );
	m_pStrike[6] = dynamic_cast< ImagePanel * >( FindChildByName( "STRIKE_07" ) );
	m_pStrike[7] = dynamic_cast< ImagePanel * >( FindChildByName( "STRIKE_08" ) );
	m_pStrike[8] = dynamic_cast< ImagePanel * >( FindChildByName( "STRIKE_09" ) );

	// sizes, from 640x480 units
	if ( m_pSelectionBG )
	{
		m_pSelectionBG->SetWide( m_flWide );
		m_pSelectionBG->SetTall( m_flTall );
	}

	m_pMarinePic->SetWide( m_pMarinePic->GetWide() * m_flScaleX );
	m_pMarinePic->SetTall( m_pMarinePic->GetTall() * m_flScaleY );
	m_pMarineInfo->SetWide( m_pMarineInfo->GetWide() * m_flScaleX );
	m_pMarineInfo->SetTall( m_pMarineInfo->GetTall() * m_flScaleY );
	m_pClassTick->SetWide( m_pClassTick->GetWide() * m_flScaleX );
	m_pClassTick->SetTall( m_pClassTick->GetTall() * m_flScaleY );
	m_pMarineTick->SetWide( m_pClassTick->GetWide() );
	m_pMarineTick->SetTall( m_pClassTick->GetTall() );
	m_pEnterCross->SetWide( m_pClassTick->GetWide() );
	m_pEnterCross->SetTall( m_pClassTick->GetTall() );
	m_pHiddenCross->SetWide( m_pClassTick->GetWide() );
	m_pHiddenCross->SetTall( m_pClassTick->GetTall() );

	m_pEnterCross->SetPos( 72, 415 );
	m_pEnterCross->GetPos( m_iX, m_iY );
	m_pEnterCross->SetPos( m_iX * m_flScaleX, m_iY * m_flScaleY );
	m_pHiddenCross->SetPos( 199, 380 );
	m_pHiddenCross->GetPos( m_iX, m_iY );
	m_pHiddenCross->SetPos( m_iX * m_flScaleX, m_iY * m_flScaleY );

	m_pAssaultButton->SetWide( m_pAssaultButton->GetWide() * m_flScaleX );
	m_pAssaultButton->SetTall( m_pAssaultButton->GetTall() * m_flScaleY );
	m_pSupportButton->SetWide( m_pSupportButton->GetWide() * m_flScaleX );
	m_pSupportButton->SetTall( m_pSupportButton->GetTall() * m_flScaleY );
	m_pRandomButton->SetWide( m_pRandomButton->GetWide() * m_flScaleX );
	m_pRandomButton->SetTall( m_pRandomButton->GetTall() * m_flScaleY );
	m_pSpectateButton->SetWide( m_pSpectateButton->GetWide() * m_flScaleX );
	m_pSpectateButton->SetTall( m_pSpectateButton->GetTall() * m_flScaleY );

	m_pMem4Button->SetWide( m_pMem4Button->GetWide() * m_flScaleX );
	m_pMem4Button->SetTall( m_pMem4Button->GetTall() * m_flScaleY );
	m_pStrike[3]->SetWide( m_pMem4Button->GetWide() );
	m_pStrike[3]->SetTall( m_pMem4Button->GetTall() );

	m_pMem5Button->SetWide( m_pMem5Button->GetWide() * m_flScaleX );
	m_pMem5Button->SetTall( m_pMem5Button->GetTall() * m_flScaleY );
	m_pStrike[4]->SetWide( m_pMem5Button->GetWide() );
	m_pStrike[4]->SetTall( m_pMem5Button->GetTall() );

	m_pMem6Button->SetWide( m_pMem6Button->GetWide() * m_flScaleX );
	m_pMem6Button->SetTall( m_pMem6Button->GetTall() * m_flScaleY );
	m_pStrike[5]->SetWide( m_pMem6Button->GetWide() );
	m_pStrike[5]->SetTall( m_pMem6Button->GetTall() );

	m_pMem7Button->SetWide( m_pMem7Button->GetWide() * m_flScaleX );
	m_pMem7Button->SetTall( m_pMem7Button->GetTall() * m_flScaleY );
	m_pStrike[6]->SetWide( m_pMem7Button->GetWide() );
	m_pStrike[6]->SetTall( m_pMem7Button->GetTall() );

	m_pMem8Button->SetWide( m_pMem8Button->GetWide() * m_flScaleX );
	m_pMem8Button->SetTall( m_pMem8Button->GetTall() * m_flScaleY );
	m_pStrike[7]->SetWide( m_pMem8Button->GetWide() );
	m_pStrike[7]->SetTall( m_pMem8Button->GetTall() );

	m_pMem9Button->SetWide( m_pMem9Button->GetWide() * m_flScaleX );
	m_pMem9Button->SetTall( m_pMem9Button->GetTall() * m_flScaleY );
	m_pStrike[8]->SetWide( m_pMem9Button->GetWide() );
	m_pStrike[8]->SetTall( m_pMem9Button->GetTall() );

	m_pMem1Button->SetWide( m_pMem1Button->GetWide() * m_flScaleX );
	m_pMem1Button->SetTall( m_pMem1Button->GetTall() * m_flScaleY );
	m_pStrike[0]->SetWide( m_pMem1Button->GetWide() );
	m_pStrike[0]->SetTall( m_pMem1Button->GetTall() );

	m_pMem2Button->SetWide( m_pMem2Button->GetWide() * m_flScaleX );
	m_pMem2Button->SetTall( m_pMem2Button->GetTall() * m_flScaleY );
	m_pStrike[1]->SetWide( m_pMem2Button->GetWide() );
	m_pStrike[1]->SetTall( m_pMem2Button->GetTall() );

	m_pMem3Button->SetWide( m_pMem3Button->GetWide() * m_flScaleX );
	m_pMem3Button->SetTall( m_pMem3Button->GetTall() * m_flScaleY );
	m_pStrike[2]->SetWide( m_pMem3Button->GetWide() );
	m_pStrike[2]->SetTall( m_pMem3Button->GetTall() );

	m_pEnterButton->SetWide( m_pEnterButton->GetWide() * m_flScaleX );
	m_pEnterButton->SetTall( m_pEnterButton->GetTall() * m_flScaleY );

	// positions
	m_pMarinePic->GetPos( m_iX, m_iY );
	m_pMarinePic->SetPos( m_iX * m_flScaleX, m_iY * m_flScaleY );
	m_pMarineInfo->GetPos( m_iX, m_iY );
	m_pMarineInfo->SetPos( m_iX * m_flScaleX, m_iY * m_flScaleY );
	m_pAssaultButton->GetPos( m_iX, m_iY );
	m_pAssaultButton->SetPos( m_iX * m_flScaleX, m_iY * m_flScaleY );
	m_pSupportButton->GetPos( m_iX, m_iY );
	m_pSupportButton->SetPos( m_flScaleX * 62, m_flScaleY * 400 );
	m_pRandomButton->GetPos( m_iX, m_iY );
	m_pRandomButton->SetPos( m_flScaleX * 201, m_flScaleY * 378 );
	m_pSpectateButton->GetPos( m_iX, m_iY );
	m_pSpectateButton->SetPos( m_flScaleX * 64, m_flScaleY * 424 );

	m_pMem1Button->SetPos( m_flScaleX * 57, m_flScaleY * 173 );
	m_pMem1Button->GetPos( m_iX, m_iY );
	m_pStrike[0]->SetPos( m_iX, m_iY );

	m_pMem2Button->SetPos( m_flScaleX * 53, m_flScaleY * 145 );
	m_pMem2Button->GetPos( m_iX, m_iY );
	m_pStrike[1]->SetPos( m_iX, m_iY );

	m_pMem3Button->SetPos( m_flScaleX * 63, m_flScaleY * 278 );
	m_pMem3Button->GetPos( m_iX, m_iY );
	m_pStrike[2]->SetPos( m_iX, m_iY );

	m_pMem4Button->GetPos( m_iX, m_iY );
	m_pMem4Button->SetPos( m_iX * m_flScaleX, m_iY * m_flScaleY );
	m_pMem4Button->GetPos( m_iX, m_iY );
	m_pStrike[3]->SetPos( m_iX, m_iY );

	m_pMem5Button->GetPos( m_iX, m_iY );
	m_pMem5Button->SetPos( m_iX * m_flScaleX, m_iY * m_flScaleY );
	m_pMem5Button->GetPos( m_iX, m_iY );
	m_pStrike[4]->SetPos( m_iX, m_iY );

	m_pMem6Button->GetPos( m_iX, m_iY );
	m_pMem6Button->SetPos( m_iX * m_flScaleX, m_iY * m_flScaleY );
	m_pMem6Button->GetPos( m_iX, m_iY );
	m_pStrike[5]->SetPos( m_iX, m_iY );

	m_pMem7Button->GetPos( m_iX, m_iY );
	m_pMem7Button->SetPos( m_iX * m_flScaleX, m_iY * m_flScaleY );
	m_pMem7Button->GetPos( m_iX, m_iY );
	m_pStrike[6]->SetPos( m_iX, m_iY );

	m_pMem8Button->GetPos( m_iX, m_iY );
	m_pMem8Button->SetPos( m_iX * m_flScaleX, m_iY * m_flScaleY );
	m_pMem8Button->GetPos( m_iX, m_iY );
	m_pStrike[7]->SetPos( m_iX, m_iY );

	m_pMem9Button->GetPos( m_iX, m_iY );
	m_pMem9Button->SetPos( m_iX * m_flScaleX, m_iY * m_flScaleY );
	m_pMem9Button->GetPos( m_iX, m_iY );
	m_pStrike[8]->SetPos( m_iX, m_iY );

	m_pEnterButton->GetPos( m_iX, m_iY );
	m_pEnterButton->SetPos( m_iX * m_flScaleX, m_iY * m_flScaleY );

	m_pAssaultButton->SetPaintBorderEnabled( false );
	m_pSupportButton->SetPaintBorderEnabled( false );
	m_pRandomButton->SetPaintBorderEnabled( false );
	m_pSpectateButton->SetPaintBorderEnabled( false );
	m_pMem1Button->SetPaintBorderEnabled( false );
	m_pMem2Button->SetPaintBorderEnabled( false );
	m_pMem3Button->SetPaintBorderEnabled( false );
	m_pMem4Button->SetPaintBorderEnabled( false );
	m_pMem5Button->SetPaintBorderEnabled( false );
	m_pMem6Button->SetPaintBorderEnabled( false );
	m_pMem7Button->SetPaintBorderEnabled( false );
	m_pMem8Button->SetPaintBorderEnabled( false );
	m_pMem9Button->SetPaintBorderEnabled( false );
	m_pEnterButton->SetPaintBorderEnabled( false );

	if ( m_pPlayer->GetNoHidden() )
	{
		m_pHiddenCross->SetImage( "selection/tick" );
	}
	else
	{
		m_pHiddenCross->SetImage( "selection/cross" );
	}

	m_bClassChosen = false;
	m_bMarineChosen = false;
}

//-----------------------------------------------------------------------------
// Purpose: Strikes out the members other players have, shows the picture and
//			the file of the member under the cursor, and ticks "enter" once
//			both choices are made
//-----------------------------------------------------------------------------
void HDNTeamMenu::OnThink()
{
	C_SDKGameRules *pRules = SDKGameRules();	// REBUILD: name
	if ( !pRules )
		return;

	if ( pRules->IsCharacterTaken( 0 ) == true )
	{
		m_pStrike[6]->SetVisible( true );
		m_pStrike[6]->SetAlpha( 127 );
		m_pMem7Button->SetEnabled( false );
	}
	else
	{
		if ( m_pMem7Button->IsCursorOver() )
		{
			m_pMarinePic->SetVisible( true );
			m_pMarinePic->SetImage( "selection/wakefield" );
			m_pMarineInfo->SetVisible( true );
			m_pMarineInfo->SetImage( "selection/colininfo" );
		}

		m_pMem7Button->SetEnabled( true );
		m_pStrike[6]->SetVisible( false );
	}

	if ( pRules->IsCharacterTaken( 1 ) == true )
	{
		m_pStrike[4]->SetVisible( true );
		m_pMem5Button->SetEnabled( false );
	}
	else
	{
		if ( m_pMem5Button->IsCursorOver() )
		{
			m_pMarinePic->SetVisible( true );
			m_pMarinePic->SetImage( "selection/sven" );
			m_pMarineInfo->SetVisible( true );
			m_pMarineInfo->SetImage( "selection/sveninfo" );
		}

		m_pMem5Button->SetEnabled( true );
		m_pStrike[4]->SetVisible( false );
	}

	if ( pRules->IsCharacterTaken( 2 ) == true )
	{
		m_pStrike[2]->SetVisible( true );
		m_pMem3Button->SetEnabled( false );
	}
	else
	{
		if ( m_pMem3Button->IsCursorOver() )
		{
			m_pMarinePic->SetVisible( true );
			m_pMarinePic->SetImage( "selection/kuti" );
			m_pMarineInfo->SetVisible( true );
			m_pMarineInfo->SetImage( "selection/kutiinfo" );
		}

		m_pMem3Button->SetEnabled( true );
		m_pStrike[2]->SetVisible( false );
	}

	if ( pRules->IsCharacterTaken( 3 ) == true )
	{
		m_pStrike[5]->SetVisible( true );
		m_pMem6Button->SetEnabled( false );
	}
	else
	{
		if ( m_pMem6Button->IsCursorOver() )
		{
			m_pMarinePic->SetVisible( true );
			m_pMarinePic->SetImage( "selection/swope" );
			m_pMarineInfo->SetVisible( true );
			m_pMarineInfo->SetImage( "selection/swopeinfo" );
		}

		m_pMem6Button->SetEnabled( true );
		m_pStrike[5]->SetVisible( false );
	}

	if ( pRules->IsCharacterTaken( 4 ) == true )
	{
		m_pStrike[0]->SetVisible( true );
		m_pMem1Button->SetEnabled( false );
	}
	else
	{
		if ( m_pMem1Button->IsCursorOver() )
		{
			m_pMarinePic->SetVisible( true );
			m_pMarinePic->SetImage( "selection/choi" );
			m_pMarineInfo->SetVisible( true );
			m_pMarineInfo->SetImage( "selection/choiinfo" );
		}

		m_pMem1Button->SetEnabled( true );
		m_pStrike[0]->SetVisible( false );
	}

	if ( pRules->IsCharacterTaken( 5 ) == true )
	{
		m_pStrike[3]->SetVisible( true );
		m_pMem4Button->SetEnabled( false );
	}
	else
	{
		if ( m_pMem4Button->IsCursorOver() )
		{
			m_pMarinePic->SetVisible( true );
			m_pMarinePic->SetImage( "selection/lewis" );
			m_pMarineInfo->SetVisible( true );
			m_pMarineInfo->SetImage( "selection/lewisinfo" );
		}

		m_pMem4Button->SetEnabled( true );
		m_pStrike[3]->SetVisible( false );
	}

	if ( pRules->IsCharacterTaken( 6 ) == true )
	{
		m_pStrike[7]->SetVisible( true );
		m_pMem8Button->SetEnabled( false );
	}
	else
	{
		if ( m_pMem8Button->IsCursorOver() )
		{
			m_pMarinePic->SetVisible( true );
			m_pMarinePic->SetImage( "selection/wallace" );
			m_pMarineInfo->SetVisible( true );
			m_pMarineInfo->SetImage( "selection/duncaninfo" );
		}

		m_pMem8Button->SetEnabled( true );
		m_pStrike[7]->SetVisible( false );
	}

	if ( pRules->IsCharacterTaken( 7 ) == true )
	{
		m_pStrike[1]->SetVisible( true );
		m_pMem2Button->SetEnabled( false );
	}
	else
	{
		if ( m_pMem2Button->IsCursorOver() )
		{
			m_pMarinePic->SetVisible( true );
			m_pMarinePic->SetImage( "selection/gruber" );
			m_pMarineInfo->SetVisible( true );
			m_pMarineInfo->SetImage( "selection/gruberinfo" );
		}

		m_pMem2Button->SetEnabled( true );
		m_pStrike[1]->SetVisible( false );
	}

	if ( pRules->IsCharacterTaken( 8 ) == true )
	{
		m_pStrike[8]->SetVisible( true );
		m_pMem9Button->SetEnabled( false );
	}
	else
	{
		if ( m_pMem9Button->IsCursorOver() )
		{
			m_pMarinePic->SetVisible( true );
			m_pMarinePic->SetImage( "selection/kasim" );
			m_pMarineInfo->SetVisible( true );
			m_pMarineInfo->SetImage( "selection/kasiminfo" );
		}

		m_pMem9Button->SetEnabled( true );
		m_pStrike[8]->SetVisible( false );
	}

	if ( m_pAssaultButton->IsCursorOver() )
	{
		m_pMarineInfo->SetVisible( true );
		m_pMarineInfo->SetImage( "selection/assaultinfo" );
	}

	if ( m_pSupportButton->IsCursorOver() )
	{
		m_pMarineInfo->SetVisible( true );
		m_pMarineInfo->SetImage( "selection/supportinfo" );
	}

	if ( m_bMarineChosen == true && m_bClassChosen == true )
	{
		m_pEnterCross->SetImage( "selection/tick" );
	}
	else
	{
		m_pEnterCross->SetImage( "selection/cross" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: The .res file's commands, told apart by their length:
//			"changeclass N", "changemarine N", "enter", "chooseh 0" and
//			"spectate"
//-----------------------------------------------------------------------------
void HDNTeamMenu::OnCommand( const char *command )
{
	int len = strlen( command );

	if ( len == 13 )
	{
		m_bClassChosen = true;
		strcpy( m_szClassCommand, command );

		if ( m_pAssaultButton->IsCursorOver() )
		{
			m_pClassTick->SetVisible( true );
			m_pClassTick->SetPos( m_flScaleX * 41, m_flScaleY * 378 );
		}

		if ( m_pSupportButton->IsCursorOver() )
		{
			m_pClassTick->SetVisible( true );
			m_pClassTick->SetPos( m_flScaleX * 42, m_flScaleY * 400 );
		}

		if ( m_pRandomButton->IsCursorOver() )
		{
			m_pClassTick->SetVisible( true );
			m_pClassTick->SetPos( m_flScaleX * 43, m_flScaleY * 422 );
			return;
		}
	}
	else if ( len == 14 )
	{
		m_bMarineChosen = true;
		strcpy( m_szMarineCommand, command );

		if ( m_pMem1Button->IsCursorOver() )
		{
			m_pMarineTick->SetVisible( true );
			m_pMarineTick->SetPos( m_flScaleX * 31, m_flScaleY * 176 );
			return;
		}

		if ( m_pMem2Button->IsCursorOver() )
		{
			m_pMarineTick->SetVisible( true );
			m_pMarineTick->SetPos( m_flScaleX * 29, m_flScaleY * 148 );
			return;
		}

		if ( m_pMem3Button->IsCursorOver() )
		{
			m_pMarineTick->SetVisible( true );
			m_pMarineTick->SetPos( m_flScaleX * 38, m_flScaleY * 281 );
			return;
		}

		if ( m_pMem4Button->IsCursorOver() )
		{
			m_pMarineTick->SetVisible( true );
			m_pMarineTick->SetPos( m_flScaleX * 32, m_flScaleY * 230 );
			return;
		}

		if ( m_pMem5Button->IsCursorOver() )
		{
			m_pMarineTick->SetVisible( true );
			m_pMarineTick->SetPos( m_flScaleX * 33, m_flScaleY * 204 );
			return;
		}

		if ( m_pMem6Button->IsCursorOver() )
		{
			m_pMarineTick->SetVisible( true );
			m_pMarineTick->SetPos( m_flScaleX * 34, m_flScaleY * 255 );
			return;
		}

		if ( m_pMem7Button->IsCursorOver() )
		{
			m_pMarineTick->SetVisible( true );
			m_pMarineTick->SetPos( m_flScaleX * 27, m_flScaleY * 124 );
			return;
		}

		if ( m_pMem8Button->IsCursorOver() )
		{
			m_pMarineTick->SetVisible( true );
			m_pMarineTick->SetPos( m_flScaleX * 40, m_flScaleY * 332 );
			return;
		}

		if ( m_pMem9Button->IsCursorOver() )
		{
			m_pMarineTick->SetVisible( true );
			m_pMarineTick->SetPos( m_flScaleX * 40, m_flScaleY * 306 );
			return;
		}
	}
	else if ( len == 5 )
	{
		// "enter": a missing choice is sent as -2, then the loadout menu
		if ( m_bClassChosen != true )
		{
			strcpy( m_szClassCommand, "changeclass -2" );
		}

		engine->ClientCmd( m_szClassCommand );
		BaseClass::OnCommand( m_szClassCommand );

		if ( m_bMarineChosen == true )
		{
			engine->ClientCmd( m_szMarineCommand );
		}
		else
		{
			strcpy( m_szMarineCommand, "changemarine -2" );
			engine->ClientCmd( m_szMarineCommand );
		}

		BaseClass::OnCommand( m_szMarineCommand );

		strcpy( m_szMarineCommand, "weaponmenu" );
		engine->ClientCmd( m_szMarineCommand );
		BaseClass::OnCommand( m_szMarineCommand );
		Close();
	}
	else if ( len == 9 )
	{
		// "chooseh 0": the Hidden opt out
		char szCommand[16];
		const char *pszImage;

		if ( m_pPlayer->GetNoHidden() )
		{
			strcpy( szCommand, "choosehidden 0" );
			pszImage = "selection/cross";
		}
		else
		{
			strcpy( szCommand, "choosehidden 1" );
			pszImage = "selection/tick";
		}

		m_pHiddenCross->SetImage( pszImage );
		engine->ClientCmd( szCommand );
		BaseClass::OnCommand( szCommand );
	}
	else if ( len == 8 )
	{
		// "spectate"
		engine->ClientCmd( command );
		BaseClass::OnCommand( command );
		Close();
	}
}

Panel *HDNTeamMenu::CreateControlByName( const char *controlName )
{
	if ( !Q_stricmp( controlName, "CSClassImagePanel" ) )
	{
		return new CCSClassImagePanel( NULL, controlName );
	}

	return BaseClass::CreateControlByName( controlName );
}

CON_COMMAND( classmenu, "Shows the personnel selection menu" )
{
	HDNTeamMenu *pMenu = new HDNTeamMenu( g_pClientMode->GetViewport() );
	pMenu->Activate();
}
