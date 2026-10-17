"""Function-level comparison of AIPathfind.cpp against a frozen reference (used by test_stage4b1_reference.py).

The original Stage4B.1 guard compared the WHOLE file token for token against a 2026-10-09 snapshot. That is only true until the first later change and
it has been false since the X64.35 profiling scopes, so it could neither tell an authorized change from an unauthorized one nor keep protecting the
rest of the file. This module replaces that model:

  * the file is split into top-level definitions (functions, struct blocks) with a brace counter that ignores #else branches;
  * measurement code is removed before comparing (PERFORMANCE_PROFILE_SCOPE, PerformanceProfile::..., pathProfile / dispatch statements, `if (pathProfile.enabled()...)`; X64.43: PathFailureDiag::... statements, the failDiag observer and `if (PathFailureDiag::...)`)
    and braces are ignored, so adding or removing profiler hooks is not a behaviour change - everything else, in order, is;
  * every definition must then equal the reference, EXCEPT definitions listed in an explicit table of authorized changes (milestone, reason, what protects it);
  * definitions added or removed since the reference must be listed too, so the table cannot silently grow, and a listed entry that no longer differs is reported as stale.
"""
import re
import subprocess

ROOT = None   # set by the caller

TOKEN = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|\w+|::|->|[^\s]')
NL = chr(10)


def strip_comments(s):
    out = []
    i = 0
    n = len(s)
    while i < n:
        c = s[i]
        if c == '"' or c == "'":
            q = c
            j = i + 1
            while j < n and s[j] != q:
                j += 2 if s[j] == '\\' else 1
            out.append(s[i:j + 1])
            i = j + 1
        elif s.startswith('//', i):
            j = s.find(NL, i)
            i = n if j < 0 else j
        elif s.startswith('/*', i):
            j = s.find('*/', i + 2)
            end = n if j < 0 else j + 2
            out.append(NL * s.count(NL, i, end) or ' ')
            i = end
        else:
            out.append(c)
            i += 1
    return ''.join(out)


def tokens_flagged(s):
    """(tokens, flags): flag = the token lies in an #else / #elif branch (brace counting ignores those so alternative branches cannot unbalance a definition)."""
    toks, flags = [], []
    stack = []
    for line in strip_comments(s.replace('\r\n', NL)).split(NL):
        st = line.strip()
        if st.startswith('#'):
            d = st[1:].strip()
            if d.startswith(('if', 'ifdef', 'ifndef')):
                stack.append(False)
            elif d.startswith(('else', 'elif')):
                if stack:
                    stack[-1] = True
            elif d.startswith('endif'):
                if stack:
                    stack.pop()
        lt = TOKEN.findall(line)
        toks.extend(lt)
        flags.extend([any(stack)] * len(lt))
    return toks, flags


def _ident(t):
    return re.match(r'\w+$', t) is not None


def _definition_key(toks, brace):
    """(qualified name, parameter text) of the definition whose '{' is toks[brace]; or ('block', head text)."""
    j = brace - 1
    while j >= 0 and toks[j] in ('const', 'override', 'final', 'noexcept', 'volatile'):
        j -= 1
    if j >= 0 and toks[j] == ')':
        params = None
        while True:
            d = 0
            k = j
            while k >= 0:
                if toks[k] == ')':
                    d += 1
                elif toks[k] == '(':
                    d -= 1
                    if d == 0:
                        break
                k -= 1
            if params is None:
                params = ' '.join(toks[k:j + 1])
            m = k - 1
            parts = []
            if m >= 0 and (_ident(toks[m]) or toks[m] == 'operator'):
                parts.append(toks[m])
                m -= 1
                while m >= 1 and toks[m] == '::' and _ident(toks[m - 1]):
                    parts.insert(0, '::')
                    parts.insert(0, toks[m - 1])
                    m -= 2
                if m >= 0 and toks[m] == '~':
                    parts.insert(0, '~')
                    m -= 1
            name = ''.join(parts)
            # constructor initialiser list: `Name::Name(...) : a(1), b(2) {` - continue to the parameter list in front of it
            if m >= 0 and toks[m] in (':', ',') and m >= 1:
                p = m - 1
                if toks[p] == ')':
                    j = p
                    continue
            return (name, params)
    head = []
    m = brace - 1
    while m >= 0 and toks[m] not in (';', '}', '{') and len(head) < 6:
        head.append(toks[m])
        m -= 1
    return ('block', ' '.join(reversed(head)))


