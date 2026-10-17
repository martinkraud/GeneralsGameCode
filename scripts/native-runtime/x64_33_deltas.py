"""The exact, reviewed X64.33 edits to files that earlier milestones' contracts keep byte-identical to an accepted
baseline.

Those contracts (test_display_contract, test_ordinary_host_contract, test_ordinary_terrain_contract) protect the
legacy/original code paths from accidental change. X64.33 (Modern Experience display) intentionally extends a few of
those files, so each contract now compares ``revert(path, current)`` with the baseline: everything *except* the
listed X64.33 hunks must still be byte-identical. A change outside these hunks fails the original contract exactly
as before; nothing in the contract is weakened.

Every hunk is a (new_text, old_text) pair in LF line endings.
"""

DELTAS = {
    # X64.48A (second-PC correctness pass): the open combo box owns its list rectangle in the mouse path (Generals Skirmish slots 7/8 lost clicks).
    "Core/GameEngine/Source/GameClient/GUI/GameWindowManager.cpp": [
        ('\t// X64.48A: an open combo box (the "lone window") owns the rectangle of its drop-down list. The list is a child that extends the combo window\n\t// downwards, and the sibling scan below tries windows in creation order: an earlier sibling that takes no input and overlaps the list (the\n\t// Generals Skirmish screen\'s "Starting Cash" label lies over the lists of player slots 7 and 8) was found first and then discarded as "no input",\n\t// so the click on the list row was lost. The open combo is tried first.\n\tif (m_loneWindow && BitIsSet(m_loneWindow->winGetStyle(), GWS_COMBO_BOX) && !BitIsSet(m_loneWindow->m_status, WIN_STATUS_HIDDEN) &&\n\t\tBitIsSet(m_loneWindow->m_status, WIN_STATUS_ENABLED) && m_loneWindow->winPointInWindow(mousePos->x, mousePos->y))\n\t{\n\t\treturn m_loneWindow->winPointInChild(mousePos->x, mousePos->y);\n\t}\n\n', ""),
    ],
    "Core/Libraries/Source/WWVegas/WW3D2/ww3d.cpp": [
        ("""    // X64.33: any valid size; the diagnostic profiles still pass 800x600 and are validated by their own gates.
    if(!configuration.window || configuration.width<1 || configuration.height<1 ||
       configuration.width>16384 || configuration.height>16384)
        throw std::runtime_error("Native presentation requires a window and a valid size");
""", """    if(!configuration.window || configuration.width!=800 || configuration.height!=600)
        throw std::runtime_error("Native presentation requires the fixed diagnostic window");
"""),
        ("    config.software=false;config.diagnostic_windowed=true;config.vsync=configuration.vsync;\n",
         "    config.software=false;config.diagnostic_windowed=true;\n"),
        ("""void WW3D::Resize_Native_Presentation(unsigned width,unsigned height) {
    if(!NativePresentation) throw std::runtime_error("No native presentation to resize");
    static_cast<D3D11Backend *>(NativePresentation->Backend())->Resize(width,height);
    NativePresentation->Resize(PresentationSize{width,height});
}
void WW3D::Set_Native_Vsync(bool enabled) {
    if(NativePresentation) static_cast<D3D11Backend *>(NativePresentation->Backend())->Set_Vsync(enabled);
}
""", ""),
    ],
    "Core/Libraries/Source/WWVegas/WW3D2/PresentationLifecycle.h": [
        ("    unsigned width=800,height=600;\n    bool vsync=false; // X64.33: present with sync interval 1 (Options > VSync)\n",
         "    unsigned width=800,height=600;\n"),
        ("    unsigned Frames() const;\n    void Resize(PresentationSize dimensions); // X64.33: after the backend swap chain was resized\n",
         "    unsigned Frames() const;\n"),
    ],
    # X64.36 (Enhanced gameplay HUD): the reviewed hooks in the push button image draw; no-ops unless Enhanced is active.
    "Core/GameEngineDevice/Source/W3DDevice/GameClient/GUI/Gadget/W3DPushButton.cpp": [
        ("""#include "GameClient/EnhancedHudImage.h"
#include "GameClient/Gadget.h"
""", """#include "GameClient/Gadget.h"
"""),
        ("""		// Enhanced HUD: registered windows draw their bitmap aspect-fitted and centred (no-op for Original)
		EnhancedHudImage::fitImage( window, image, &start, &end );

""", ""),
    ],
    "Core/Libraries/Source/WWVegas/WW3D2/PresentationLifecycle.cpp": [
        ("""void PresentationLifecycle::Resize(PresentationSize dimensions) {
    if(active || !dimensions.width || !dimensions.height || dimensions.width>16384 || dimensions.height>16384)
        throw std::runtime_error("Invalid presentation resize");
    size=dimensions;
}
""", ""),
    ],
}

