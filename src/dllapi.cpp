#include "precompiled.h"

DLL_FUNCTIONS gFunctionTable =
{
	nullptr,					// pfnGameInit
	nullptr,					// pfnSpawn
	nullptr,					// pfnThink
	nullptr,					// pfnUse
	nullptr,					// pfnTouch
	nullptr,					// pfnBlocked
	nullptr,					// pfnKeyValue
	nullptr,					// pfnSave
	nullptr,					// pfnRestore
	nullptr,					// pfnSetAbsBox
	nullptr,					// pfnSaveWriteFields
	nullptr,					// pfnSaveReadFields
	nullptr,					// pfnSaveGlobalState
	nullptr,					// pfnRestoreGlobalState
	nullptr,					// pfnResetGlobalState
	nullptr,					// pfnClientConnect
	nullptr,					// pfnClientDisconnect
	nullptr,					// pfnClientKill
	nullptr,					// pfnClientPutInServer
	nullptr,					// pfnClientCommand
	nullptr,					// pfnClientUserInfoChanged
	nullptr,					// pfnServerActivate
	nullptr,					// pfnServerDeactivate
	PlayerPreThinkPre,		// pfnPlayerPreThink
	nullptr,					// pfnPlayerPostThink
	nullptr,					// pfnStartFrame
	nullptr,					// pfnParmsNewLevel
	nullptr,					// pfnParmsChangeLevel
	nullptr,					// pfnGetGameDescription
	nullptr,					// pfnPlayerCustomization
	nullptr,					// pfnSpectatorConnect
	nullptr,					// pfnSpectatorDisconnect
	nullptr,					// pfnSpectatorThink
	nullptr,					// pfnSys_Error
	nullptr,					// pfnPM_Move
	nullptr,					// pfnPM_Init
	nullptr,					// pfnPM_FindTextureType
	nullptr,					// pfnSetupVisibility
	nullptr,					// pfnUpdateClientData
	nullptr,					// pfnAddToFullPack
	nullptr,					// pfnCreateBaseline
	nullptr,					// pfnRegisterEncoders
	nullptr,					// pfnGetWeaponData
	nullptr,					// pfnCmdStart
	nullptr,					// pfnCmdEnd
	nullptr,					// pfnConnectionlessPacket
	nullptr,					// pfnGetHullBounds
	nullptr,					// pfnCreateInstancedBaselines
	nullptr,					// pfnInconsistentFile
	nullptr,					// pfnAllowLagCompensation
};

DLL_FUNCTIONS gFunctionTable_Post =
{
	nullptr,					// pfnGameInit
	nullptr,					// pfnSpawn
	nullptr,					// pfnThink
	nullptr,					// pfnUse
	nullptr,					// pfnTouch
	nullptr,					// pfnBlocked
	nullptr,					// pfnKeyValue
	nullptr,					// pfnSave
	nullptr,					// pfnRestore
	nullptr,					// pfnSetAbsBox
	nullptr,					// pfnSaveWriteFields
	nullptr,					// pfnSaveReadFields
	nullptr,					// pfnSaveGlobalState
	nullptr,					// pfnRestoreGlobalState
	nullptr,					// pfnResetGlobalState
	nullptr,					// pfnClientConnect
	nullptr,					// pfnClientDisconnect
	nullptr,					// pfnClientKill
	PutInServer,			// pfnClientPutInServer
	nullptr,					// pfnClientCommand
	nullptr,					// pfnClientUserInfoChanged
	nullptr,					// pfnServerActivate
	nullptr,					// pfnServerDeactivate
	nullptr,					// pfnPlayerPreThink
	PlayerPostThinkPost,	// pfnPlayerPostThink
	StartFramePost,					// pfnStartFrame
	nullptr,					// pfnParmsNewLevel
	nullptr,					// pfnParmsChangeLevel
	nullptr,					// pfnGetGameDescription
	nullptr,					// pfnPlayerCustomization
	nullptr,					// pfnSpectatorConnect
	nullptr,					// pfnSpectatorDisconnect
	nullptr,					// pfnSpectatorThink
	nullptr,					// pfnSys_Error
	nullptr,					// pfnPM_Move
	nullptr,					// pfnPM_Init
	nullptr,					// pfnPM_FindTextureType
	nullptr,					// pfnSetupVisibility
	nullptr,	// pfnUpdateClientData
	AddToFullPackPost,					// pfnAddToFullPack
	nullptr,					// pfnCreateBaseline
	nullptr,					// pfnRegisterEncoders
	nullptr,					// pfnGetWeaponData
	nullptr,					// pfnCmdStart
	nullptr,					// pfnCmdEnd
	nullptr,					// pfnConnectionlessPacket
	nullptr,					// pfnGetHullBounds
	nullptr,					// pfnCreateInstancedBaselines
	nullptr,					// pfnInconsistentFile
	nullptr,					// pfnAllowLagCompensation
};


C_DLLEXPORT int GetEntityAPI2_Pre(DLL_FUNCTIONS* pFunctionTable,
	int* interfaceVersion)
{
	if (!pFunctionTable) {
		UTIL_LogPrintf("GetEntityAPI2 called with null pFunctionTable");
		return(FALSE);
	}
	else if (*interfaceVersion != INTERFACE_VERSION) {
		UTIL_LogPrintf("GetEntityAPI2 version mismatch; requested=%d ours=%d", *interfaceVersion, INTERFACE_VERSION);
		//! Tell metamod what version we had, so it can figure out who is out of date.
		*interfaceVersion = INTERFACE_VERSION;
		return(FALSE);
	}
	memcpy(pFunctionTable, &gFunctionTable, sizeof(DLL_FUNCTIONS));
	return(TRUE);
}

C_DLLEXPORT int GetEntityAPI2_Post(DLL_FUNCTIONS *pFunctionTable,
							  int *interfaceVersion)
{
	if(!pFunctionTable) {
		UTIL_LogPrintf("GetEntityAPI2 called with null pFunctionTable");
		return(FALSE);
	}
	else if(*interfaceVersion != INTERFACE_VERSION) {
		UTIL_LogPrintf("GetEntityAPI2 version mismatch; requested=%d ours=%d", *interfaceVersion, INTERFACE_VERSION);
		//! Tell metamod what version we had, so it can figure out who is out of date.
		*interfaceVersion = INTERFACE_VERSION;
		return(FALSE);
	}
	memcpy(pFunctionTable, &gFunctionTable_Post, sizeof(DLL_FUNCTIONS));
	return(TRUE);
}