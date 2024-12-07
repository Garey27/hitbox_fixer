//===== Copyright ?1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include <metahook.h>
#include <vgui_controls/Controls.h>
#include <locale.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// interface
#include <VGUI/ISurface.h>
#include <VGUI/IVGui.h>
#include <VGUI/IInput.h>
#include <VGUI/IScheme.h>
#include <VGUI/ISystem.h>
#include <VGUI/ILocalize.h>

extern int g_nYou_Must_Add_Public_Vgui_Controls_Vgui_ControlsCpp_To_Your_Project;

vgui::ISurface *g_pVGuiSurface;
vgui::IPanel *g_pVGuiPanel;
vgui::IInput *g_pVGuiInput;
vgui::IVGui *g_pVGui;
vgui::ISystem *g_pVGuiSystem;
vgui::ISchemeManager *g_pVGuiSchemeManager;
vgui::ILocalize *g_pVGuiLocalize;

namespace vgui
{

static char g_szControlsModuleName[256];

//-----------------------------------------------------------------------------
// Purpose: Initializes the controls
//-----------------------------------------------------------------------------
bool VGui_InitInterfacesList( const char *moduleName, CreateInterfaceFn *factoryList, int numFactories )
{
	g_nYou_Must_Add_Public_Vgui_Controls_Vgui_ControlsCpp_To_Your_Project = 1;
	
	// keep a record of this module name
	strncpy(g_szControlsModuleName, moduleName, sizeof(g_szControlsModuleName));
	g_szControlsModuleName[sizeof(g_szControlsModuleName) - 1] = 0;

	// initialize our locale (must be done for every vgui dll/exe)
	// "" makes it use the default locale, required to make iswprint() work correctly in different languages
	setlocale(LC_CTYPE, "");
	setlocale(LC_TIME, "");
	setlocale(LC_COLLATE, "");
	setlocale(LC_MONETARY, "");

	// Get this factorie
	CreateInterfaceFn thisFactorie = Sys_GetFactoryThis();

	// Get interface
	g_pVGuiSurface = (ISurface *)factoryList[0](VGUI_SURFACE_INTERFACE_VERSION, NULL);
	g_pVGuiPanel = (IPanel *)factoryList[1](VGUI_PANEL_INTERFACE_VERSION, NULL);
	g_pVGuiInput = (IInput *)factoryList[1](VGUI_INPUT_INTERFACE_VERSION, NULL);
	g_pVGui = (IVGui *)factoryList[1](VGUI_IVGUI_INTERFACE_VERSION, NULL);
	g_pVGuiSystem = (ISystem *)factoryList[1](VGUI_SYSTEM_INTERFACE_VERSION, NULL);
	g_pVGuiSchemeManager = (ISchemeManager *)factoryList[1](VGUI_SCHEME_INTERFACE_VERSION, NULL);
	g_pVGuiLocalize = (ILocalize *)thisFactorie(VGUI_LOCALIZE_INTERFACE_VERSION, NULL);

	if (!g_pVGuiLocalize)
		g_pVGuiLocalize = (ILocalize *)factoryList[1](VGUI_LOCALIZE_INTERFACE_VERSION, NULL);

	// NOTE: Vgui expects to use these interfaces which are defined in tier3.lib
	if ( !g_pVGui || !g_pVGuiInput || !g_pVGuiPanel || 
		 !g_pVGuiSurface || !g_pVGuiSchemeManager || !g_pVGuiSystem || !g_pVGuiLocalize )
	{
		Warning( "vgui_controls is missing a required interface!\n" );
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: returns the name of the module this has been compiled into
//-----------------------------------------------------------------------------
const char *GetControlsModuleName()
{
#if 0
	return g_szControlsModuleName;
#else
	return "CSBTE";
#endif
}

} // namespace vgui



