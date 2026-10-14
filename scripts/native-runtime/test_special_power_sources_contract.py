"""Special-power source selection contract (x64-31 fix5).

Native review x64-31-fix4: a Daisy Cutter fired while a supply-drop cargo plane was in flight was dispatched with
the cargo plane (not the Command Center) as source; the plane carries a scripted copy of SuperweaponDaisyCutter
(CreateLocation = USE_OWNER_OBJECT), so no B-52 was created. Source selection must therefore skip objects flagged
KINDOF_IGNORED_IN_GUI, and every object that carries such duplicated scripted powers must be flagged.
"""
import os
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(path):
    return (ROOT / path).read_bytes().decode("latin1").replace("\r\n", "\n")


def big_entries(path):
    data = path.read_bytes()
    count = int.from_bytes(data[8:12], "big")
    pos = 16
    out = {}
    for _ in range(count):
        off = int.from_bytes(data[pos:pos + 4], "big")
        size = int.from_bytes(data[pos + 4:pos + 8], "big")
        end = data.index(b"\0", pos + 8)
        name = data[pos + 8:end].decode("latin1").lower().replace("/", "\\")
        pos = end + 1
        out[name] = data[off:off + size].decode("latin1").replace("\r", "")
    return out


class SpecialPowerSourceContract(unittest.TestCase):
    def test_player_shortcut_source_selection_ignores_gui_ignored_objects(self):
        p = read("Generals/Code/GameEngine/Source/Common/RTS/Player.cpp")
        self.assertIn("static Bool isShortcutSourceCandidate( const Object *obj )", p)
        self.assertIn("return !obj->isKindOf( KINDOF_IGNORED_IN_GUI );", p)
        for fn in ("static void doFindSpecialPowerSourceObject", "static void doCountSpecialPowersReady"):
            body = p[p.index(fn):p.index(fn) + 400]
            self.assertIn("isShortcutSourceCandidate( obj )", body, fn)

    def test_delivery_trace_budget_is_per_aircraft(self):
        d = read("Generals/Code/GameEngine/Source/GameLogic/Object/Update/AIUpdate/DeliverPayloadAIUpdate.cpp")
        self.assertIn("perObject[(unsigned)getObject()->getID()]++ < 40", d)

    def test_every_transport_with_scripted_owner_powers_is_gui_ignored_in_retail_data(self):
        game = os.environ.get("GENERALS_GAME_DIR")
        ini = Path(game) / "INI.big" if game else None
        if not ini or not ini.exists():
            self.skipTest("GENERALS_GAME_DIR with INI.big not available")
        files = big_entries(ini)
        checked = []
        for name, text in files.items():
            if not name.startswith("data\\ini\\object\\") or not name.endswith(".ini"):
                continue
            for m in re.finditer(r"\nObject (\w+)\b([\s\S]*?)\nEnd\n", text):
                obj, body = m.group(1), m.group(2)
                kind = " ".join(re.findall(r"\n\s*KindOf\s*=\s*([^\n;]*)", body))
                for block in re.findall(r"Behavior\s*=\s*OCLSpecialPower[\s\S]*?\n  End", body):
                    if "USE_OWNER_OBJECT" not in block:
                        continue
                    scripted_only = re.search(r"ScriptedSpecialPowerOnly\s*=\s*Yes", block, re.I)
                    if "IGNORED_IN_GUI" in kind or scripted_only:
                        checked.append(obj)
                        continue
                    # Ground units that are themselves the source (ConvoyTruckArmedWithNuke) are legitimately
                    # selectable; aircraft that re-task themselves as delivery transports are not.
                    self.assertNotIn("AIRCRAFT", kind.split(), obj + " is a selectable aircraft special-power source")
        self.assertIn("AmericaJetCargoPlane", checked)
        self.assertIn("AmericaJetB52", checked)


if __name__ == "__main__":
    unittest.main()
