#include "cbase.h"
#include "hdn_menu.h"
#include "c_baseplayer.h"
#include "iclientmode.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: The marine's loadout: a primary, a secondary and a piece of
//			equipment, sent to the server on "enter"
//-----------------------------------------------------------------------------
class HDNWeaponMenu : public HDNMenu
{
private:
	DECLARE_CLASS_SIMPLE( HDNWeaponMenu, HDNMenu );

public:
	HDNWeaponMenu( vgui::Panel *pParent );
	virtual ~HDNWeaponMenu() {}

	vgui::VPANEL GetVPanel( void ) { return BaseClass::GetVPanel(); }
	virtual bool IsVisible() { return BaseClass::IsVisible(); }
	virtual void SetParent( vgui::VPANEL parent ) { BaseClass::SetParent( parent ); }

	virtual void OnCommand( const char *command );
	virtual void OnThink();

	virtual void InitPanel( vgui::Panel *pParent );
	virtual void InitImages( void );
	virtual void InitButtons( void );
	virtual void InitInfoImages( void );

private:
	// REBUILD: names of the non-virtual helpers
	void SetPrimary( int iPrimary );
	void SetSecondary( int iSecondary );
	void SetEquipment( int iEquipment );
	void SendLoadout( void );

	char	m_szPrimary[64];
	char	m_szSecondary[64];
	char	m_szEquipment[64];
};

HDNWeaponMenu::HDNWeaponMenu( Panel *pParent ) : HDNMenu( pParent )
{
	Init( pParent );

	SetPrimary( m_pPlayer->GetPrimary() );
	SetSecondary( m_pPlayer->GetSecondary() );
	SetEquipment( m_pPlayer->GetEquipment() );
}

void HDNWeaponMenu::InitPanel( Panel *pParent )
{
	SetTitle( "Weapon Selection", true );
	LoadControlSettings( "Resource/UI/Weaponmenu.res" );

	BaseClass::InitPanel( pParent );
}

void HDNWeaponMenu::InitImages( void )
{
	m_Images.AddToTail( dynamic_cast< ImagePanel * >( FindChildByName( "SelectionBG" ) ) );
	m_Images.AddToTail( dynamic_cast< ImagePanel * >( FindChildByName( "WeaponInfo" ) ) );
	m_Images.AddToTail( dynamic_cast< ImagePanel * >( FindChildByName( "PrimaryTick" ) ) );
	m_Images.AddToTail( dynamic_cast< ImagePanel * >( FindChildByName( "SecondaryTick" ) ) );
	m_Images.AddToTail( dynamic_cast< ImagePanel * >( FindChildByName( "EquipmentTick" ) ) );
	m_Images.AddToTail( dynamic_cast< ImagePanel * >( FindChildByName( "STRIKE" ) ) );
	m_Images.AddToTail( dynamic_cast< ImagePanel * >( FindChildByName( "STRIKE1" ) ) );

	BaseClass::InitImages();
}

void HDNWeaponMenu::InitButtons( void )
{
	m_Buttons.AddToTail( dynamic_cast< Button * >( FindChildByName( "riflebutton" ) ) );
	m_Buttons.AddToTail( dynamic_cast< Button * >( FindChildByName( "smgbutton" ) ) );
	m_Buttons.AddToTail( dynamic_cast< Button * >( FindChildByName( "shotgunbutton" ) ) );
	m_Buttons.AddToTail( dynamic_cast< Button * >( FindChildByName( "paintbutton" ) ) );
	m_Buttons.AddToTail( dynamic_cast< Button * >( FindChildByName( "pistolbutton1" ) ) );
	m_Buttons.AddToTail( dynamic_cast< Button * >( FindChildByName( "pistolbutton2" ) ) );
	m_Buttons.AddToTail( dynamic_cast< Button * >( FindChildByName( "equipbutton1" ) ) );
	m_Buttons.AddToTail( dynamic_cast< Button * >( FindChildByName( "equipbutton2" ) ) );
	m_Buttons.AddToTail( dynamic_cast< Button * >( FindChildByName( "equipbutton3" ) ) );
	m_Buttons.AddToTail( dynamic_cast< Button * >( FindChildByName( "equipbutton4" ) ) );
	m_Buttons.AddToTail( dynamic_cast< Button * >( FindChildByName( "equipbutton5" ) ) );
	m_Buttons.AddToTail( dynamic_cast< Button * >( FindChildByName( "enter" ) ) );

	BaseClass::InitButtons();
}

void HDNWeaponMenu::InitInfoImages( void )
{
	m_InfoImages.AddToTail( "selection/fn2000loadout" );
	m_InfoImages.AddToTail( "selection/P90Loadout" );
	m_InfoImages.AddToTail( "selection/ShotLoadout" );
	m_InfoImages.AddToTail( "selection/fn303loadout" );
	m_InfoImages.AddToTail( "selection/57loadout" );
	m_InfoImages.AddToTail( "selection/fnp9loadout" );
	m_InfoImages.AddToTail( "selection/laminfo" );
	m_InfoImages.AddToTail( "selection/flashlightinfo" );
	m_InfoImages.AddToTail( "selection/laginfo" );
	m_InfoImages.AddToTail( "selection/tripinfo" );
	m_InfoImages.AddToTail( "selection/adrenalineinfo" );

	BaseClass::InitInfoImages();
}

