"""Enhanced gameplay HUD (USA prototype) source contracts.

The prototype is presentation only: it moves existing windows and draws a procedural frame. These contracts pin that
the Original path is untouched when the Enhanced presentation is not active, that no command/gameplay logic was
duplicated, that the choice is persisted by the Modern Experience settings and that unsupported factions fall back.
"""
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]


def text(rel):
    return (ROOT / rel).read_text(encoding="utf-8", errors="replace").replace("\r\n", "\n")


BAR = "Core/GameEngine/Source/GameClient/GUI/ControlBar/ControlBarEnhanced.cpp"
SCHEME = "Core/GameEngine/Source/GameClient/GUI/ControlBar/ControlBarScheme.cpp"


class EnhancedHudContracts(unittest.TestCase):
    def test_enhanced_code_is_presentation_only(self):
        s = text(BAR)
        for forbidden in ("TheMessageStream", "GameMessage", "appendMessage", "processCommandUI",
                          "ThePlayerList", "doSpecialPower", "TheBuildAssistant", "setRankLevel",
                          "addSkillPoints", "setSkillPoints", "getMoney()->", "withdraw", "deposit",
                          "MSG_", "Xfer", "TheRandom", "GameLogicRandom"):
            self.assertNotIn(forbidden, s, forbidden)
        # the only simulation reads allowed are the shared elapsed time and the game mode (Battle Overview header / skirmish table)
        for use in re.findall(r"TheGameLogic->(\w+)", s):
            self.assertIn(use, ("getFrame", "getGameMode"), use)
        # it only moves/sizes/hides windows and draws rectangles
        for allowed in ("winSetPosition", "winSetSize", "winHide", "drawFillRect"):
            self.assertIn(allowed, s)

    def test_every_entry_point_is_gated_and_original_is_a_no_op(self):
        s = text(BAR)
        apply_ = s[s.index("void ControlBar::applyEnhancedLayout()"):]
        self.assertIn("if( !EnhancedHud::active() || !TheWindowManager )", apply_[:200])
        restore = s[s.index("void ControlBar::restoreOriginalLayout()"):]
        self.assertIn("if( s_barMaster == nullptr && s_powersParent == nullptr )", restore[:400])
        self.assertIn("return;", restore[:400])
        power = s[s.index("void ControlBar::applyEnhancedPowerShortcutLayout()"):]
        self.assertIn("if( !EnhancedHud::active() )", power[:150])
        tip = s[s.index("Bool ControlBar::getEnhancedTooltipPosition"):]
        self.assertIn("if( !EnhancedHud::active()", tip[:200])
        frame = s[s.index("void drawEnhancedHudFrame()"):]
        self.assertIn("if( !EnhancedHud::active()", frame[:150])

    def test_scheme_hooks_keep_original_drawing_when_inactive(self):
        s = text(SCHEME)
        self.assertIn("EnhancedHud::update(", s)
        self.assertIn("m_side.str()", s)              # the faction decision uses the scheme's side
        bg = s[s.index("void ControlBarSchemeManager::drawBackground"):]
        self.assertRegex(bg[:260], r"if\( EnhancedHud::active\(\) \)\s*\{\s*drawEnhancedHudFrame\(\);\s*return;\s*\}\s*if\(m_currentScheme\)")
        fg = s[s.index("void ControlBarScheme::drawForeground"):]
        self.assertIn("if( EnhancedHud::active() )\n\t\treturn;", fg[:300])
        # Original: the previous scheme art call is untouched
        self.assertIn("m_currentScheme->drawBackground( m_multiplier, DisplayRuntime::offsetByControlBarOrigin(offset) );", s)
        init_tail = s[s.index("// Enhanced: move the windows into the island"):]
        self.assertIn("applyEnhancedLayout();", init_tail[:300])
        self.assertIn("restoreOriginalLayout();", init_tail[:300])

    def test_only_usa_and_never_classic(self):
        layout = text("Core/GameEngine/Include/Common/EnhancedHudLayout.h")
        self.assertIn("EnhancedHudTheme::implemented(EnhancedHudTheme::factionOfSide(side))", layout)
        self.assertIn('std::strncmp(side, "America", 7)', text("Core/GameEngine/Include/Common/EnhancedHudTheme.h"))
        self.assertIn('std::strstr(side, "Small") == nullptr', layout)
        self.assertIn("requested && modernDisplay && !classic && supportsSide(side)", layout)
        for engine in ("TheDisplay", "TheControlBar", "GameWindow", "TheGameLogic", "ThePlayerList"):
            self.assertNotIn(engine, layout)          # pure geometry: no engine dependency
        hud = text("Core/GameEngine/Include/Common/EnhancedHud.h")
        self.assertIn("bool requested = false;", hud)  # Original by default

    def test_choice_is_persisted_default_original_and_in_options(self):
        settings = text("Core/GameEngine/Include/Common/DisplaySettings.h")
        self.assertIn("bool enhancedHud = false;", settings)
        self.assertIn('"InterfaceStyle"', settings)
        self.assertIn('s.enhancedHud ? "Enhanced" : "Original"', settings)
        page = text("Core/GameEngine/Include/Common/OptionsPageLayout.h")
        self.assertIn('{"ModernInterfaceLabel", "ModernInterface", ControlKind::Combo}', page)
        opts = text("Core/GameEngine/Source/GameClient/GUI/ModernDisplayOptions.cpp")
        self.assertIn("EnhancedHud::setRequested(next.enhancedHud)", opts)
        self.assertIn("select(s_interface, 0);   // Original", opts)       # Defaults button = Original
        host = text("Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplayOrdinaryHost.cpp")
        self.assertIn("EnhancedHud::setRequested(settings.enhancedHud);", host)

    def test_no_win32_reference_or_zero_hour_changes_outside_shared_engine(self):
        # the prototype lives in the shared Core engine only; Generals/GeneralsMD trees are not touched
        import subprocess
        out = subprocess.run(["git", "diff", "--name-only", "HEAD"], cwd=ROOT, capture_output=True, text=True).stdout
        for line in out.split():
            self.assertFalse(line.startswith(("Generals/", "GeneralsMD/")) and "ControlBar" in line, line)

    def test_original_assets_and_commands_are_reused(self):
        s = text("Core/GameEngine/Include/Common/EnhancedHudLayout.h")
        # every window moved is an existing ControlBar.wnd window (names from the retail layout)
        for name in ("LeftHUD", "CommandWindow", "ButtonCommand01", "ButtonCommand12", "GeneralsExp", "ExpBarForeground",
                     "ButtonGeneral", "MoneyDisplay", "PowerWindow", "WinUnitSelected", "ProductionQueueWindow", "CameoWindow"):
            self.assertIn('"' + name + '"', s)
        # the special power shortcut column is the existing GenPowersShortcutBar layout, moved not rebuilt
        bar = text(BAR)
        self.assertIn("m_specialPowerShortcutParent", bar)
        cb = text("Core/GameEngine/Source/GameClient/GUI/ControlBar/ControlBar.cpp")
        self.assertIn("if( EnhancedHud::active() )\n\t\tapplyEnhancedPowerShortcutLayout();", cb)

    def test_one_shared_placement_mechanism_keeps_drawn_and_interactive_geometry_equal(self):
        # X64.36 hit-test fix: the engine and the unit tests run the same template (no separate hitboxes)
        hdr = text("Core/GameEngine/Include/Common/EnhancedHudWindows.h")
        self.assertIn("fitContainers(master, master, placed, remember);", hdr)
        self.assertIn("void applyPowerColumn(", hdr)
        bar = text(BAR)
        self.assertIn("EnhancedHudWindows::apply<GameWindow>(", bar)
        self.assertIn("EnhancedHudWindows::applyPowerColumn<GameWindow>(", bar)
        # the engine file no longer carries a private placement helper
        self.assertNotIn("void placeAbsolute(", bar)
        # the unit test drives the same header with GameWindow's position/hit-test semantics
        t = text("Tests/Google/Core/GameEngine/Common/EnhancedHudInteractiveTest.cpp")
        self.assertIn("EnhancedHudWindows::apply<Win>(", t)
        self.assertIn("pointInChild", t)
        for name in ("CenterBackground", "ButtonCommand01", "ButtonGeneral", "LeftHUD", "ButtonCommand5"):
            self.assertIn(name, t)

    def test_refinement_is_read_only_and_original_assets_are_untouched(self):
        bar = text(BAR)
        # the rank ladder only READS the authoritative rank; the XP progress still comes from the retail GeneralsExp draw
        self.assertIn("player->getRankLevel()", bar)
        self.assertIn("getRankLevelCount()", bar)
        # cash: presentation grouping only, gated on Enhanced in both titles; Original keeps the retail format call
        for rel in ("Generals/Code/GameEngine/Source/GameClient/InGameUI.cpp",
                    "GeneralsMD/Code/GameEngine/Source/GameClient/InGameUI.cpp"):
            ui = text(rel)
            self.assertIn("if( EnhancedHud::active() )", ui)
            self.assertIn("EnhancedHudLayout::groupThousands( currentMoney, grouped, 32 );", ui)
            self.assertIn('buffer.format(TheGameText->fetch( "GUI:ControlBarMoneyDisplay" ), currentMoney );', ui)
        # the retail bitmaps are only cleared inside the Enhanced apply path (the scheme re-applies them on every init)
        apply_ = bar[bar.index("void ControlBar::applyEnhancedLayout()"):bar.index("void ControlBar::restoreOriginalLayout()")]
        self.assertIn("GadgetButtonSetEnabledImage( win, nullptr );", apply_)
        self.assertIn("if( !EnhancedHud::active() || !TheWindowManager )", apply_[:200])
        sch = text(SCHEME)
        self.assertNotIn("GadgetButtonSetEnabledImage( win, nullptr )", sch)

    def test_cp10_does_not_touch_reference_guarded_sources(self):
        # the scripts/rendering and scripts/performance reference-token guards (baselined in
        # docs/modernization/REFERENCE_TOKEN_GUARD_BASELINE.md) watch these trees; the HUD work must not change them
        import subprocess
        out = subprocess.run(["git", "diff", "--name-only", "4a8641178"], cwd=ROOT, capture_output=True, text=True).stdout
        guarded = ("Core/Libraries/Source/WWVegas/WW3D2/", "Core/Libraries/Source/WWVegas/WW3D2/Backend/",
                   "Generals/Code/GameEngine/Source/GameLogic/AI/AIPathfind.cpp",
                   "GeneralsMD/Code/GameEngine/Source/GameLogic/AI/AIPathfind.cpp")
        post = [l for l in out.split() if l.startswith(guarded)]
        # X64.35 attribution/instrumentation lives before 4a8641178; nothing after it may touch the guarded trees
        self.assertEqual(post, [], post)

    def test_image_fitting_is_enhanced_only_and_hit_regions_are_untouched(self):
        bar = text(BAR)
        # every hook is gated by the Enhanced registry (empty for Original) and by EnhancedHud::active()
        impl = bar[bar.index("void EnhancedHudImage::fitImage"):bar.index("void ControlBar::applyEnhancedLayout()")]
        self.assertIn("!EnhancedHud::active()", impl)
        self.assertIn("s_aspectFit.empty()", impl)
        self.assertIn("EnhancedHudImage::gaugeScale", text("Core/GameEngineDevice/Source/W3DDevice/GameClient/GUI/GUICallbacks/W3DControlBar.cpp"))
        push = text("Core/GameEngineDevice/Source/W3DDevice/GameClient/GUI/Gadget/W3DPushButton.cpp")
        self.assertIn("EnhancedHudImage::fitImage( window, image, &start, &end );", push)
        # only the draw rectangle of the bitmap is narrowed; no window size/position is touched by the fit
        fit = bar[bar.index("void EnhancedHudImage::fitImage"):bar.index("Real EnhancedHudImage::gaugeScale")]
        self.assertNotIn("winSetSize", fit)
        self.assertNotIn("winSetPosition", fit)
        # the shared policy is the single table; the layout tables are covered by a unit test
        pol = text("Core/GameEngine/Include/Common/EnhancedHudImageFit.h")
        for cls in ("AspectFit", "NativeSize", "FrameArt", "Gauge", "Procedural", "SelfFitting"):
            self.assertIn(cls, pol)
        # the attack warning keeps its retail window and enable-toggle flashing; only its bitmaps are cleared in Enhanced
        self.assertIn('barWindow( "WinUAttack" )', bar)
        self.assertIn("!BitIsSet( win->winGetStatus(), WIN_STATUS_ENABLED )", bar)
        for forbidden in ("triggerRadarAttackGlow", "m_radarAttackGlow", "winEnable"):
            self.assertNotIn(forbidden, bar)
        # Original keeps scheme images: the scheme file is not edited for Enhanced
        self.assertNotIn("EnhancedHudImage", text(SCHEME))

    def test_global_powers_surface_is_independent_and_follows_its_own_window(self):
        bar = text(BAR)
        # the frame is the shortcut window's own draw function: it slides/hides with that window, no second visibility state
        self.assertIn("parent->winSetDrawFunc( powersPanelDraw );", bar)
        self.assertIn("s_powersParent->winSetDrawFunc( s_powersOriginalDraw );", bar)
        draw = bar[bar.index("void powersPanelDraw( GameWindow *window, WinInstanceData * )"):]
        self.assertIn("if( !EnhancedHud::active()", draw[:200])
        self.assertIn("winIsHidden()", draw)
        lay = text("Core/GameEngine/Include/Common/EnhancedHudLayout.h")
        self.assertIn("kPowerGap", lay)
        self.assertNotIn("powerBesideIsland", lay)         # no longer attached to the island
        # the placement of the five real buttons is the shared mechanism (column or compact row)
        self.assertIn("EnhancedHudWindows::applyPowerRow<GameWindow>(", bar)
        self.assertIn("EnhancedHudWindows::applyPowerColumn<GameWindow>(", bar)
        # no special power state or command is touched
        for forbidden in ("doSpecialPower", "SpecialPowerType", "getSpecialPower", "triggerSpecialPower"):
            self.assertNotIn(forbidden, bar)

    def test_console_frame_follows_the_real_control_bar_window(self):
        bar = text(BAR)
        frame = bar[bar.index("void drawEnhancedHudFrame()"):]
        self.assertIn("windowRect( s_barMaster )", frame)
        self.assertIn("EnhancedHudLayout::translated(", frame)
        # presentation only: the frame owns no flag about the bar being open/hidden
        for forbidden in ("static bool", "s_hidden", "s_visible", "m_currentControlBarStage"):
            self.assertNotIn(forbidden, frame)

    def test_command_bay_draws_all_twelve_slots_without_reordering_commands(self):
        bar = text(BAR)
        frame = bar[bar.index("void drawEnhancedHudFrame()"):]
        self.assertIn("for( int i = 0; i < 12; ++i )", frame)
        self.assertIn("occupied", frame)
        # slot windows are only drawn behind, never re-assigned or moved by index
        for forbidden in ("setControlCommand", "findCommandButton", "CommandSet", "std::swap"):
            self.assertNotIn(forbidden, bar)

    def test_theme_layer_is_data_only_and_the_renderer_reads_it(self):
        th = text("Core/GameEngine/Include/Common/EnhancedHudTheme.h")
        # pure data: no engine/gameplay includes, no callables
        for inc in ("GameClient/", "GameLogic/", "Common/Player", "Common/Money", "Common/Xfer", "GameWindow"):
            self.assertNotIn(inc, th)
        for forbidden in ("std::function", "virtual", "GameMessage", "CommandButton"):
            self.assertNotIn(forbidden, th)
        for fac in ("Usa", "China", "Gla", "Europe", "Russia"):
            self.assertIn(fac, th)
        self.assertIn("inline bool implemented(Faction f) { return f == Faction::Usa || f == Faction::China; }", th)
        bar = text(BAR)
        # all colours come from the theme: the renderer has exactly one GameMakeColor (the Rgba conversion)
        self.assertEqual(bar.count("GameMakeColor("), 1)
        self.assertIn("EnhancedHud::theme()", bar)
        hud = text("Core/GameEngine/Include/Common/EnhancedHud.h")
        self.assertIn("EnhancedHudTheme::factionOfSide(side)", hud)

    def test_global_powers_are_plates_without_an_enclosing_panel(self):
        bar = text(BAR)
        draw = bar[bar.index("void powersPanelDraw( GameWindow *window, WinInstanceData * )"):]
        draw = draw[:draw.index("}  // namespace")]
        self.assertIn("powerPlateRect(", draw)
        self.assertNotIn("drawModule(", draw)                      # no panel behind the group
        self.assertNotIn("inflate( windowRect( window )", draw)     # the group window rectangle is never filled
        # retail slot frame art is cleared for Enhanced and remembered/restored for Original
        self.assertIn("s_powerSlotArt", bar)
        self.assertIn("it->first->winSetEnabledImage( 0, it->second.enabled );", bar)

    def test_radar_black_bars_are_fixed_by_fitting_the_window_not_by_stretching(self):
        bar = text(BAR)
        frame = bar[bar.index("void drawEnhancedHudFrame()"):]
        self.assertIn("getMapExtent()", frame)
        self.assertIn("EnhancedHudImageFit::containFit(", frame)
        self.assertIn("EnhancedHudWindows::placeAbsolute( radar, fitted );", frame)
        # the retail radar draw (black fill + aspect preserving area) is not edited: Original is untouched
        w3d = text("Core/GameEngineDevice/Source/W3DDevice/Common/System/W3DRadar.cpp")
        self.assertIn("draw black border areas where we need map", w3d)
        self.assertNotIn("EnhancedHud", w3d)
        self.assertNotIn("EnhancedHud", text("Core/GameEngine/Source/Common/System/Radar.cpp"))
        radar_h = text("Core/GameEngine/Include/Common/Radar.h")
        self.assertIn("const Region3D &getMapExtent() const { return m_mapExtent; }", radar_h)

    def test_renderer_is_shared_and_reads_shape_language_from_the_theme(self):
        bar = text(BAR)
        frame = bar[bar.index("void drawEnhancedHudFrame()"):]
        for field in ("th.shellCorner", "th.fasteners", "th.seams", "th.tickMarks", "th.junctionRibs", "th.bezelRef", "th.moduleLeds"):
            self.assertIn(field, frame)
        for field in ("th.brushedChassis", "th.bevelModules", "th.vents", "th.windowCorner"):
            self.assertIn(field, bar)
        self.assertIn("c.theme->stateRails", bar)
        self.assertIn("c.theme->plateCorner", bar)
        th = text("Core/GameEngine/Include/Common/EnhancedHudTheme.h")
        for field in ("Corner shellCorner;", "bool fasteners;", "bool seams;", "bool vents;", "bool tickMarks;", "bool stateRails;",
                      "int bezelRef;", "differsBeyondColour"):
            self.assertIn(field, th)
        # only one renderer: no faction names in the renderer
        for name in ("Usa", "China", "Gla", "Europe", "Russia", "USA"):
            self.assertNotIn(name, bar.split("namespace\n{\n\ntypedef EnhancedHudLayout::Rect HRect;")[-1].replace("USA console", ""))

    def test_global_powers_plates_are_edge_mounted_with_cut_corners_on_the_battlefield_side(self):
        bar = text(BAR)
        draw = bar[bar.index("void powersPanelDraw( GameWindow *window, WinInstanceData * )"):]
        draw = draw[:draw.index("}  // namespace")]
        self.assertIn("CM_TL | CM_BL", draw)
        self.assertIn("powerPlateRect(", draw)
        lay = text("Core/GameEngine/Include/Common/EnhancedHudLayout.h")
        self.assertIn("const int cx = surfaceW - cR;", lay)
        self.assertNotIn("safeRight", lay)

    def test_global_powers_edge_dock_measures_real_windows(self):
        win = text("Core/GameEngine/Include/Common/EnhancedHudWindows.h")
        self.assertIn("int edgeRight = -1, int plateMargin = 0", win)
        self.assertIn("measureLeafRight(child, maxRight);", win)
        bar = text(BAR)
        self.assertIn("layout.powerEdgeRight, layout.powerPlateMargin", bar)
        lay = text("Core/GameEngine/Include/Common/EnhancedHudLayout.h")
        self.assertIn("r.powerEdgeRight = surfaceW;", lay)
        self.assertNotIn("const int inset = 1;", lay)


    def test_general_milestones_popup_and_overview_are_presentation_only(self):
        bar = text(BAR)
        # new code reads existing retail state and draws; it never buys a power, changes rank/xp, or writes game state
        for forbidden in ("purchaseScience", "addSkillPoints", "setSkillPoints", "setRankLevel", "grantScience",
                          "doSpecialPower", "setControlCommand", "GadgetProgressBarSetProgress", "winEnable(", "mute("):
            self.assertNotIn(forbidden, bar)
        # the four cells use the real rank table and the real xp
        self.assertIn("EnhancedHudPanels::milestoneState(", bar)
        self.assertIn("EnhancedHudPanels::milestoneProgressPermille(", bar)
        self.assertIn("info->m_skillPointsNeeded", bar)
        self.assertIn("player->getSciencePurchasePoints()", bar)
        # popup: gated hook, original draw function and background image restored for Original
        self.assertIn("gen->winSetDrawFunc( purchasePanelDraw );", bar)
        self.assertIn("s_purchaseWindow->winSetDrawFunc( s_purchaseOriginalDraw );", bar)
        self.assertIn("s_purchaseWindow->winSetEnabledImage( 0, s_purchaseOriginalImage );", bar)
        draw = bar[bar.index("void purchasePanelDraw( GameWindow *window, WinInstanceData * )"):]
        self.assertIn("if( !EnhancedHud::active()", draw[:200])
        self.assertIn("EnhancedHudPanels::powerStateDetailed(", draw)

    def test_popup_connectors_exist_only_for_real_science_prerequisites(self):
        bar = text(BAR)
        draw = bar[bar.index("void purchasePanelDraw( GameWindow *window, WinInstanceData * )"):]
        draw = draw[:draw.index("// ---- Battle Overview")]
        # a connector is produced only by chainConnectors, fed by the ScienceStore prerequisite relation (never by geometry alone)
        self.assertEqual(draw.count("chainConnectors("), 1)
        self.assertIn("TheScienceStore->getPrerequisiteSciences( plates[ b ].st, pre );", draw)
        self.assertIn("std::find( pre.begin(), pre.end(), plates[ a ].st ) != pre.end()", draw)
        # the only rail drawing sits inside the loop over the connectors that came back
        self.assertIn("for( int i = 0; i < linkCount; ++i )", draw)
        loop = draw[draw.index("for( int i = 0; i < linkCount; ++i )"):]
        loop = loop[:loop.index("// plates (with chain step numerals)")]
        self.assertIn("fillPolygon(", loop)
        self.assertNotIn("drawLine(", draw[draw.index("const int linkCount"):draw.index("// plates (with chain step numerals)")])
        panels = text("Core/GameEngine/Include/Common/EnhancedHudPanels.h")
        fn = panels[panels.index("int chainConnectors("):]
        fn = fn[:fn.index("// Depth of a power")]
        self.assertIn("!isPrerequisite(a, b)", fn)
        self.assertIn("between", fn)

    def test_science_prerequisite_getter_is_read_only_in_both_titles(self):
        for title in ("Generals", "GeneralsMD"):
            h = text(f"{title}/Code/GameEngine/Include/Common/Science.h")
            c = text(f"{title}/Code/GameEngine/Source/Common/RTS/Science.cpp")
            self.assertIn("Bool getPrerequisiteSciences(ScienceType st, ScienceVec& out) const;", h)
            body = c[c.index("Bool ScienceStore::getPrerequisiteSciences"):]
            body = body[:body.index("}", body.index("return")) + 1]
            self.assertIn("m_prereqSciences", body)
            for forbidden in ("m_prereqSciences.push_back", "m_prereqSciences.clear", "m_prereqSciences =", "new "):
                self.assertNotIn(forbidden, body)

    def test_popup_rank_tiers_follow_rank_ini_and_hidden_windows_are_restored(self):
        panels = text("Core/GameEngine/Include/Common/EnhancedHudPanels.h")
        self.assertIn("constexpr int kPopupTierRank[3] = {1, 3, 5};", panels)
        bar = text(BAR)
        self.assertIn("s_purchaseHidden.insert(", bar)
        self.assertIn("( *it )->winHide( FALSE );", bar)

    def test_blue_rims_are_state_not_decoration(self):
        bar = text(BAR)
        # the General entry button gets an outline only through the interaction model; no unconditional drawOpenRect on it
        self.assertIn("EnhancedHudPanels::generalButtonOutline( interaction )", bar)
        self.assertNotIn("if( pips > 0 )", bar)
        self.assertIn("EnhancedHudPanels::generalNotificationLamp( points )", bar)
        # the points readout has no surrounding rectangle
        draw = bar[bar.index("void purchasePanelDraw( GameWindow *window, WinInstanceData * )"):]
        self.assertNotIn("pr.w + 2 * P( 2 ) + 2", draw)
        # power plates take their outline from the semantic role, not from ad hoc colours
        self.assertEqual(draw.count("EnhancedHudPanels::powerOutline("), 3)
        # the retail blinking highlight art is replaced only while Enhanced is active; Original keeps the retail flash
        cb = text("Core/GameEngine/Source/GameClient/GUI/ControlBar/ControlBar.cpp")
        self.assertIn("if(!m_genStarFlash || EnhancedHud::active())", cb)
        self.assertIn("GadgetButtonSetEnabledImage(win, m_generalButtonHighlight);", cb)

    def test_framework_v1_shared_headers_are_frozen(self):
        # Shared Enhanced HUD Framework v1 is frozen (docs/modernization/ENHANCED_HUD_FRAMEWORK_V1.md). Changing the shared
        # geometry or placement mechanism needs a deliberate version bump, a documented reason and a new native review.
        import hashlib
        pins = {
            "Core/GameEngine/Include/Common/EnhancedHudLayout.h": "c940c83b8a003b459fc428ee9f7f6f085b6fc572082c206d14d79410b5f3ac61",
            "Core/GameEngine/Include/Common/EnhancedHudWindows.h": "0b83d551690699dd0fee50e8895e31d527ff6be267ddcf823dadec7c7d80c125",
        }
        for path, digest in pins.items():
            data = (ROOT / path).read_bytes().replace(chr(13).encode() + chr(10).encode(), chr(10).encode())
            self.assertEqual(hashlib.sha256(data).hexdigest(), digest, path + " changed: Framework v1 is frozen")
        hud = text("Core/GameEngine/Include/Common/EnhancedHud.h")
        self.assertIn("constexpr int kFrameworkVersion = 1;", hud)
        doc = text("docs/modernization/ENHANCED_HUD_FRAMEWORK_V1.md")
        self.assertIn("kFrameworkVersion == 1", doc)
        self.assertIn("FROZEN", doc)

    def test_shared_framework_headers_do_not_name_a_faction(self):
        for path in ("EnhancedHudLayout.h", "EnhancedHudWindows.h", "EnhancedHudImageFit.h"):
            src = text("Core/GameEngine/Include/Common/" + path)
            for word in ("Faction::Usa", "Faction::China", "Faction::Gla", "\"America\"", "\"China\"", "\"GLA\""):
                self.assertNotIn(word, src, path + " must not name a faction")

    def test_china_theme_is_construction_not_a_recolour_and_the_renderer_stays_faction_blind(self):
        th = text("Core/GameEngine/Include/Common/EnhancedHudTheme.h")
        china = th[th.index("inline const Theme &china()"):]
        china = china[:china.index("\n}\n")]
        usa = th[th.index("inline const Theme &usa()"):th.index("inline const Theme &china()")]
        # USA (the reference) carries none of the heavy-construction switches
        self.assertIn("/* heavyBevel, rivets, cornerCaps, moduleBrackets, girder */ false, false, false, false, false,", usa)
        self.assertIn("/* heavyBevel, rivets, cornerCaps, moduleBrackets, girder */ true, true, true, true, true,", china)
        self.assertIn("Corner::Square", china)
        # every heavy-construction branch in the renderer is a theme switch, never a faction test
        bar = text(BAR)
        for flag in ("th.girder", "th.cornerCaps", "th.moduleBrackets", "c.theme->heavyBevel", "c.theme->rivets"):
            self.assertIn(flag, bar)
        self.assertNotIn("Faction::", bar)
        self.assertNotIn("EnhancedHud::faction()", bar)

    def test_battle_overview_is_enhanced_only_and_restores_the_retail_window(self):
        bar = text(BAR)
        self.assertIn("void EnhancedHudOverview::attach( GameWindow *root )", bar)
        attach = bar[bar.index("void EnhancedHudOverview::attach( GameWindow *root )"):]
        self.assertIn("if( !EnhancedHud::active() )\n\t{\n\t\tforget( root, TRUE );", attach[:200])
        self.assertIn("root->winSetDrawFunc( s_overviewDraw );", attach)
        self.assertIn("it->first->winSetEnabledImage( 0, it->second.enabled );", attach)
        dip = text("Core/GameEngine/Source/GameClient/GUI/GUICallbacks/Diplomacy.cpp")
        self.assertIn("EnhancedHudOverview::attach( theWindow );", dip)
        # the retail branch is intact; Enhanced only adds a second way into the player table
        self.assertIn("if (TheRecorder->isMultiplayer() || EnhancedHudOverview::showPlayerTable())", dip)
        self.assertIn("EnhancedHudOverview::forget(theWindow, FALSE);", dip)
        # no hidden information: the overview code never touches money, units, score keeping or other players' objects
        ov = bar[bar.index("void overviewDraw( GameWindow *window, WinInstanceData * )"):]
        ov = ov[:ov.index("}  // namespace")]
        for forbidden in ("getMoney", "ScoreKeeper", "getUnitCount", "countMoney", "ThePlayerList", "getObject", "KindOf", "isMuted", "mute("):
            self.assertNotIn(forbidden, ov)
        # shared data only: elapsed logic time, map name
        self.assertIn("TheGameLogic->getFrame()", ov)
        panels = text("Core/GameEngine/Include/Common/EnhancedHudPanels.h")
        self.assertIn('{OverviewTab::Army, "ARMY", false,', panels)
        self.assertIn('{OverviewTab::Economy, "ECONOMY", false,', panels)
        self.assertIn('{OverviewTab::Combat, "COMBAT", false,', panels)


    def test_popup_relayout_moves_the_retail_windows_and_restores_them(self):
        bar = text(BAR)
        self.assertIn("EnhancedHudWindows::applyPopupLayout<GameWindow>( gen, place.rect, place.scale,", bar)
        # geometry is recorded before it changes and put back for Original (positions and sizes only; hidden state is the game's)
        self.assertIn("it->first->winSetPosition( it->second.pos.x, it->second.pos.y );", bar)
        self.assertIn("it->first->winSetSize( it->second.size.x, it->second.size.y );", bar)
        win = text("Core/GameEngine/Include/Common/EnhancedHudWindows.h")
        body = win[win.index("void applyPopupLayout("):]
        body = body[:body.index("// Places every window of the layout tables")]
        # the layout only places windows: no creation, destruction, command or state change
        for forbidden in ("winCreate", "winDestroy", "winEnable", "setControlCommand", "GadgetButton", "winSetUserData"):
            self.assertNotIn(forbidden, body)
        # the only visibility change: the retail tier header texts flagged by the layout table, restored for Original
        self.assertEqual(body.count("winHide"), 1)
        self.assertIn("if (t.hide)", body)
        self.assertIn("child->winHide(true);", body)
        self.assertIn("s_purchaseHidden", text(BAR))
        self.assertIn("( *it )->winHide( FALSE );", text(BAR))
        self.assertIn("winSetPosition", body)
        self.assertIn("winSetSize", body)

    def test_power_meter_is_permanent_and_reads_only_existing_state(self):
        bar = text(BAR)
        frame = bar[bar.index("void drawEnhancedHudFrame()"):]
        meter = frame[frame.index("POWER METER"):]
        meter = meter[:meter.index("HRect cells[ 12 ];")]
        # the track is drawn from the window rectangle alone: it does not depend on any power value being non zero
        self.assertIn("kPowerSegments", meter)
        self.assertIn("drawRecess( m, c, P( 1.2 ) );", meter)
        self.assertIn("energy->hasSufficientPower()", meter)
        for forbidden in ("getProduction", "getConsumption", "addProduction", "addConsumption", "setEnergy", "adjust"):
            self.assertNotIn(forbidden, meter)
        # the separators are drawn by the retail gauge function through a gated hook; the tooltip window is the same window
        w3d = text("Core/GameEngineDevice/Source/W3DDevice/GameClient/GUI/GUICallbacks/W3DControlBar.cpp")
        self.assertIn("EnhancedHudImage::drawGaugeSegments( window, pos.x, pos.y, size.x, size.y );", w3d)
        self.assertIn("if( !window || !EnhancedHud::active() || window != s_powerMeter", bar)
        self.assertIn('{"PowerWindow",', text("Core/GameEngine/Include/Common/EnhancedHudLayout.h"))


if __name__ == "__main__":
    unittest.main()
