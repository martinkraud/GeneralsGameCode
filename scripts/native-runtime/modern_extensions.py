#!/usr/bin/env python3
"""Generals "modern extensions" data overlay (X64.32).

Original Generals ships a special-power shortcut sidebar with only 5 / 5 / 3 slots (USA / China / GLA). Its
superweapon buildings (Particle Uplink Cannon, Nuclear/Neutron Missile Launcher, SCUD Storm), the CIA
Intelligence camp, the Spy Satellite and the GLA Radar Van scan never appear there. Zero Hour solved this with a
taller bar (up to 11 slots, 48x41 slots on a 35 px pitch) and `*FromShortcut` command buttons.

This tool adapts that design to Generals *from the player's own installed Generals archives*, producing loose
override files for a staged game directory:

    Data/INI/CommandButton.ini     retail + five FromShortcut buttons cloned from the retail Generals buttons
    Data/INI/CommandSet.ini        retail + the new entries in the three SpecialPowerShortcut* sets
    Data/INI/PlayerTemplate.ini    retail with SpecialPowerShortcutButtonCount raised to the new slot counts
    Window/GenPowersShortcutBar{US,China,GLA}.wnd   re-laid-out bars (Zero Hour slot geometry)

Nothing EA-derived is stored in the repository; power mechanics, cooldowns, science requirements and targeting
rules are untouched (only which buttons the sidebar offers). Removing the generated files restores retail.
"""
import argparse
import os
import re
import sys
from pathlib import Path

CRLF = "\r\n"

# power -> (new command button, retail button it is cloned from)
NEW_BUTTONS = [
    ("Command_FireParticleUplinkCannonFromShortcut", "Command_FireParticleUplinkCannon"),
    ("Command_CIAIntelligenceFromShortcut", "Command_CIAIntelligence"),
    ("Command_NeutronMissileFromShortcut", "Command_NeutronMissile"),
    ("Command_ScudStormFromShortcut", "Command_ScudStorm"),
    ("Command_RadarVanScanFromShortcut", "Command_RadarVanScan"),
]

# Appended to the retail shortcut sets, in Zero Hour's order (Particle, Spy Satellite, CIA for the USA).
SET_ADDITIONS = {
    "SpecialPowerShortcutUSA": [
        (6, "Command_FireParticleUplinkCannonFromShortcut"),
        (7, "Command_SpySatelliteScanFromCommandCenter"),   # retail button, never on the retail bar
        (8, "Command_CIAIntelligenceFromShortcut"),
    ],
    "SpecialPowerShortcutChina": [
        (6, "Command_NeutronMissileFromShortcut"),
    ],
    "SpecialPowerShortcutGLA": [
        (4, "Command_ScudStormFromShortcut"),
        (5, "Command_RadarVanScanFromShortcut"),
    ],
}

# (window file in Window.big, side name in PlayerTemplate, slot count, bar bottom edge)
BARS = {
    "GenPowersShortcutBarUS.wnd": ("America", 8, 429),
    "GenPowersShortcutBarChina.wnd": ("China", 6, 420),
    "GenPowersShortcutBarGLA.wnd": ("GLA", 5, 421),
}

# Zero Hour slot geometry (WindowZH.big GenPowersShortcutBar*.wnd), 800x600 creation resolution.
SLOT_X, SLOT_W, SLOT_H, SLOT_PITCH = 752, 48, 41, 35
CMD_DX, CMD_DY, CMD_W, CMD_H = 1, 7, 39, 27


# ---------------------------------------------------------------------------------------------------------------
def read_big(path):
    """Returns {lower-case backslash name: bytes} for a BIGF/BIG4 archive."""
    data = Path(path).read_bytes()
    if data[:4] not in (b"BIGF", b"BIG4"):
        raise ValueError("not a BIG archive: %s" % path)
    count = int.from_bytes(data[8:12], "big")
    pos = 16
    out = {}
    for _ in range(count):
        off = int.from_bytes(data[pos:pos + 4], "big")
        size = int.from_bytes(data[pos + 4:pos + 8], "big")
        end = data.index(b"\0", pos + 8)
        name = data[pos + 8:end].decode("latin1").lower().replace("/", "\\")
        out[name] = data[off:off + size]
        pos = end + 1
    return out


def to_text(blob):
    return blob.decode("latin1").replace("\r\n", "\n").replace("\r", "\n")


def to_crlf(text):
    return text.replace("\n", CRLF).encode("latin1")


# ---------------------------------------------------------------------------------------------------------------
def clone_button(ini_text, source, new_name):
    m = re.search(r"\nCommandButton " + re.escape(source) + r"\n([\s\S]*?)\nEnd\b", ini_text)
    if not m:
        raise KeyError("retail command button not found: " + source)
    body = m.group(1)
    body = re.sub(r"(?m)^(\s*Command\s*=\s*)SPECIAL_POWER\b", r"\1SPECIAL_POWER_FROM_SHORTCUT", body)
    if "SPECIAL_POWER_FROM_SHORTCUT" not in body:
        raise ValueError(source + " is not a plain SPECIAL_POWER button")
    opt = re.search(r"(?m)^(\s*)Options(\s*=\s*)([^\n;]*)", body)
    if opt:
        if "NEED_SPECIAL_POWER_SCIENCE" not in opt.group(3):
            body = body[:opt.start(3)] + "NEED_SPECIAL_POWER_SCIENCE " + body[opt.start(3):]
    else:
        body = re.sub(r"(?m)^(\s*SpecialPower\s*=[^\n]*)\n", r"\1\n  Options           = NEED_SPECIAL_POWER_SCIENCE\n", body, count=1)
    return "CommandButton %s\n%s\nEnd\n" % (new_name, body)


