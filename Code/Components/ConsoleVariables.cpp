#include "ConsoleVariables.h"
#include <CrySystem/IConsole.h>
#include <CrySystem/ConsoleRegistration.h>

void CConsoleVariables::RegisterCVars()
{
	ConsoleRegistrationHelper::RegisterFloat("g_WalkSpeed", 0, VF_RESTRICTEDMODE, "Player Walk Speed");
}

void CConsoleVariables::UnregisterCVars()
{
	IConsole* pConsole = gEnv->pConsole;
	pConsole->UnregisterVariable("g_WalkSpeed",true);
}
