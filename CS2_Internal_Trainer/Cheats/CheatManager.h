#pragma once
#include "Aimbot.h"
#include "ESP.h"
#include "CheatHKeys.h"
#include "ConsoleCheatMenu.h"
#include "MyPointers.h"
#include "GameState.h"
#include "TrampHook.h"
#include "MyD3D11.h"

namespace CheatManager
{
	bool InitHook();
	bool Run();
};