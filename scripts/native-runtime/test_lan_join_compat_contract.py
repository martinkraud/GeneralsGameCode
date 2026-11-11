"""Contract for the LAN join compatibility check (Public Alpha gate): enforced in both titles on native 64-bit builds, wire format unchanged."""
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]


def read(rel):
    return (ROOT / rel).read_text(encoding="utf-8", errors="replace").replace("\r\n", "\n")


HANDLERS = "Core/GameEngine/Source/GameNetwork/LANAPIhandlers.cpp"
HEADER = "Core/GameEngine/Include/GameNetwork/LanJoinCompat.h"


class LanJoinCompatContract(unittest.TestCase):
    def test_the_check_is_enforced_for_zero_hour_on_native_64_bit(self):
        text = read(HANDLERS)
        self.assertIn("#if !RTS_ZEROHOUR || defined(_WIN64)", text)
        block = text[text.index("#if !RTS_ZEROHOUR || defined(_WIN64)"):][:1200]
        self.assertIn("LanJoinCompat::compatible(", block)
        self.assertIn("RET_CRC_MISMATCH", block)
        self.assertIn('"join_denied_incompatible_build"', block)      # visible in generals64.log (Release has no network log)

    def test_the_wire_format_is_unchanged(self):
        api = read("Core/GameEngine/Include/GameNetwork/LANAPI.h")
        join = api[api.index("// GameToJoin is sent with REQUEST_JOIN"):][:260]
        for field in ("UnsignedInt gameIP;", "UnsignedInt exeCRC;", "UnsignedInt iniCRC;", "char serial[g_maxSerialLength];"):
            self.assertIn(field, join)
        self.assertIn("RET_CRC_MISMATCH,		// OnGameJoin", api)                     # same reason code, same position

    def test_the_rule_is_pure_and_classic_keeps_the_retail_text(self):
        header = read(HEADER)
        self.assertEqual([l for l in header.splitlines() if l.startswith("#include")], [])
        callbacks = read("Core/GameEngine/Source/GameNetwork/LANAPICallbacks.cpp")
        block = callbacks[callbacks.index("case RET_CRC_MISMATCH:"):][:420]
        self.assertIn("DisplayRuntime::modern() && !ModernExperience::classic()", block)
        self.assertIn('TheGameText->fetch("LAN:ErrorCRCMismatch")', block)       # Classic: the retail string

    def test_the_documentation_states_the_new_behaviour(self):
        plan = read("docs/modernization/LAN_MULTIPLAYER_TEST_PLAN.md")
        self.assertIn("LanJoinCompat", plan)


if __name__ == "__main__":
    unittest.main()
