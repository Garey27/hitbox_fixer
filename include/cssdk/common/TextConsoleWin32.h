/*
*
*    This program is free software; you can redistribute it and/or modify it
*    under the terms of the GNU General Public License as published by the
*    Free Software Foundation; either version 2 of the License, or (at
*    your option) any later version.
*
*    This program is distributed in the hope that it will be useful, but
*    WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
*    General Public License for more details.
*
*    You should have received a copy of the GNU General Public License
*    along with this program; if not, write to the Free Software Foundation,
*    Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*
*    In addition, as a special exception, the author gives permission to
*    link the code of this program with the Half-Life Game Engine ("HL
*    Engine") and Modified Game Libraries ("MODs") developed by Valve,
*    L.L.C ("Valve").  You must obey the GNU General Public License in all
*    respects for all of the code used other than the HL Engine and MODs
*    from Valve.  If you modify this file, you may extend this exception
*    to your version of the file, but you are not obligated to do so.  If
*    you do not wish to do so, delete this exception statement from your
*    version.
*
*/

#pragma once

#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#include <windows.h>
#include "TextConsole.h"

class CTextConsoleWin32: public CTextConsole {
public:
	virtual ~CTextConsoleWin32();

	bool Init(IBaseSystem *system = nullptr);
	void ShutDown();

	void SetTitle(char *pszTitle);
	void SetStatusLine(char *pszStatus);
	void UpdateStatus();

	void PrintRaw(char * pszMsz, int nChars = 0);
	void Echo(char * pszMsz, int nChars = 0);
	char *GetLine();
	int GetWidth();

	void SetVisible(bool visible);
	void SetColor(WORD);

private:
	HANDLE hinput;		// standard input handle
	HANDLE houtput;		// standard output handle
	WORD Attrib;		// attrib colours for status bar

	char statusline[81];	// first line in console is status line
};

extern CTextConsoleWin32 console;
