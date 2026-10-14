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
