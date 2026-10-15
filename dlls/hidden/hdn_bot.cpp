//========= Hidden: Source ============//
//
// Purpose: CHDNBot, the start of a nav mesh bot. It finds a path to a cheap nav
//			area and steps its goal along it; aiming, shooting and looking for
//			the Hidden are still empty.
//
//			REBUILD: nothing in the server creates a CHDNBot (the tutorial bot is
//			made by sdk_bot_temp.cpp), and there is no entity name for it.
//
//=====================================//

#include "cbase.h"
#include "sdk_player.h"
#include "usercmd.h"
#include "nav_mesh.h"
#include "nav_pathfind.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


class CHDNBot : public CSDKPlayer
{
public:
	DECLARE_CLASS( CHDNBot, CSDKPlayer );

	CHDNBot();
	~CHDNBot();

	virtual void	Think( void );

	void			CheckForHidden( void );
	void			FireWeapon( void );
	void			MoveToGoal( void );
	void			FindRoute( void );

	// REBUILD: the names below are ours.
	CUserCmd		m_cmd;
	CNavArea		*m_pClosestArea;	// the end of the path NavAreaBuildPath found
	CNavArea		*m_pCurrentArea;	// the area the bot stood in when the route was made
	Vector			m_vecGoal;			// the centre of the next area on the path
	int				m_iState;			// 0 find a route, 1 follow it, 2 look, 3 fire
	CUtlLinkedList<CNavArea *, int>	m_Path;
};


CHDNBot::CHDNBot()
{
}

CHDNBot::~CHDNBot()
{
}

void CHDNBot::CheckForHidden( void )
{
}

void CHDNBot::FireWeapon( void )
{
}

void CHDNBot::MoveToGoal( void )
{
	if ( m_Path.Count() > 1 )
	{
		int i = m_Path.Head();

		// Reached the first area: drop it
		if ( m_Path[i] == m_pCurrentArea )
		{
			m_Path.FindAndRemove( m_Path[i] );
			i++;
		}

		m_vecGoal = m_Path[i]->GetCenter();
	}
}

void CHDNBot::FindRoute( void )
{
	m_Path.Purge();

	m_pCurrentArea = TheNavMesh->GetNavArea( GetAbsOrigin() );
	if ( !m_pCurrentArea )
		return;

	ShortestPathCost cost;
	CNavArea *pGoalArea = FindMinimumCostArea( m_pCurrentArea, cost );

	m_pClosestArea = NULL;
	NavAreaBuildPath( m_pCurrentArea, pGoalArea, NULL, cost, &m_pClosestArea );

	if ( m_pClosestArea )
	{
		// Walk the parents back to the start
		m_Path.AddToTail( m_pClosestArea );
		for ( CNavArea *pArea = m_pClosestArea->GetParent(); pArea; pArea = pArea->GetParent() )
			m_Path.AddToHead( pArea );

		m_iState = 1;
	}
}

void CHDNBot::Think( void )
{
	Q_memset( &m_cmd, 0, sizeof( m_cmd ) );

	// REBUILD: five states (the ref has a five-entry jump table); 0, 4 and anything
	// else find a route, the look and fire states do nothing yet.
	switch ( m_iState )
	{
	case 0:
		FindRoute();
		break;
	case 1:
		MoveToGoal();
		break;
	case 2:
		CheckForHidden();
		break;
	case 3:
		FireWeapon();
		break;
	case 4:
		FindRoute();
		break;
	default:
		FindRoute();
		break;
	}
}
