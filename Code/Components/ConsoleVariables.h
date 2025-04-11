#pragma once

class CConsoleVariables
{
public:
	CConsoleVariables()
	{

	}
	~CConsoleVariables()
	{
		UnregisterCVars();
	}

	void RegisterCVars();
	void UnregisterCVars();
};