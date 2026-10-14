"""Engine-vocabulary gate for the generated overlay INI (x64-32 first native run: the Zero Hour spelling of the
shortcut command is not a token in a Generals build, so CommandButton.ini failed to parse and the engine stopped
at InGameUI init).

Every field name, Options bit, Command token and RadiusCursorType of a generated CommandButton must exist in the
Generals build's own parser tables, which are read from the source tree.
"""
import re
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import modern_extensions as mx  # noqa: E402
from test_modern_extensions import BUTTONS  # noqa: E402


def source(path):
    return (ROOT / path).read_text(encoding="latin1").replace("\r\n", "\n")


def field_names():
    text = source("Core/GameEngine/Source/GameClient/GUI/ControlBar/ControlBar.cpp")
    start = text.index("CommandButton::s_commandButtonFieldParseTable[] =")
    return set(re.findall(r'\{\s*"(\w+)"', text[start:text.index("{ nullptr,", start)]))


def array_tokens(text, anchor):
    start = text.index(anchor)
    return start, text[start:text.index("nullptr", start)]


def option_names():
    text = source("Core/GameEngine/Include/GameClient/ControlBar.h")
    _, body = array_tokens(text, "TheCommandOptionNames[] =")
    return set(re.findall(r'"([A-Z_]+)"', body))


def command_names():
    text = source("Core/GameEngine/Include/GameClient/ControlBar.h")
    start = text.index("static const char *const TheGuiCommandNames[] =")
    body = text[start:text.index("static_assert(ARRAY_SIZE(TheGuiCommandNames)", start)]
    body = re.sub(r"#if RTS_GENERALS(.*?)#else.*?#endif", lambda m: m.group(1), body, flags=re.S)
    return set(re.findall(r'"([A-Z_0-9]+)"', body))


class GeneratedButtonsUseOnlyEngineVocabulary(unittest.TestCase):
    def setUp(self):
        out = mx.patch_command_buttons(BUTTONS)
        self.blocks = re.findall(r"CommandButton \w+\n([\s\S]*?)\nEnd", out[out.index("; ---- X64.32"):])
        self.assertEqual(len(self.blocks), len(mx.NEW_BUTTONS))

    def test_field_names(self):
        fields = field_names()
        self.assertIn("SpecialPower", fields)
        for block in self.blocks:
            for line in block.split("\n"):
                line = line.split(";")[0].strip()
                if line:
                    self.assertIn(line.partition("=")[0].strip(), fields, line)

    def test_options_bits(self):
        options = option_names()
        self.assertIn("NEED_SPECIAL_POWER_SCIENCE", options)
        for block in self.blocks:
            for m in re.finditer(r"(?m)^\s*Options\s*=\s*([^;\n]*)", block):
                for bit in m.group(1).split():
                    self.assertIn(bit, options, bit)

    def test_command_tokens(self):
        commands = command_names()
        self.assertNotIn("SPECIAL_POWER_FROM_SHORTCUT", commands)   # Zero Hour spelling: not accepted by Generals
        for block in self.blocks:
            m = re.search(r"(?m)^\s*Command\s*=\s*(\w+)", block)
            self.assertIsNotNone(m)
            self.assertIn(m.group(1), commands)


if __name__ == "__main__":
    unittest.main()
