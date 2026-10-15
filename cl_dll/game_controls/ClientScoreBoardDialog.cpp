//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include <stdio.h>

#include <cdll_client_int.h>
#include <cdll_util.h>
#include <globalvars_base.h>
#include <igameresources.h>

#include "clientscoreboarddialog.h"

#include <vgui/IScheme.h>
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include <vgui/IVGui.h>
#include <vstdlib/IKeyValuesSystem.h>

#include <KeyValues.h>
#include <vgui_controls/ImageList.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/SectionedListPanel.h>

#include <cl_dll/iviewport.h>
#include <igameresources.h>

#include "voice_status.h"
//#include "Friends/IFriendsUser.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// extern vars
//extern IFriendsUser *g_pFriendsUser;

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CClientScoreBoardDialog::CClientScoreBoardDialog(IViewPort *pViewPort) : Frame( NULL, PANEL_SCOREBOARD )
{
	m_iPlayerIndexSymbol = KeyValuesSystem()->GetSymbolForString("playerIndex");

	memset(s_VoiceImage, 0x0, sizeof( s_VoiceImage ));
	TrackerImage = 0;
	m_pViewPort = pViewPort;

	// initialize dialog
	SetProportional(true);
	SetKeyBoardInputEnabled(false);
	SetMouseInputEnabled(false);
	SetSizeable(false);

	// hide the system buttons
	SetTitleBarVisible( false );
	SetPaintBackgroundEnabled( false );

	// set the scheme before any child control is created
	SetScheme("ScoreScheme");

	m_pPlayerList = new SectionedListPanel(this, "PlayerList");
	m_pPlayerList->SetVerticalScrollbar(false);
	m_pPlayerList->SetPaintBackgroundEnabled( false );
	m_pPlayerList->SetZPos( 1 );

	LoadControlSettings("Resource/UI/ScoreBoard.res");
	m_iDesiredHeight = GetTall();
	m_pPlayerList->SetVisible( false ); // hide this until we load the images in applyschemesettings

	m_HLTVSpectators = 0;

	// Hidden: the background image fills the whole scoreboard, behind the list
	m_pSelectionBG = dynamic_cast<ImagePanel *>( FindChildByName( "SelectionBG" ) );
	if ( m_pSelectionBG )
	{
		m_pSelectionBG->SetWide( GetWide() );
		m_pSelectionBG->SetTall( GetTall() );
		m_pSelectionBG->SetZPos( 0 );
	}
	
	// update scoreboard instantly if on of these events occure
	gameeventmanager->AddListener( this, "hltv_status", false );
	gameeventmanager->AddListener( this, "server_spawn", false );
	gameeventmanager->AddListener( this, "game_round_restart", false );
	gameeventmanager->AddListener( this, "player_connect", false );
	gameeventmanager->AddListener( this, "player_death", false );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CClientScoreBoardDialog::~CClientScoreBoardDialog()
{
	gameeventmanager->RemoveListener(this);
}

//-----------------------------------------------------------------------------
// Purpose: clears everything in the scoreboard and all it's state
//-----------------------------------------------------------------------------
void CClientScoreBoardDialog::Reset()
{
	// clear
	m_pPlayerList->DeleteAllItems();
	m_pPlayerList->RemoveAllSections();

	m_iSectionId = 0;
	m_fNextUpdateTime = 0;
	// add all the sections
	InitScoreboardSections();
}

//-----------------------------------------------------------------------------
// Purpose: adds all the team sections to the scoreboard
//-----------------------------------------------------------------------------
void CClientScoreBoardDialog::InitScoreboardSections()
{
	AddHeader();
}

//-----------------------------------------------------------------------------
// Purpose: sets up screen
//-----------------------------------------------------------------------------
void CClientScoreBoardDialog::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	ImageList *imageList = new ImageList(false);
	s_VoiceImage[0] = 0;	// index 0 is always blank
	s_VoiceImage[CVoiceStatus::VOICE_NEVERSPOKEN] = imageList->AddImage(scheme()->GetImage("gfx/vgui/640_speaker1", true));
	s_VoiceImage[CVoiceStatus::VOICE_NOTTALKING] = imageList->AddImage(scheme()->GetImage("gfx/vgui/640_speaker2", true));
	s_VoiceImage[CVoiceStatus::VOICE_TALKING] = imageList->AddImage(scheme()->GetImage( "gfx/vgui/640_speaker3", true));
	s_VoiceImage[CVoiceStatus::VOICE_BANNED] = imageList->AddImage(scheme()->GetImage("gfx/vgui/640_voiceblocked", true));
	
//	TrackerImage = imageList->AddImage(scheme()->GetImage("gfx/vgui/640_scoreboardtracker", true));

	// resize the images to our resolution
	for (int i = 0; i < imageList->GetImageCount(); i++ )
	{
		int wide, tall;
		imageList->GetImage(i)->GetSize(wide, tall);
		imageList->GetImage(i)->SetSize(scheme()->GetProportionalScaledValue(wide), scheme()->GetProportionalScaledValue(tall));
	}

	m_pPlayerList->SetImageList(imageList, false);
	m_pPlayerList->SetVisible( true );

	// light up scoreboard a bit
	SetBgColor( Color( 0,0,0,0) );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CClientScoreBoardDialog::ShowPanel(bool bShow)
{
	if ( BaseClass::IsVisible() == bShow )
		return;

	if ( bShow )
	{
		Reset();
		Update();

		Activate();
	}
	else
	{
		BaseClass::SetVisible( false );
		SetMouseInputEnabled( false );
		SetKeyBoardInputEnabled( false );
	}
}



void CClientScoreBoardDialog::FireGameEvent( IGameEvent *event )
{
	const char * type = event->GetName();

	if ( Q_strcmp(type, "hltv_status") == 0 )
	{
		// spectators = clients - proxies
		m_HLTVSpectators = event->GetInt( "clients" );
		m_HLTVSpectators -= event->GetInt( "proxies" );
	}

	else if ( Q_strcmp(type, "server_spawn") == 0 )
	{
		SetControlString("ServerName", event->GetString("hostname") );
		MoveLabelToFront("ServerName");

		// Hidden: the map name
		char mapname[256];	// REBUILD: name
		Q_FileBase( engine->GetLevelName(), mapname, 255 );
		SetControlString("MapName", mapname );
		MoveLabelToFront("MapName");
	}

	// Hidden: update instantly on these
	else if ( Q_strcmp(type, "game_round_restart") == 0 )
	{
		Update();
	}

	else if ( Q_strcmp(type, "player_connect") == 0 )
	{
		Update();
	}

	else if ( Q_strcmp(type, "player_death") == 0 )
	{
		Update();
	}

	if( IsVisible() )
		Update();

}

bool CClientScoreBoardDialog::NeedsUpdate( void )
{
	return (m_fNextUpdateTime < gpGlobals->curtime);
		

}

//-----------------------------------------------------------------------------
// Purpose: Recalculate the internal scoreboard data
//-----------------------------------------------------------------------------
void CClientScoreBoardDialog::Update( void )
{
	// Set the title
	
	// Reset();
	m_pPlayerList->DeleteAllItems();
	
	FillScoreBoard();

	// grow the scoreboard to fit all the players
	int wide, tall;
	m_pPlayerList->GetContentSize(wide, tall);
	wide = GetWide();
	if (m_iDesiredHeight < tall)
	{
		SetSize(wide, tall);
		m_pPlayerList->SetSize(wide, tall);
	}
	else
	{
		SetSize(wide, m_iDesiredHeight);
		m_pPlayerList->SetSize(wide, m_iDesiredHeight);
	}

	MoveToCenterOfScreen();

	// update every second
	m_fNextUpdateTime = gpGlobals->curtime + 1.0f; 
}

//-----------------------------------------------------------------------------
// Purpose: Sort all the teams
//-----------------------------------------------------------------------------
void CClientScoreBoardDialog::UpdateTeamInfo()
{
// TODO: work out a sorting algorthim for team display for TF2
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CClientScoreBoardDialog::UpdatePlayerInfo()
{
	m_iSectionId = 0; // 0'th row is a header
	int selectedRow = -1;

	// walk all the players and make sure they're in the scoreboard
	for ( int i = 0; i < gpGlobals->maxClients + 1; i++ )
	{
		IGameResources *gr = GameResources();

		if ( gr && gr->IsConnected( i ) )
		{
			// add the player to the list
			KeyValues *playerData = new KeyValues("data");
			GetPlayerScoreInfo( i, playerData );

	
			const char *oldName = playerData->GetString("name","");
			int bufsize = strlen(oldName) * 2;
			char *newName = (char *)_alloca( bufsize );

			UTIL_MakeSafeName( oldName, newName, bufsize );

			playerData->SetString("name", newName);

			int itemID = FindItemIDForPlayerIndex( i );
  			int sectionID = gr->GetTeam( i );
			
			if ( gr->IsLocalPlayer( i ) )
			{
				selectedRow = itemID;
			}
			if (itemID == -1)
			{
				// add a new row
				itemID = m_pPlayerList->AddItem( sectionID, playerData );
			}
			else
			{
				// modify the current row
				m_pPlayerList->ModifyItem( itemID, sectionID, playerData );
			}

			// Hidden: the local player's row is blue, the others are coloured by team
			if ( Q_strcmp( C_BasePlayer::GetLocalPlayer()->GetPlayerName(), playerData->GetString("name","") ) == 0 )
			{
				m_pPlayerList->SetItemFgColor( itemID, Color( 20, 20, 200, 160 ) );
			}
			else
			{
				if ( gr->GetTeam( i ) == TEAM_HIDDEN )
					m_pPlayerList->SetItemFgColor( itemID, Color( 255, 50, 50, 160 ) );

				if ( gr->GetTeam( i ) == TEAM_IRIS )
					m_pPlayerList->SetItemFgColor( itemID, Color( 0, 0, 0, 160 ) );

				if ( gr->GetTeam( i ) == TEAM_SPECTATOR )
					m_pPlayerList->SetItemFgColor( itemID, Color( 52, 173, 32, 200 ) );
			}

			playerData->deleteThis();
		}
		else
		{
			// remove the player
			int itemID = FindItemIDForPlayerIndex( i );
			if (itemID != -1)
			{
				m_pPlayerList->RemoveItem(itemID);
			}
		}
	}

	if ( selectedRow != -1 )
	{
		m_pPlayerList->SetSelectedItem(selectedRow);
	}
}

//-----------------------------------------------------------------------------
// Purpose: adds the top header of the scoreboars
//			Hidden: an empty header, then one section each for IRIS, the Hidden
//			and the spectators, always visible
//-----------------------------------------------------------------------------
void CClientScoreBoardDialog::AddHeader()
{
	// add the top header
	m_iSectionId = 0;
	m_pPlayerList->AddSection(m_iSectionId, "", StaticPlayerSortFunc);

	// IRIS
	m_iSectionId = TEAM_IRIS;
	m_pPlayerList->AddSection(m_iSectionId, "", StaticPlayerSortFunc);
	m_pPlayerList->SetSectionAlwaysVisible(m_iSectionId);
	m_pPlayerList->AddColumnToSection(m_iSectionId, "name", "", 0, scheme()->GetProportionalScaledValue(NAME_WIDTH - STATUS_WIDTH) );
	m_pPlayerList->AddColumnToSection(m_iSectionId, "status", "", 0, scheme()->GetProportionalScaledValue(STATUS_WIDTH) );
	m_pPlayerList->AddColumnToSection(m_iSectionId, "points", "", 0, scheme()->GetProportionalScaledValue(SCORE_WIDTH) );
	m_pPlayerList->AddColumnToSection(m_iSectionId, "deaths", "", 0, scheme()->GetProportionalScaledValue(DEATH_WIDTH) );
	m_pPlayerList->AddColumnToSection(m_iSectionId, "ping", "", 0, scheme()->GetProportionalScaledValue(PING_WIDTH) );
	m_pPlayerList->AddColumnToSection(m_iSectionId, "voice", "", SectionedListPanel::COLUMN_IMAGE | SectionedListPanel::COLUMN_CENTER, scheme()->GetProportionalScaledValue(VOICE_WIDTH) );

	// the Hidden
	m_iSectionId = TEAM_HIDDEN;
	m_pPlayerList->AddSection(m_iSectionId, "", StaticPlayerSortFunc);
	m_pPlayerList->SetSectionAlwaysVisible(m_iSectionId);
	m_pPlayerList->AddColumnToSection(m_iSectionId, "name", "", 0, scheme()->GetProportionalScaledValue(NAME_WIDTH) );
	m_pPlayerList->AddColumnToSection(m_iSectionId, "points", "", 0, scheme()->GetProportionalScaledValue(SCORE_WIDTH) );
	m_pPlayerList->AddColumnToSection(m_iSectionId, "deaths", "", 0, scheme()->GetProportionalScaledValue(DEATH_WIDTH) );
	m_pPlayerList->AddColumnToSection(m_iSectionId, "ping", "", 0, scheme()->GetProportionalScaledValue(PING_WIDTH) );
	m_pPlayerList->AddColumnToSection(m_iSectionId, "voice", "", SectionedListPanel::COLUMN_IMAGE | SectionedListPanel::COLUMN_CENTER, scheme()->GetProportionalScaledValue(VOICE_WIDTH) );

	// spectators
	m_iSectionId = TEAM_SPECTATOR;
	m_pPlayerList->AddSection(m_iSectionId, "", StaticPlayerSortFunc);
	m_pPlayerList->SetSectionAlwaysVisible(m_iSectionId);
	m_pPlayerList->AddColumnToSection(m_iSectionId, "name", "", 0, scheme()->GetProportionalScaledValue(NAME_WIDTH) );
	m_pPlayerList->AddColumnToSection(m_iSectionId, "points", "", 0, scheme()->GetProportionalScaledValue(SCORE_WIDTH) );
	m_pPlayerList->AddColumnToSection(m_iSectionId, "deaths", "", 0, scheme()->GetProportionalScaledValue(DEATH_WIDTH) );
	m_pPlayerList->AddColumnToSection(m_iSectionId, "ping", "", 0, scheme()->GetProportionalScaledValue(PING_WIDTH) );
	m_pPlayerList->AddColumnToSection(m_iSectionId, "voice", "", SectionedListPanel::COLUMN_IMAGE | SectionedListPanel::COLUMN_CENTER, scheme()->GetProportionalScaledValue(VOICE_WIDTH) );
}

//-----------------------------------------------------------------------------
// Purpose: Adds a new section to the scoreboard (i.e the team header)
//-----------------------------------------------------------------------------
void CClientScoreBoardDialog::AddSection(int teamType, int teamNumber)
{
	if ( teamType == TYPE_TEAM )
	{
		IGameResources *gr = GameResources();

		if ( !gr )
			return;

		// setup the team name
		wchar_t *teamName = localize()->Find( gr->GetTeamName(teamNumber) );
		wchar_t name[64];
		wchar_t string1[1024];
		
		if (!teamName)
		{
			localize()->ConvertANSIToUnicode(gr->GetTeamName(teamNumber), name, sizeof(name));
			teamName = name;
		}

		localize()->ConstructString( string1, sizeof( string1 ), localize()->Find("#Player"), 2, teamName );
		
		m_pPlayerList->AddSection(m_iSectionId, "", StaticPlayerSortFunc);

		m_pPlayerList->AddColumnToSection(m_iSectionId, "name", string1, 0, scheme()->GetProportionalScaledValue(NAME_WIDTH) );
		m_pPlayerList->AddColumnToSection(m_iSectionId, "points", "", 0, scheme()->GetProportionalScaledValue(SCORE_WIDTH) );
		m_pPlayerList->AddColumnToSection(m_iSectionId, "deaths", "", 0, scheme()->GetProportionalScaledValue(DEATH_WIDTH) );
		m_pPlayerList->AddColumnToSection(m_iSectionId, "ping", "", 0, scheme()->GetProportionalScaledValue(PING_WIDTH) );
	}
	else if ( teamType == TYPE_SPECTATORS )
	{
		m_pPlayerList->AddSection(m_iSectionId, "");
		m_pPlayerList->AddColumnToSection(m_iSectionId, "name", "#Spectators", 0, scheme()->GetProportionalScaledValue(NAME_WIDTH));
		m_pPlayerList->AddColumnToSection(m_iSectionId, "points", "", 0, scheme()->GetProportionalScaledValue(SCORE_WIDTH) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Used for sorting players
//-----------------------------------------------------------------------------
bool CClientScoreBoardDialog::StaticPlayerSortFunc(vgui::SectionedListPanel *list, int itemID1, int itemID2)
{
	KeyValues *it1 = list->GetItemData(itemID1);
	KeyValues *it2 = list->GetItemData(itemID2);
	Assert(it1 && it2);

	// first compare points
	int v1 = it1->GetInt("points");
	int v2 = it2->GetInt("points");
	if (v1 > v2)
		return true;
	else if (v1 < v2)
		return false;

	// next compare deaths
	v1 = it1->GetInt("deaths");
	v2 = it2->GetInt("deaths");
	if (v1 > v2)
		return false;
	else if (v1 < v2)
		return true;

	// the same, so compare itemID's (as a sentinel value to get deterministic sorts)
	return itemID1 < itemID2;
}

//-----------------------------------------------------------------------------
// Purpose: Adds a new row to the scoreboard, from the playerinfo structure
//-----------------------------------------------------------------------------
bool CClientScoreBoardDialog::GetPlayerScoreInfo(int playerIndex, KeyValues *kv)
{
	IGameResources *gr = GameResources();

	if (!gr )
		return false;

	kv->SetInt("deaths", gr->GetDeaths( playerIndex ) );
	kv->SetInt("points", gr->GetFrags( playerIndex ) );
	kv->SetInt("ping", gr->GetPing( playerIndex ) ) ;
	kv->SetString("name", gr->GetPlayerName( playerIndex ) );
	kv->SetInt("playerIndex", playerIndex);

	kv->SetInt("voice",	s_VoiceImage[GetClientVoiceMgr()->GetSpeakerStatus( playerIndex - 1) ]);	

	// Hidden: spectators and dead players see who is killed in action
	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();	// REBUILD: name
	if ( pLocalPlayer && ( pLocalPlayer->GetTeamNumber() == TEAM_SPECTATOR || !pLocalPlayer->IsAlive() ) )
	{
		if ( !gr->IsAlive( playerIndex ) || gr->GetTeam( playerIndex ) == TEAM_SPECTATOR )
		{
			kv->SetString("status", "KiA");
		}
	}

/*	// setup the tracker column
	if (g_pFriendsUser)
	{
		unsigned int trackerID = gEngfuncs.GetTrackerIDForPlayer(row);

		if (g_pFriendsUser->IsBuddy(trackerID) && trackerID != g_pFriendsUser->GetFriendsID())
		{
			kv->SetInt("tracker",TrackerImage);
		}
	}
*/
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: reload the player list on the scoreboard
//-----------------------------------------------------------------------------
void CClientScoreBoardDialog::FillScoreBoard()
{
	// update totals information
	UpdateTeamInfo();

	// update player info
	UpdatePlayerInfo();
} 

//-----------------------------------------------------------------------------
// Purpose: searches for the player in the scoreboard
//-----------------------------------------------------------------------------
int CClientScoreBoardDialog::FindItemIDForPlayerIndex(int playerIndex)
{
	for (int i = 0; i <= m_pPlayerList->GetHighestItemID(); i++)
	{
		if (m_pPlayerList->IsItemIDValid(i))
		{
			KeyValues *kv = m_pPlayerList->GetItemData(i);
			kv = kv->FindKey(m_iPlayerIndexSymbol);
			if (kv && kv->GetInt() == playerIndex)
				return i;
		}
	}
	return -1;
}




//-----------------------------------------------------------------------------
// Purpose: Sets the text of a control by name
//-----------------------------------------------------------------------------
void CClientScoreBoardDialog::MoveLabelToFront(const char *textEntryName)
{
	Label *entry = dynamic_cast<Label *>(FindChildByName(textEntryName));
	if (entry)
	{
		entry->MoveToFront();
	}
}

