#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "hud_basetimer.h"
#include "iclientmode.h"
#include "c_baseplayer.h"
#include "sdk_gamerules.h"

#include <vgui_controls/AnimationController.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Time left in the round, from the game rules
//-----------------------------------------------------------------------------
class CHudRoundTimer : public CHudElement, public CHudBaseTimer
{
	DECLARE_CLASS_SIMPLE( CHudRoundTimer, CHudBaseTimer );

public:
	CHudRoundTimer( const char *pElementName );

	virtual void Init( void );
	virtual void VidInit( void );
	virtual void Reset( void );

protected:
	virtual void OnThink( void );

private:
	float			m_flRoundStart;		// the rules' m_fStart the timer was started for
	int				m_iLastTime;		// seconds shown, -1 while there is no round time
	C_BasePlayer	*m_pPlayer;			// REBUILD: stored every think, never read
};

DECLARE_HUDELEMENT( CHudRoundTimer );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudRoundTimer::CHudRoundTimer( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudRoundTimer" )
{
	SetPaintEnabled( false );

	SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD | HIDEHUD_NEEDSUIT );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudRoundTimer::Init( void )
{
	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudRoundTimer::VidInit( void )
{
	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudRoundTimer::Reset( void )
{
	SetSeconds( m_iLastTime );
}

//-----------------------------------------------------------------------------
// Purpose: Follows the round timer and plays its animations
//-----------------------------------------------------------------------------
void CHudRoundTimer::OnThink( void )
{
	m_pPlayer = C_BasePlayer::GetLocalPlayer();

	if ( m_pPlayer && m_pPlayer->GetTeamNumber() == TEAM_HIDDEN )
	{
		SetFgColor( Color( 228, 207, 154, 200 ) );
	}

	CSDKGameRules *pRules = SDKGameRules();
	if ( !pRules )
		return;

	int iTime = pRules->GetRoundTimerRemain();

	if ( iTime < 0 && m_iLastTime < 0 )
		return;

	if ( iTime == m_iLastTime )
		return;

	if ( iTime < 0 )
	{
		SetPaintEnabled( false );
		SetPaintBackgroundEnabled( false );
		m_iLastTime = -1;
		return;
	}

	// A new round started
	if ( m_flRoundStart != pRules->m_fStart || m_iLastTime < 0 )
	{
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "RoundTimerInit" );
		SetPaintEnabled( true );
		SetPaintBackgroundEnabled( false );
		m_flRoundStart = pRules->m_fStart;
	}

	if ( m_iLastTime == 20 )
	{
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "RoundTimerBelow20" );
	}
	else if ( iTime < 10 )
	{
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "RoundTimerPulse" );
	}

	if ( iTime == 5 )
	{
		// REBUILD: the sequence name is misspelt in client.dll
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "RoundimerBelow5" );
	}

	m_iLastTime = iTime;
	SetSeconds( iTime );
}
