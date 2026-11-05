"""The exact, reviewed X64.44 edit to Generals GameEngine.cpp, applied to the accepted baseline by the contracts that keep that file
byte-identical to it (test_ordinary_menu_contract, test_ordinary_game_contract).

X64.44 adds the opt-in endurance telemetry hooks (inert unless GENERALS_ENDURANCE is set): one include, one engineFrame() call at the top of
GameEngine::update and a LogicTick scope around the single TheGameLogic->UPDATE(). A change outside these three hunks still fails the original
contract; nothing in the contract is weakened.

The multiplayer smoothness audit adds two more measurement-only lines of the same kind (inert unless GENERALS_FRAMEDELIVERY is set): its include after the
endurance include and a FrameDeliveryTelemetry::Tick scope next to the LogicTick scope. They are applied here as well.
"""

INCLUDE_ANCHOR = '#include "Common/PerformanceProfile.h"\n'
INCLUDE_NEW = '#include "Common/EnduranceTelemetry.h"   // X64.44: opt-in endurance time series (inert unless GENERALS_ENDURANCE)\n'
SCOPE_ANCHOR = "\tPERFORMANCE_PROFILE_SCOPE(EngineUpdate);\n"
SCOPE_NEW = "\tEnduranceTelemetry::engineFrame();\n"
UPDATE_ANCHOR = "\t\t\tTheGameLogic->UPDATE();\n"
UPDATE_NEW = "\t\t\t{\n\t\t\t\tEnduranceTelemetry::LogicTick enduranceTick;\n\t\t\t\tTheGameLogic->UPDATE();\n\t\t\t}\n"


FRAME_INCLUDE_NEW = '#include "Common/FrameDeliveryTelemetry.h"   // multiplayer smoothness audit: opt-in frame delivery log (inert unless GENERALS_FRAMEDELIVERY)\n'
FRAME_SCOPE_NEW = "\t\t\t\tFrameDeliveryTelemetry::Tick deliveryTick;\n"


def with_endurance_hooks(baseline_text):
    text = baseline_text.replace("\r\n", "\n")
    for anchor in (INCLUDE_ANCHOR, SCOPE_ANCHOR, UPDATE_ANCHOR):
        assert text.count(anchor) == 1, "baseline anchor is not unique: " + anchor.strip()
    text = text.replace(INCLUDE_ANCHOR, INCLUDE_ANCHOR + INCLUDE_NEW + FRAME_INCLUDE_NEW)
    text = text.replace(SCOPE_ANCHOR, SCOPE_ANCHOR + SCOPE_NEW)
    return text.replace(UPDATE_ANCHOR, UPDATE_NEW.replace("\t\t\t\tTheGameLogic->UPDATE();", FRAME_SCOPE_NEW + "\t\t\t\tTheGameLogic->UPDATE();"))
