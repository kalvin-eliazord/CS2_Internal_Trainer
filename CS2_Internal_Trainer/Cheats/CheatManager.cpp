#include "CheatManager.h"

MyD3D11 g_myD3d11{};

HRESULT hkPresent(IDXGISwapChain* pChain, UINT pSyncInterval, UINT pFlags)
{
	if (CheatManager::bHookPtr)
	{
		if (!g_myD3d11.bDrawInit)
			g_myD3d11.bDrawInit = g_myD3d11.InitDraw(pChain);

		CheatManager::Run();
	}

	return g_myD3d11.t_presentGateway(pChain, pSyncInterval, pFlags);
}

bool CheatManager::InitHook()
{
	ConsoleMenu::InitConsole();

	// Get original Present address with the dummy device method
	if (!g_myD3d11.SetO_Present()) return false;

	// Hook original Present address
	TrampHook tHook(
		reinterpret_cast<intptr_t*>(g_myD3d11.o_Present),
		reinterpret_cast<intptr_t*>(hkPresent),
		14); // Present stolen bytes size

	bHookPtr = &tHook.GetbHookRef();
	if (!bHookPtr) return false;

	g_myD3d11.t_presentGateway = reinterpret_cast<MyD3D11::T_Present>(tHook.GetGateway());

	while (!(GetAsyncKeyState(VK_DELETE) & 1)) Sleep(10);

	return true;
}

bool CheatManager::Run()
{
	if (!bHookPtr) return false;

	if (ConsoleMenu::IsOptionChanged())
		ConsoleMenu::PrintCheatOptions();

	if (GameState::IsMatchStarted())
	{
		// Removing TeamCheck by default when in DeathMatch
		if(!ConsoleMenu::bInitTeamCheck && ConsoleMenu::SetDefaultTeamCheck(GameState::IsDeathMatch()))
			ConsoleMenu::PrintCheatOptions();

		// Cheat Features
		if (ConsoleMenu::bAimbot) Aimbot::Start();
		if (ConsoleMenu::bESP)    ESP::Start();
	}
	else
	{
		ConsoleMenu::bInitTeamCheck = false;
		//TODO: ESP->draw the box on myself
	}

	return true;
}