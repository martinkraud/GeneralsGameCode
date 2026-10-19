#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""X64.50 AI archaeology: read-only decoder for the DataChunk files the game uses for map / skirmish scripts (.scb, .map script chunks) and BIG archives.

The format is taken from this repository (Core/GameEngine/Source/Common/System/DataChunk.cpp, GameLogic/ScriptEngine/Scripts.cpp, Map/SidesList.cpp):
  table of contents  'CkMp', int count, count x (u8 length, name, u32 id)
  chunk              u32 label id, u16 version, u32 data size, data (children are chunks again)
Scripts: PlayerScriptsList > ScriptList (one per player) > ScriptGroup | Script > OrCondition > Condition, ScriptAction, ScriptActionFalse.
Teams:   ScriptTeams = a sequence of Dicts (u16 pair count; u32 key<<8|type; value).
Nothing here modifies a game file; it only reads what the user's own installation contains. Decoded content is derived from retail data and is therefore NOT committed:
only aggregate statistics are (see ai_audit_stats.py).
"""
import re
import struct

PARAM = ['INT', 'REAL', 'SCRIPT', 'TEAM', 'COUNTER', 'FLAG', 'COMPARISON', 'WAYPOINT', 'BOOLEAN', 'TRIGGER_AREA', 'TEXT_STRING', 'SIDE', 'SOUND', 'SCRIPT_SUBROUTINE',
         'UNIT', 'OBJECT_TYPE', 'COORD3D', 'ANGLE']


def read_big(path):
    """Yield (name, bytes) for every entry of a BIG archive."""
    with open(path, 'rb') as f:
        d = f.read()
    if d[:4] not in (b'BIGF', b'BIG4'):
        raise ValueError('not a BIG archive: %s' % path)
    n = struct.unpack('>I', d[8:12])[0]
    p = 16
    for _ in range(n):
        off, size = struct.unpack('>II', d[p:p + 8])
        p += 8
        e = d.index(b'\0', p)
        name = d[p:e].decode('latin1')
        p = e + 1
        yield name, d[off:off + size]


def enum_names(header_text, enum_name):
    """Names of the members of `enum <enum_name> { ... NUM_ITEMS }` in declaration order (explicit '= value' is not supported: the script enums use none)."""
    i = header_text.index('enum ' + enum_name)
    j = header_text.index('{', i)
    k = header_text.index('NUM_ITEMS', j)
    body = header_text[j + 1:k]
    body = re.sub('//.*', '', body)
    body = re.sub(r'/\*.*?\*/', '', body, flags=re.S)
    names = []
    for part in body.split(','):
        m = re.match(r'\s*(\w+)', part)
        if m:
            names.append(m.group(1))
    return names


class Chunked:
    def __init__(self, data):
        if data[:4] != b'CkMp':
            raise ValueError('no table of contents')
        self.d = data
        count = struct.unpack('<i', data[4:8])[0]
        p = 8
        self.table = {}
        for _ in range(count):
            ln = data[p]
            p += 1
            name = data[p:p + ln].decode('latin1')
            p += ln
            self.table[struct.unpack('<I', data[p:p + 4])[0]] = name
            p += 4
        self.first = p

    def chunks(self, p, end):
        out = []
        while p + 10 <= end:
            ident, ver, size = struct.unpack('<IHI', self.d[p:p + 10])
            p += 10
            out.append((self.table.get(ident, '?%d' % ident), ver, p, p + size))
            p += size
        return out

    def top(self):
        return self.chunks(self.first, len(self.d))

    # primitive readers over a position
    def u8(self, p): return self.d[p], p + 1
    def i32(self, p): return struct.unpack('<i', self.d[p:p + 4])[0], p + 4
    def f32(self, p): return struct.unpack('<f', self.d[p:p + 4])[0], p + 4

    def s(self, p):
        n = struct.unpack('<H', self.d[p:p + 2])[0]
        return self.d[p + 2:p + 2 + n].decode('latin1'), p + 2 + n

    def dict(self, p):
        n = struct.unpack('<H', self.d[p:p + 2])[0]
        p += 2
        out = {}
        for _ in range(n):
            ka, p = self.i32(p)
            t = ka & 0xff
            key = self.table.get(ka >> 8, '?')
            if t == 0: v, p = self.u8(p)
            elif t == 1: v, p = self.i32(p)
            elif t == 2: v, p = self.f32(p)
            elif t == 3: v, p = self.s(p)
            elif t == 4:
                l = struct.unpack('<H', self.d[p:p + 2])[0]
                v = self.d[p + 2:p + 2 + 2 * l].decode('utf-16le')
                p += 2 + 2 * l
            else:
                raise ValueError('bad dict type %d' % t)
            out[key] = v
        return out, p

    def teams(self):
        for name, ver, p, end in self.top():
            if name == 'ScriptTeams':
                res = []
                while p < end:
                    t, p = self.dict(p)
                    res.append(t)
                return res
        return []

    def parm(self, p):
        t, p = self.i32(p)
        if t == 16:
            x, p = self.f32(p); y, p = self.f32(p); z, p = self.f32(p)
            return 'COORD3D:(%.0f,%.0f,%.0f)' % (x, y, z), p
        i, p = self.i32(p); f, p = self.f32(p); s, p = self.s(p)
        tn = PARAM[t] if t < len(PARAM) else 'P%d' % t
        if s:
            return '%s:%s' % (tn, s), p
        if tn in ('REAL', 'ANGLE'):
            return '%s:%g' % (tn, f), p
        return '%s:%d' % (tn, i), p

    def item(self, c, kind, names):
        """One Condition ('C') or ScriptAction ('A') chunk -> (name, [params as text])."""
        _, ver, p, end = c
        typ, p = self.i32(p)
        name = names[typ] if names and 0 <= typ < len(names) else '#%d' % typ
        if ver >= (4 if kind == 'C' else 2):
            key, p = self.i32(p)
            name = self.table.get(key >> 8, name)
        n, p = self.i32(p)
        params = []
        for _ in range(n):
            t, p = self.parm(p)
            params.append(t)
        return name, params

    def script(self, c, action_names, cond_names):
        _, ver, p, end = c
        name, p = self.s(p); _, p = self.s(p); _, p = self.s(p); _, p = self.s(p)
        flags = []
        for _ in range(6):
            b, p = self.u8(p)
            flags.append(b)
        delay = 0
        if ver >= 2:
            delay, p = self.i32(p)
        s = {'name': name, 'active': flags[0], 'once': flags[1], 'easy': flags[2], 'normal': flags[3], 'hard': flags[4], 'subroutine': flags[5], 'delay': delay,
             'or': [], 'actions': [], 'else': []}
        for sub in self.chunks(p, end):
            if sub[0] == 'OrCondition':
                s['or'].append([self.item(cc, 'C', cond_names) for cc in self.chunks(sub[2], sub[3]) if cc[0] == 'Condition'])
            elif sub[0] == 'ScriptAction':
                s['actions'].append(self.item(sub, 'A', action_names))
            elif sub[0] == 'ScriptActionFalse':
                s['else'].append(self.item(sub, 'A', action_names))
        return s

    def script_lists(self, action_names=None, cond_names=None):
        """-> list (per player) of {'groups': [{'name', 'scripts': [...]}, ...], 'scripts': [...ungrouped...]}"""
        lists = []
        for name, ver, p, end in self.top():
            if name != 'PlayerScriptsList':
                continue
            for ln, lv, lp, lend in self.chunks(p, end):
                if ln != 'ScriptList':
                    continue
                cur = {'groups': [], 'scripts': []}
                for sn, sv, sp, send in self.chunks(lp, lend):
                    if sn == 'ScriptGroup':
                        g, q = self.s(sp)
                        act, q = self.u8(q)
                        if sv >= 2:
                            _, q = self.u8(q)
                        grp = {'name': g, 'active': act, 'scripts': []}
                        for ss in self.chunks(q, send):
                            if ss[0] == 'Script':
                                grp['scripts'].append(self.script(ss, action_names, cond_names))
                        cur['groups'].append(grp)
                    elif sn == 'Script':
                        cur['scripts'].append(self.script((sn, sv, sp, send), action_names, cond_names))
                lists.append(cur)
        return lists

    def player_names(self):
        for name, ver, p, end in self.top():
            if name == 'ScriptsPlayers':
                return re.findall(rb'[A-Za-z][A-Za-z0-9_]{3,}', self.d[p:end])
        return []
