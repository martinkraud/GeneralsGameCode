/*
**	Command & Conquer Generals(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// GameMain.cpp
// The main entry point for the game
// Author: Michael S. Booth, April 2001

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine
#include "Common/StartupDiagnostics.h"
#if defined(_WIN64)
#include "WW3D2/Backend/NativeGraphicsSmoke.h"
#include "GameClient/Display.h"
#endif

#include "Common/FramePacer.h"
#include "Common/GameEngine.h"
#include "Common/ReplaySimulation.h"


/**
 * This is the entry point for the game system.
 */
Int GameMain()
{
	int exitcode = 0;
	// initialize the game engine using factory function
	TheFramePacer = new FramePacer();
	TheFramePacer->enableFramesPerSecondLimit(TRUE);
	TheGameEngine = CreateGameEngine();
	StartupDiagnostics::checkpoint("engine_init", "begin");
	TheGameEngine->init();
	StartupDiagnostics::checkpoint("engine_init", "complete", TheGlobalData->getPath_UserData().str());

	if (StartupDiagnostics::enabled())
	{
#if defined(_WIN64)
        if (StartupDiagnostics::ordinaryHost())
        {
            // Ordinary application/display lifetime; world/GameLogic/execute stay closed.
            if (!TheGlobalData->m_headless || !StartupDiagnostics::healthy() || !TheDisplay) exitcode = 1;
            else if (StartupDiagnostics::ordinaryGame()) exitcode = TheDisplay->runOrdinaryGameHost();
            else if (StartupDiagnostics::ordinaryMenu()) exitcode = TheDisplay->runOrdinaryMenuHost();
            else exitcode = TheDisplay->runOrdinaryHostGate();
            StartupDiagnostics::checkpoint("ordinary_host_boundary", exitcode ? "fail" : "complete",
                StartupDiagnostics::ordinaryPlay() ? "ordinary game loop; GameLogic live; native D3D11 world objects and interactive input; execute/replay not entered" : StartupDiagnostics::ordinaryTerrain() ? "ordinary game loop; GameLogic live; terrain-only D3D11 world pass; execute/replay not entered" : StartupDiagnostics::ordinaryGame() ? "ordinary game loop; GameLogic live; no world rendering; execute/replay not entered"
                                                   : "execute/replay/simulation/GameLogic skipped", exitcode);
        }
        else if (StartupDiagnostics::displaySmoke())
        {
            if (!TheGlobalData->m_headless || !StartupDiagnostics::healthy() || !TheDisplay) exitcode = 1;
            else exitcode = TheDisplay->runNativePresentationSmoke();
            StartupDiagnostics::checkpoint("graphics_boundary", exitcode ? "fail" : "complete",
                "execute/replay/simulation skipped", exitcode);
        }
        else if (StartupDiagnostics::graphicsSmoke())
        {
            if (!TheGlobalData->m_headless || !StartupDiagnostics::healthy()) exitcode = 1;
            else {
                NativeGraphicsSmoke::Configuration config; // hardware, visible, bounded defaults
                config.sink = [](void *, const char *stage, const char *status, const char *detail, unsigned code) {
                    StartupDiagnostics::checkpoint(stage, status, detail, code);
                };
                exitcode = NativeGraphicsSmoke::Run(config);
            }
            StartupDiagnostics::checkpoint("graphics_boundary", exitcode ? "fail" : "complete",
                "execute/replay/simulation skipped", exitcode);
        }
        else
#endif
        {
		if (!TheGlobalData->m_headless || !StartupDiagnostics::healthy()) exitcode = 1;
		StartupDiagnostics::checkpoint("init_only", "complete", "execute/replay/simulation skipped");
        }
	}
	else if (!TheGlobalData->m_simulateReplays.empty())
	{
		exitcode = ReplaySimulation::simulateReplays(TheGlobalData->m_simulateReplays, TheGlobalData->m_simulateReplayJobs);
	}
	else
	{
		// run it
		TheGameEngine->execute();
	}

	// since execute() returned, we are exiting the game
	StartupDiagnostics::checkpoint("engine_cleanup", "begin");
	delete TheFramePacer;
	TheFramePacer = nullptr;
	delete TheGameEngine;
	TheGameEngine = nullptr;
	StartupDiagnostics::checkpoint("engine_cleanup", "complete");

	return exitcode;
}

