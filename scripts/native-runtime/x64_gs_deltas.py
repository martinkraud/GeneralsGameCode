"""Game Speed: the two accepted hunks in Generals/Code/GameEngine/Source/Common/GameEngine.cpp relative to the pre-Game-Speed baseline.

1. canUpdateGameLogic(): the scheduler pace follows the match rule (TheFramePacer->setGameSpeedPercent(TheGameLogic->getGameSpeed())).
2. canUpdateRegularGameLogic(): the logic rate is read as an exact (fractional) Real, because Game Speed levels are 22.5 / 37.5 / 45 logic frames per second.
"""

HOOK_OLD = "\t// This updates the paused game status of the game logic.\n\tTheGameLogic->preUpdate();\n"
HOOK_NEW = HOOK_OLD + ("\n\t// Game Speed match rule: the scheduler pace follows the match's rule (Normal outside a Skirmish / replay game); render settings never enter into it.\n"
                       "\tTheFramePacer->setGameSpeedPercent(TheGameLogic->getGameSpeed());\n")
RATE_OLD = "\tconst Int logicTimeScaleFps = TheFramePacer->getActualLogicTimeScaleFps(logicTimeQueryFlags);\n"
RATE_NEW = ("\tconst Real logicTimeScaleFps = TheFramePacer->getActualLogicTimeScaleFpsExact(logicTimeQueryFlags);"
            "   // exact: Game Speed levels are fractional (22.5 / 37.5 logic frames per second)\n")


def with_game_speed(baseline_text):
    text = baseline_text.replace("\r\n", "\n")
    for anchor in (HOOK_OLD, RATE_OLD):
        assert text.count(anchor) == 1, "baseline anchor is not unique: " + anchor.strip()
    return text.replace(HOOK_OLD, HOOK_NEW).replace(RATE_OLD, RATE_NEW)
