/*
*
*   This program is free software; you can redistribute it and/or modify it
*   under the terms of the GNU General Public License as published by the
*   Free Software Foundation; either version 2 of the License, or (at
*   your option) any later version.
*
*   This program is distributed in the hope that it will be useful, but
*   WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
*   General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software Foundation,
*   Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*
*   In addition, as a special exception, the author gives permission to
*   link the code of this program with the Half-Life Game Engine ("HL
*   Engine") and Modified Game Libraries ("MODs") developed by Valve,
*   L.L.C ("Valve").  You must obey the GNU General Public License in all
*   respects for all of the code used other than the HL Engine and MODs
*   from Valve.  If you modify this file, you may extend this exception
*   to your version of the file, but you are not obligated to do so.  If
*   you do not wish to do so, delete this exception statement from your
*   version.
*
*/
#pragma once

class CCSBot;

// This class represents the game state as known by a particular bot
class CSGameState {
public:
	// bomb defuse scenario
	enum BombState
	{
		MOVING,		// being carried by a Terrorist
		LOOSE,		// loose on the ground somewhere
		PLANTED,	// planted and ticking
		DEFUSED,	// the bomb has been defused
		EXPLODED,	// the bomb has exploded
	};

	bool IsBombMoving() const { return (m_bombState == MOVING); }
	bool IsBombLoose() const { return (m_bombState == LOOSE); }
	bool IsBombPlanted() const { return (m_bombState == PLANTED); }
	bool IsBombDefused() const { return (m_bombState == DEFUSED); }
	bool IsBombExploded() const { return (m_bombState == EXPLODED); }

public:
	CCSBot *m_owner;			// who owns this gamestate
	bool m_isRoundOver;			// true if round is over, but no yet reset

	// bomb defuse scenario
	BombState GetBombState() { return m_bombState; }
	BombState m_bombState;			// what we think the bomb is doing

	IntervalTimer m_lastSawBomber;
	Vector m_bomberPos;

	IntervalTimer m_lastSawLooseBomb;
	Vector m_looseBombPos;

	bool m_isBombsiteClear[4];		// corresponds to zone indices in CCSBotManager
	int m_bombsiteSearchOrder[4];		// randomized order of bombsites to search
	int m_bombsiteCount;
	int m_bombsiteSearchIndex;		// the next step in the search

	int m_plantedBombsite;			// zone index of the bombsite where the planted bomb is

	bool m_isPlantedBombPosKnown;		// if true, we know the exact location of the bomb
	Vector m_plantedBombPos;

	// hostage rescue scenario
	struct HostageInfo
	{
		CHostage *hostage;
		Vector knownPos;
		bool isValid;
		bool isAlive;
		bool isFree;			// not being escorted by a CT
	}
	m_hostage[MAX_HOSTAGES];
	int m_hostageCount;					// number of hostages left in map
	CountdownTimer m_validateInterval;

	bool m_allHostagesRescued;				// if true, so every hostages been is rescued
	bool m_haveSomeHostagesBeenTaken;			// true if a hostage has been moved by a CT (and we've seen it)
};