def patch_command_buttons(ini_text):
    out = ini_text.rstrip("\n") + "\n"
    out += "\n; ---- X64.32 modern extension: FromShortcut variants cloned from the retail Generals buttons ----\n"
    for new, src in NEW_BUTTONS:
        if re.search(r"\nCommandButton " + re.escape(new) + r"\n", ini_text):
            raise ValueError("already defined: " + new)
        out += "\n" + clone_button(ini_text, src, new)
    return out


def patch_command_sets(ini_text):
    out = ini_text
    for set_name, additions in SET_ADDITIONS.items():
        m = re.search(r"(CommandSet " + set_name + r"\n)([\s\S]*?)(\nEND\b)", out, re.I)
        if not m:
            raise KeyError("retail command set not found: " + set_name)
        used = {int(x) for x in re.findall(r"(?m)^\s*(\d+)\s*=", m.group(2))}
        lines = ""
        for slot, button in additions:
            if slot in used:
                raise ValueError("%s slot %d already used" % (set_name, slot))
            lines += "\n  %d = %s" % (slot, button)
        out = out[:m.end(2)] + lines + out[m.end(2):]
    return out


def patch_player_template(ini_text):
    out = ini_text
    for _, (side, slots, _bottom) in BARS.items():
        m = re.search(r"(?m)^(\s*Side\s*=\s*" + side + r"\s*(?:;[^\n]*)?\n)", out)
        if not m:
            raise KeyError("player template side not found: " + side)
        c = re.compile(r"(?m)^(\s*SpecialPowerShortcutButtonCount\s*=\s*)(\d+)")
        cm = c.search(out, m.end())
        nxt = re.search(r"(?m)^\s*Side\s*=", out[m.end():])
        if not cm or (nxt and cm.start() > m.end() + nxt.start()):
            raise KeyError("ButtonCount not found for " + side)
        out = out[:cm.start(2)] + str(slots) + out[cm.end(2):]
    return out


def build_wnd(retail_text, slots, bottom):
    """Regenerates a shortcut-bar WND with `slots` slots on Zero Hour's geometry (slot 1 on top, bottom anchored)."""
    lines = retail_text.split("\n")
    starts = [i for i, l in enumerate(lines) if l == "  CHILD"]
    ends = [i for i, l in enumerate(lines) if l == "  ENDALLCHILDREN"]
    if not starts or not ends:
        raise ValueError("unexpected WND structure")
    end = max(ends)
    head, template, tail = lines[:starts[0]], lines[starts[0]:starts[1] if len(starts) > 1 else end], lines[end:]
    top = bottom - SLOT_H - SLOT_PITCH * (slots - 1)
    rect = re.compile(r"UPPERLEFT: \d+ \d+,\n(\s*)BOTTOMRIGHT: \d+ \d+,")

    def with_rects(block_lines, prect, crect):
        text = "\n".join(block_lines)
        hits = []

        def repl(m):
            r = hits.__len__()
            hits.append(1)
            x1, y1, x2, y2 = (prect, crect)[min(r, 1)]
            return "UPPERLEFT: %d %d,\n%sBOTTOMRIGHT: %d %d," % (x1, y1, m.group(1), x2, y2)
        return rect.sub(repl, text), len(hits)

    # bar parent rectangle (first rectangle in the file)
    head_text = "\n".join(head)
    head_text, n = rect.subn("UPPERLEFT: %d %d,\n\\1BOTTOMRIGHT: %d %d," % (SLOT_X, top, SLOT_X + SLOT_W, bottom),
                             head_text, count=1)
    if n != 1:
        raise ValueError("bar parent rectangle not found")
    blocks = []
    for k in range(1, slots + 1):
        y = top + SLOT_PITCH * (k - 1)
        prect = (SLOT_X, y, SLOT_X + SLOT_W, y + SLOT_H)
        crect = (SLOT_X + CMD_DX, y + CMD_DY, SLOT_X + CMD_DX + CMD_W, y + CMD_DY + CMD_H)
        text, hits = with_rects(template, prect, crect)
        if hits != 2:
            raise ValueError("slot template must hold a parent and a command rectangle")
        text = re.sub(r"ButtonParent\d+", "ButtonParent%d" % k, text)
        text = re.sub(r"ButtonCommand\d+", "ButtonCommand%d" % k, text)
        blocks.append(text)
    return head_text + "\n" + "\n".join(blocks) + "\n" + "\n".join(tail)


# ---------------------------------------------------------------------------------------------------------------
def build(game_dir, out_dir):
    game = Path(game_dir)
    out = Path(out_dir)
    ini = read_big(game / "INI.big")
    win = read_big(game / "Window.big")
    files = {}
    cb = to_text(ini["data\\ini\\commandbutton.ini"])
    cs = to_text(ini["data\\ini\\commandset.ini"])
    pt = to_text(ini["data\\ini\\playertemplate.ini"])
    files["Data/INI/CommandButton.ini"] = to_crlf(patch_command_buttons(cb))
    files["Data/INI/CommandSet.ini"] = to_crlf(patch_command_sets(cs))
    files["Data/INI/PlayerTemplate.ini"] = to_crlf(patch_player_template(pt))
    for wnd, (_side, slots, bottom) in BARS.items():
        key = "window\\" + wnd.lower()
        files["Window/" + wnd] = to_crlf(build_wnd(to_text(win[key]), slots, bottom))
    for rel, data in files.items():
        target = out / rel
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    return sorted(files)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--game", required=True, help="directory with the retail Generals INI.big / Window.big")
    ap.add_argument("--out", required=True, help="staged game directory to receive the override files")
    args = ap.parse_args(argv)
    for rel in build(args.game, args.out):
        print("wrote", rel)
    return 0


if __name__ == "__main__":
    sys.exit(main())