//-----------------------------------------------------------------------------
// Purpose: Each choice writes its command and moves the tick to the button
//			(positions in 640x480 units); anything else clears it
//-----------------------------------------------------------------------------
void HDNWeaponMenu::SetPrimary( int iPrimary )
{
	switch ( iPrimary )
	{
	case 0:
		strcpy( m_szPrimary, "primary 0" );
		SetImagePos( m_Images[2], 23, 118 );
		break;
	case 1:
		strcpy( m_szPrimary, "primary 1" );
		SetImagePos( m_Images[2], 23, 140 );
		break;
	case 2:
		strcpy( m_szPrimary, "primary 2" );
		SetImagePos( m_Images[2], 23, 159 );
		break;
	case 3:
		strcpy( m_szPrimary, "primary 3" );
		SetImagePos( m_Images[2], 24, 181 );
		break;
	default:
		strcpy( m_szPrimary, "primary 9" );
		m_Images[2]->SetVisible( false );
		break;
	}
}

void HDNWeaponMenu::SetSecondary( int iSecondary )
{
	switch ( iSecondary )
	{
	case 0:
		strcpy( m_szSecondary, "secondary 0" );
		SetImagePos( m_Images[3], 28, 224 );
		break;
	case 1:
		strcpy( m_szSecondary, "secondary 1" );
		SetImagePos( m_Images[3], 27, 246 );
		break;
	default:
		strcpy( m_szSecondary, "secondary 9" );
		m_Images[3]->SetVisible( false );
		break;
	}
}

void HDNWeaponMenu::SetEquipment( int iEquipment )
{
	switch ( iEquipment )
	{
	case 0:
		strcpy( m_szEquipment, "equip 0" );
		SetImagePos( m_Images[4], 29, 296 );
		break;
	case 1:
		strcpy( m_szEquipment, "equip 1" );
		SetImagePos( m_Images[4], 31, 316 );
		break;
	case 2:
		strcpy( m_szEquipment, "equip 2" );
		SetImagePos( m_Images[4], 32, 339 );
		break;
	case 3:
		strcpy( m_szEquipment, "equip 3" );
		SetImagePos( m_Images[4], 34, 360 );
		break;
	case 4:
		strcpy( m_szEquipment, "equip 4" );
		SetImagePos( m_Images[4], 36, 380 );
		break;
	default:
		strcpy( m_szEquipment, "equip 9" );
		m_Images[4]->SetVisible( false );
		break;
	}
}

void HDNWeaponMenu::SendLoadout( void )
{
	SendCommand( m_szPrimary );
	SendCommand( m_szSecondary );
	SendCommand( m_szEquipment );
	SendCommand( "enter" );

	Close();
}

//-----------------------------------------------------------------------------
// Purpose: The support class has no rifle and no shotgun
//-----------------------------------------------------------------------------
void HDNWeaponMenu::OnThink()
{
	if ( m_pPlayer->GetNewClass() == 1 )
	{
		switch ( m_pPlayer->GetPrimary() )
		{
		case 0:
		case 2:
			strcpy( m_szPrimary, "primary 9" );
			m_Images[2]->SetVisible( false );
			break;
		}

		m_Buttons[0]->SetEnabled( false );
		m_Buttons[2]->SetEnabled( false );
		m_Images[5]->SetVisible( true );
		m_Images[6]->SetVisible( true );
	}
	else
	{
		m_Buttons[0]->SetEnabled( true );
		m_Buttons[2]->SetEnabled( true );
		m_Images[5]->SetVisible( false );
		m_Images[6]->SetVisible( false );
	}

	BaseClass::OnThink();
}

void HDNWeaponMenu::OnCommand( const char *command )
{
	if ( !strncmp( command, "primary", 7 ) )
	{
		int iValue = GetCommandValue( command );
		if ( iValue > -1 && iValue < 4 )
		{
			SetPrimary( iValue );
		}
	}
	else if ( !strncmp( command, "secondary", 9 ) )
	{
		int iValue = GetCommandValue( command );
		if ( iValue > -1 && iValue < 2 )
		{
			SetSecondary( iValue );
		}
	}
	else if ( !strncmp( command, "equip", 5 ) )
	{
		int iValue = GetCommandValue( command );
		if ( iValue > -1 && iValue < 5 )
		{
			SetEquipment( iValue );
		}
	}
	else if ( !strncmp( command, "enter", 5 ) )
	{
		SendLoadout();
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

CON_COMMAND( weaponmenu, "Shows the weapon selection menu" )
{
	HDNWeaponMenu *pMenu = new HDNWeaponMenu( g_pClientMode->GetViewport() );
	pMenu->Activate();
}
