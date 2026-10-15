#include "cbase.h"
#include "hdn_radiomenu.h"
#include "c_baseplayer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

HDNRadioMenu::HDNRadioMenu( IViewPort *pViewPort ) : Frame( NULL, PANEL_RADIO )
{
	SetTitleBarVisible( false );
	SetMinimizeButtonVisible( false );
	SetMaximizeButtonVisible( false );
	SetCloseButtonVisible( false );
	SetProportional( true );
	SetSizeable( false );
	SetVisible( false );
	SetPaintBorderEnabled( false );
	SetPaintBackgroundEnabled( false );

	LoadControlSettings( "Resource/UI/Radiomenu_marine.res" );

	m_pImage = dynamic_cast< ImagePanel * >( FindChildByName( "image01" ) );
	m_pMessages[0] = dynamic_cast< Button * >( FindChildByName( "message01" ) );
	m_pMessages[1] = dynamic_cast< Button * >( FindChildByName( "message02" ) );
	m_pMessages[2] = dynamic_cast< Button * >( FindChildByName( "message03" ) );
	m_pMessages[3] = dynamic_cast< Button * >( FindChildByName( "message04" ) );
	m_pMessages[4] = dynamic_cast< Button * >( FindChildByName( "message05" ) );
	m_pMessages[5] = dynamic_cast< Button * >( FindChildByName( "message06" ) );
	m_pMessages[6] = dynamic_cast< Button * >( FindChildByName( "message07" ) );
	m_pMessages[7] = dynamic_cast< Button * >( FindChildByName( "message08" ) );

	m_pMessages[0]->SetPaintBorderEnabled( false );
	m_pMessages[1]->SetPaintBorderEnabled( false );
	m_pMessages[2]->SetPaintBorderEnabled( false );
	m_pMessages[3]->SetPaintBorderEnabled( false );
	m_pMessages[4]->SetPaintBorderEnabled( false );
	m_pMessages[5]->SetPaintBorderEnabled( false );
	m_pMessages[6]->SetPaintBorderEnabled( false );
	m_pMessages[7]->SetPaintBorderEnabled( false );

	// the .res file has the IRIS texts
	m_iTeam = TEAM_IRIS;
}

//-----------------------------------------------------------------------------
// Purpose: The buttons send the radio commands to the server
//-----------------------------------------------------------------------------
void HDNRadioMenu::OnCommand( const char *command )
{
	engine->ClientCmd( command );

	BaseClass::OnCommand( command );

	SetVisible( false );
	SetMouseInputEnabled( false );
}

void HDNRadioMenu::ShowPanel( bool bShow )
{
	if ( BaseClass::IsVisible() == bShow )
		return;

	m_pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !m_pPlayer )
		return;

	if ( bShow && m_pPlayer->IsAlive() )
	{
		SetVisible( true );
		SetMouseInputEnabled( true );

		if ( m_iTeam == m_pPlayer->GetTeamNumber() )
			return;

		m_iTeam = m_pPlayer->GetTeamNumber();

		if ( m_pPlayer->GetTeamNumber() == TEAM_IRIS )
		{
			m_pImage->SetImage( "hud/hdn_irisradio" );
			m_pMessages[0]->SetText( "&1 Agent Down" );
			m_pMessages[1]->SetText( "&2 Hidden Spotted" );
			m_pMessages[2]->SetText( "&3 Affirmative" );
			m_pMessages[3]->SetText( "&4 Requesting Ammo" );
			m_pMessages[4]->SetText( "&5 Report In" );
			m_pMessages[5]->SetText( "&6 One Ugly Mother" );
			m_pMessages[6]->SetText( "&7 If It Bleeds" );
			m_pMessages[7]->SetText( "&8 Bring It" );
		}
		else
		{
			m_pImage->SetImage( "hud/hdn_hdnradio" );
			m_pMessages[0]->SetText( "&1 Behind You" );
			m_pMessages[1]->SetText( "&2 I'm Here" );
			m_pMessages[2]->SetText( "&3 I See You" );
			m_pMessages[3]->SetText( "&4 Look Up" );
			m_pMessages[4]->SetText( "&5 Turn Around" );
			m_pMessages[5]->SetText( "&6 Over Here" );
			m_pMessages[6]->SetText( "&7 Fresh Meat" );
			m_pMessages[7]->SetText( "&8 You're Next" );
		}
	}
	else
	{
		SetVisible( false );
		SetMouseInputEnabled( false );
	}
}