GAME_WINDOW_MANAGER_SCRIPT = "Core/GameEngine/Source/GameClient/GUI/GameWindowManagerScript.cpp"

_SCRIPT_FILE_DECL = """// File being parsed by winCreateFromScript (layout family for the modern UI anchoring policy).
static const char *s_scriptFileName = nullptr;

"""
_SCRIPT_FILE_SCOPE = """	struct ScriptFileScope
	{
		const char *previous;
		explicit ScriptFileScope( const char *name ) : previous( s_scriptFileName ) { s_scriptFileName = name; }
		~ScriptFileScope() { s_scriptFileName = previous; }
	} scriptFileScope( filename );
"""

_MODERN_BLOCK_START = "\tif( DisplayRuntime::modern() && createRes.x > 0 && createRes.y > 0 )\n"
_MODERN_ELSE = "\telse\n\t{\n"


def revert(path, text):
    """Returns `text` (LF endings) with the reviewed X64.33 hunks of `path` reverted to the accepted baseline."""
    text = text.replace("\r\n", "\n")
    for new, old in DELTAS.get(path, []):
        if new not in text:
            raise AssertionError("expected X64.33 hunk missing from %s: %r" % (path, new[:60]))
        text = text.replace(new, old, 1)
    if path == GAME_WINDOW_MANAGER_SCRIPT:
        for chunk in (_SCRIPT_FILE_DECL, _SCRIPT_FILE_SCOPE):
            if chunk not in text:
                raise AssertionError("expected X64.33 script-file hunk missing from %s" % path)
            text = text.replace(chunk, "", 1)
        # include line
        include = '\n#include "Common/DisplayRuntime.h"\n'
        if include not in text:
            raise AssertionError("expected X64.33 include missing from %s" % path)
        text = text.replace(include, "", 1)
        # modern/classic branch in parseScreenRect: keep only the classic body, de-indented one level
        start = text.index(_MODERN_BLOCK_START)
        else_at = text.index(_MODERN_ELSE, start)
        body_start = else_at + len(_MODERN_ELSE)
        body_end = text.index("\n\t}\n", body_start) + 1
        classic = text[body_start:body_end]
        classic = "".join(line[1:] if line.startswith("\t") else line for line in classic.splitlines(True))
        text = text[:start] + classic + text[body_end + len("\t}\n"):]
    return text


def with_modern_caption_hunk(baseline_text):
    """GameEngine.cpp as accepted plus the reviewed X64.33 closeout hunk: the Modern Experience window caption carries
    no product/author wording (two added lines and one include)."""
    import re
    text = baseline_text.replace("\r\n", "\n")
    line = re.search(r'#include "PreRTS\.h"[^\n]*\n', text)
    text = text[:line.end()] + '#include "Common/ModernExperience.h"\n' + text[line.end():]
    anchor = "\tUnicodeString productString = TheVersion->getUnicodeProductString();\n"
    assert text.count(anchor) == 1
    return text.replace(anchor, anchor +
        "\tif (StartupDiagnostics::normalLaunch() && ModernExperience::enabled(ModernExperience::FEATURE_MODERN_DISPLAY))\n"
        "\t\tproductString.clear();   // Modern Experience: plain game title (no author/patch wording in the caption)\n")