def definitions(source):
    toks, flags = tokens_flagged(source)
    result = {}
    n = len(toks)
    i = 0
    while i < n:
        if toks[i] == '{' and not flags[i]:
            key = _definition_key(toks, i)
            d = 0
            k = i
            while k < n:
                if not flags[k]:
                    if toks[k] == '{':
                        d += 1
                    elif toks[k] == '}':
                        d -= 1
                        if d == 0:
                            break
                k += 1
            result.setdefault(key, []).append(toks[i:k + 1])
            i = k + 1
            continue
        i += 1
    return result


PROFILE_IDENTS = ('pathProfile', 'dispatch', 'failDiag')   # X64.43: failDiag is the read-only PathFailureDiag::Search observer


def _skip_statement(t, i):
    d = 0
    n = len(t)
    while i < n:
        if t[i] in '([':
            d += 1
        elif t[i] in ')]':
            d -= 1
        elif t[i] == ';' and d == 0:
            return i + 1
        elif t[i] == '{' and d == 0:
            b = 0
            while i < n:
                if t[i] == '{':
                    b += 1
                elif t[i] == '}':
                    b -= 1
                    if b == 0:
                        return i + 1
                i += 1
            return n
        i += 1
    return n


def normalise(body):
    """Remove measurement code and braces; what is left is the behaviour-bearing token sequence."""
    t = body
    out = []
    i = 0
    n = len(t)
    while i < n:
        x = t[i]
        if x == 'PERFORMANCE_PROFILE_SCOPE':
            i = _skip_statement(t, i)
            continue
        if x == 'PerformanceProfile' and i + 1 < n and t[i + 1] == '::':
            i = _skip_statement(t, i)
            continue
        if x == 'PathFailureDiag' and i + 1 < n and t[i + 1] == '::':   # X64.43 read-only diagnostic hooks
            i = _skip_statement(t, i)
            continue
        if x in PROFILE_IDENTS and i + 1 < n and t[i + 1] == '.':
            i = _skip_statement(t, i)
            continue
        if x == 'if' and i + 3 < n and t[i + 1] == '(' and ((t[i + 2] in PROFILE_IDENTS and t[i + 3] == '.') or (t[i + 2] == 'PathFailureDiag' and t[i + 3] == '::')):
            d = 0
            j = i + 1
            while j < n:
                if t[j] == '(':
                    d += 1
                elif t[j] == ')':
                    d -= 1
                    if d == 0:
                        j += 1
                        break
                j += 1
            i = _skip_statement(t, j)
            continue
        if x in ('{', '}'):
            i += 1
            continue
        if x == ';' and out and out[-1] == ';':
            i += 1
            continue
        out.append(x)
        i += 1
    return out


def git_show(rev, path):
    return subprocess.check_output(['git', 'show', rev + ':' + path], cwd=ROOT, text=True)


def compare(reference_source, current_source, authorized, added_ok, removed_ok):
    """Returns (unauthorized, stale) - lists of readable strings. `authorized`, `added_ok`, `removed_ok` are lists of name prefixes."""
    ref = definitions(reference_source)
    cur = definitions(current_source)

    def listed(entries, key):
        text = key[0] if key[0] != 'block' else 'block:' + key[1]
        return any(text.startswith(e) or (e.startswith('block:') and key[0] == 'block' and e[6:] in key[1]) for e in entries)

    unauthorized = []
    seen_changes = set()
    for key in sorted(set(ref) | set(cur)):
        label = key[0] if key[0] != 'block' else 'block ' + key[1]
        if key not in cur:
            if listed(removed_ok, key):
                seen_changes.add(('removed', label))
            else:
                unauthorized.append('removed: ' + label)
            continue
        if key not in ref:
            if listed(added_ok, key):
                seen_changes.add(('added', label))
            else:
                unauthorized.append('added: ' + label)
            continue
        if [normalise(b) for b in ref[key]] != [normalise(c) for c in cur[key]]:
            if listed(authorized, key):
                seen_changes.add(('changed', label))
            else:
                unauthorized.append('changed: ' + label + ' ' + key[1][:60])
    # stale entries: an authorization that matches nothing that differs
    stale = []
    for kind, entries in (('changed', authorized), ('added', added_ok), ('removed', removed_ok)):
        for e in entries:
            if not any(k == kind and (lab.startswith(e) or (e.startswith('block:') and lab.startswith('block ') and e[6:] in lab)) for k, lab in seen_changes):
                stale.append('%s entry matches nothing: %s' % (kind, e))
    return unauthorized, stale
