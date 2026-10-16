/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2026 TheSuperHackers
**
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

// X64.37: display transaction (Apply / Keep / Revert / timeout), monitor identity, command-line precedence, the persistence plan and
// the resolution list. Everything here is pure: no window, no monitor and no engine singleton is needed.

#include <gtest/gtest.h>

#include "Common/DisplayRuntime.h"
#include "Common/DisplaySettings.h"
#include "Common/DisplayTransaction.h"
#include "Common/EnhancedHud.h"

#include <algorithm>
#include <functional>
#include <set>
#include <vector>

using namespace DisplayConfig;
using DisplayGeometry::Size;
using DisplayModes::Monitor;
using DisplayTransaction::Config;
using DisplayTransaction::Outcome;
using DisplayTransaction::Phase;
using DisplayTransaction::Transaction;

namespace
{
Monitor monitor(const wchar_t *name, int x, int w, int h, bool primary, int hz = 60)
{
	Monitor m;
	m.deviceName = name;
	m.bounds = {x, 0, w, h};
	m.work = {x, 0, w, h - 40};
	m.primary = primary;
	m.desktop.width = w; m.desktop.height = h; m.desktop.refreshHz = hz;
	return m;
}

Config windowed(int w, int h, int scale = 0)
{
	Config c;
	c.mode = WindowMode::Windowed;
	c.client = {w, h};
	c.uiScalePercent = scale;
	return c;
}

// A scripted display: remembers what was applied and which configurations it refuses.
struct FakeDisplay
{
	std::vector<Config> applied;
	std::vector<Config> refuse;
	bool refuseEverything = false;
	bool operator()(const Config &c)
	{
		applied.push_back(c);
		if (refuseEverything) return false;
		for (const Config &r : refuse)
			if (r == c) return false;
		return true;
	}
};
}  // namespace

// ---- the transaction -----------------------------------------------------------------------------------------------------------

TEST(DisplayTransaction, ApplyThenKeepAdoptsTheNewConfigurationAsLastKnownGood)
{
	FakeDisplay display;
	Transaction tx;
	const Config good = windowed(1920, 1080), wanted = windowed(2560, 1440);
	EXPECT_EQ(tx.begin(good, wanted, std::ref(display), 1000), Outcome::Pending);
	EXPECT_EQ(tx.phase(), Phase::AwaitingConfirmation);
	EXPECT_EQ(tx.current(), wanted);                 // CURRENT = PENDING while a confirmation is open
	EXPECT_EQ(tx.lastKnownGood(), good);
	Config accepted;
	EXPECT_TRUE(tx.keep(&accepted));
	EXPECT_EQ(accepted, wanted);
	EXPECT_EQ(tx.lastKnownGood(), wanted);
	EXPECT_EQ(tx.phase(), Phase::Idle);
	EXPECT_FALSE(tx.keep());                         // a second Keep is a no-op
	EXPECT_EQ(display.applied.size(), 1u);           // keeping applies nothing again
}

TEST(DisplayTransaction, RevertRestoresTheLastKnownGood)
{
	FakeDisplay display;
	Transaction tx;
	const Config good = windowed(1920, 1080), wanted = windowed(3440, 1440);
	ASSERT_EQ(tx.begin(good, wanted, std::ref(display), 0), Outcome::Pending);
	EXPECT_EQ(tx.revert(std::ref(display)), Outcome::RolledBack);
	ASSERT_EQ(display.applied.size(), 2u);
	EXPECT_EQ(display.applied.back(), good);
	EXPECT_EQ(tx.phase(), Phase::Idle);
	EXPECT_EQ(tx.current(), good);
	EXPECT_EQ(tx.revert(std::ref(display)), Outcome::Unchanged);   // nothing pending any more
}

TEST(DisplayTransaction, TimeoutRevertsByItself)
{
	FakeDisplay display;
	Transaction tx(15000);
	const Config good = windowed(1920, 1080), wanted = windowed(5120, 1440);
	ASSERT_EQ(tx.begin(good, wanted, std::ref(display), 100000), Outcome::Pending);
	EXPECT_EQ(tx.remainingMs(100000), 15000u);
	EXPECT_EQ(tx.remainingMs(107000), 8000u);
	Outcome outcome = Outcome::Unchanged;
	EXPECT_FALSE(tx.tick(110000, std::ref(display), &outcome));      // still waiting
	EXPECT_EQ(tx.phase(), Phase::AwaitingConfirmation);
	EXPECT_TRUE(tx.tick(115000, std::ref(display), &outcome));       // deadline reached
	EXPECT_EQ(outcome, Outcome::RolledBack);
	EXPECT_EQ(tx.phase(), Phase::Idle);
	EXPECT_EQ(display.applied.back(), good);
	EXPECT_EQ(tx.remainingMs(120000), 0u);
}

TEST(DisplayTransaction, TheClockMayWrapWithoutBreakingTheDeadline)
{
	FakeDisplay display;
	Transaction tx(15000);
	const unsigned start = 0xFFFFF000u;                              // 4 s before the 32 bit tick counter wraps
	ASSERT_EQ(tx.begin(windowed(1920, 1080), windowed(2560, 1440), std::ref(display), start), Outcome::Pending);
	EXPECT_FALSE(tx.tick(start + 10000u, std::ref(display)));
	EXPECT_TRUE(tx.tick(start + 15000u, std::ref(display)));
}

TEST(DisplayTransaction, AFailedApplyRollsBackAndNothingIsPending)
{
	FakeDisplay display;
	const Config good = windowed(1920, 1080), bad = windowed(7680, 4320);
	display.refuse.push_back(bad);
	Transaction tx;
	EXPECT_EQ(tx.begin(good, bad, std::ref(display), 0), Outcome::RolledBack);
	EXPECT_EQ(tx.phase(), Phase::Idle);
	ASSERT_EQ(display.applied.size(), 2u);                           // the refused request, then the restore
	EXPECT_EQ(display.applied.back(), good);
	EXPECT_EQ(tx.lastKnownGood(), good);
}

TEST(DisplayTransaction, IfEvenTheLastKnownGoodCannotBeRestoredTheSafeFallbackRuns)
{
	FakeDisplay display;
	const Config good = windowed(1920, 1080), bad = windowed(2560, 1440);
	display.refuse.push_back(bad);
	display.refuse.push_back(good);                                  // e.g. its monitor vanished
	Transaction tx;
	EXPECT_EQ(tx.begin(good, bad, std::ref(display), 0), Outcome::SafeFallback);
	EXPECT_EQ(display.applied.back(), DisplayTransaction::safeFallback());
	EXPECT_EQ(tx.lastKnownGood(), DisplayTransaction::safeFallback());
	EXPECT_EQ(tx.phase(), Phase::Idle);
	// the fallback is always a small window on the primary monitor
	const Config safe = DisplayTransaction::safeFallback();
	EXPECT_EQ(safe.mode, WindowMode::Windowed);
	EXPECT_EQ(safe.client, DisplayGeometry::kReferenceSize);
	EXPECT_TRUE(safe.monitorName.empty());
}

TEST(DisplayTransaction, ARevertThatFailsFallsBackToTheSafeConfiguration)
{
	FakeDisplay display;
	const Config good = windowed(1920, 1080), wanted = windowed(2560, 1440);
	Transaction tx;
	ASSERT_EQ(tx.begin(good, wanted, std::ref(display), 0), Outcome::Pending);
	display.refuse.push_back(good);                                  // the old mode is gone by the time the player declines
	EXPECT_EQ(tx.revert(std::ref(display)), Outcome::SafeFallback);
	EXPECT_EQ(display.applied.back(), DisplayTransaction::safeFallback());
}

TEST(DisplayTransaction, AnUnchangedRequestAppliesNothing)
{
	FakeDisplay display;
	Transaction tx;
	const Config good = windowed(1920, 1080);
	EXPECT_EQ(tx.begin(good, good, std::ref(display), 0), Outcome::Unchanged);
	EXPECT_TRUE(display.applied.empty());
	EXPECT_EQ(tx.phase(), Phase::Idle);
}

TEST(DisplayTransaction, ModeMonitorAndUiScaleAreOneTransaction)
{
	FakeDisplay display;
	Transaction tx;
	Config good = windowed(1920, 1080, 0);
	Config wanted = good;
	wanted.mode = WindowMode::Borderless;
	wanted.monitorIndex = 1;
	wanted.monitorName = "\\\\.\\DISPLAY2";
	wanted.uiScalePercent = 150;
	ASSERT_EQ(tx.begin(good, wanted, std::ref(display), 0), Outcome::Pending);
	EXPECT_EQ(display.applied.size(), 1u);                           // one apply for all three changes
	EXPECT_EQ(tx.revert(std::ref(display)), Outcome::RolledBack);    // and one revert restores all three
	EXPECT_EQ(display.applied.back(), good);
}

// ---- monitor identity ----------------------------------------------------------------------------------------------------------

TEST(DisplayMonitors, ChosenByStableNameBeforeIndex)
{
	const std::vector<Monitor> two = {monitor(L"\\\\.\\DISPLAY1", 0, 2560, 1440, true), monitor(L"\\\\.\\DISPLAY2", 2560, 3440, 1440, false)};
	std::vector<std::string> notes;
	EXPECT_EQ(chooseMonitor(two, "\\\\.\\DISPLAY2", 0, &notes), 1);   // the name wins over a stale index
	EXPECT_TRUE(notes.empty());
	// the monitors were enumerated in a different order next launch: the name still finds the same output
	const std::vector<Monitor> reordered = {monitor(L"\\\\.\\DISPLAY2", 0, 3440, 1440, true), monitor(L"\\\\.\\DISPLAY1", 3440, 2560, 1440, false)};
	EXPECT_EQ(chooseMonitor(reordered, "\\\\.\\DISPLAY1", 0), 1);
	// no stored name (older files): the index is honoured when valid
	EXPECT_EQ(chooseMonitor(two, "", 1), 1);
}

TEST(DisplayMonitors, ADisconnectedMonitorFallsBackToThePrimaryAndSaysSo)
{
	const std::vector<Monitor> one = {monitor(L"\\\\.\\DISPLAY1", 0, 2560, 1440, true)};
	std::vector<std::string> notes;
	EXPECT_EQ(chooseMonitor(one, "\\\\.\\DISPLAY3", 2, &notes), 0);
	ASSERT_EQ(notes.size(), 1u);
	EXPECT_NE(notes[0].find("not attached"), std::string::npos);
	notes.clear();
	EXPECT_EQ(chooseMonitor(one, "", 5, &notes), 0);
	EXPECT_EQ(notes.size(), 1u);
	// the primary is not always first in a list that was handed in unsorted
	const std::vector<Monitor> unsorted = {monitor(L"\\\\.\\DISPLAY2", 2560, 1920, 1080, false), monitor(L"\\\\.\\DISPLAY1", 0, 2560, 1440, true)};
	EXPECT_EQ(chooseMonitor(unsorted, "\\\\.\\DISPLAY9", 0), 1);
	EXPECT_EQ(chooseMonitor({}, "x", 3), 0);
}

TEST(DisplayMonitors, ResolveReportsTheChosenMonitorsIdentityAndRoundTrips)
{
	Environment env;
	env.monitors = {monitor(L"\\\\.\\DISPLAY1", 0, 2560, 1440, true), monitor(L"\\\\.\\DISPLAY2", 2560, 3440, 1440, false)};
	Settings s;
	s.monitor = 0;                                    // stale index
	s.monitorName = "\\\\.\\DISPLAY2";
	const Resolved r = resolve(s, env);
	EXPECT_EQ(r.monitorIndex, 1);
	EXPECT_EQ(r.monitorName, "\\\\.\\DISPLAY2");
	KeyValues kv;
	store(s, kv);
	EXPECT_EQ(kv["DisplayMonitorName"], "\\\\.\\DISPLAY2");
	KeyValues withVersion = kv;
	EXPECT_EQ(load(withVersion, false).monitorName, "\\\\.\\DISPLAY2");
	// the monitor is gone: primary, and the stored name is not rewritten by resolving
	env.monitors.pop_back();
	const Resolved fallback = resolve(s, env);
	EXPECT_EQ(fallback.monitorIndex, 0);
	EXPECT_EQ(fallback.monitorName, "\\\\.\\DISPLAY1");
	EXPECT_EQ(s.monitorName, "\\\\.\\DISPLAY2");
}

// ---- command-line precedence ---------------------------------------------------------------------------------------------------

TEST(DisplayPrecedence, CommandLineBeatsStoredWhichBeatsDefaults)
{
	Settings stored;
	stored.explicitSettings = true;
	stored.mode = WindowMode::Windowed;
	stored.resolution = {2560, 1440};
	SessionOverrides o;
	EXPECT_FALSE(o.any());
	EXPECT_EQ(applyOverrides(stored, o).resolution, stored.resolution);        // no flags: stored values stand
	o.resolution = {1920, 1080};
	o.fullscreen = true;
	const Settings effective = applyOverrides(stored, o);
	EXPECT_EQ(effective.resolution, (Size{1920, 1080}));
	EXPECT_EQ(effective.mode, WindowMode::Borderless);                          // -fullscreen is borderless here (no exclusive mode)
	SessionOverrides win;
	win.windowed = true;
	EXPECT_EQ(applyOverrides(effective, win).mode, WindowMode::Windowed);
	// invalid sizes are ignored, not applied
	SessionOverrides junk;
	junk.resolution = {10, 10};
	EXPECT_EQ(applyOverrides(stored, junk).resolution, stored.resolution);
	junk.resolution = {99999, 600};
	EXPECT_EQ(applyOverrides(stored, junk).resolution, stored.resolution);
}

TEST(DisplayPrecedence, AnOverrideIsNeverWrittenBackUnlessThePlayerChangesThatField)
{
	Settings stored;
	stored.explicitSettings = true;
	stored.mode = WindowMode::Windowed;
	stored.resolution = {2560, 1440};
	stored.vsync = false;
	SessionOverrides o;
	o.resolution = {1280, 720};
	o.fullscreen = true;
	const Settings effective = applyOverrides(stored, o);
	// Options > Accept with only VSync changed: size and mode keep their STORED values
	Settings requested = effective;
	requested.vsync = true;
	PersistPlan plan = planPersistence(stored, effective, requested);
	EXPECT_FALSE(plan.riskyChanged);
	EXPECT_EQ(plan.afterKeep.resolution, (Size{2560, 1440}));
	EXPECT_EQ(plan.afterKeep.mode, WindowMode::Windowed);
	EXPECT_TRUE(plan.afterKeep.vsync);
	// the player picks a new size: that field (and only that) is persisted after Keep
	requested = effective;
	requested.resolution = {1920, 1080};
	plan = planPersistence(stored, effective, requested);
	EXPECT_TRUE(plan.riskyChanged);
	EXPECT_EQ(plan.immediate.resolution, (Size{2560, 1440}));                   // not yet
	EXPECT_EQ(plan.afterKeep.resolution, (Size{1920, 1080}));
	EXPECT_EQ(plan.afterKeep.mode, WindowMode::Windowed);                       // the override mode still did not leak
}

TEST(DisplayPrecedence, RiskyFieldsWaitForKeepAndSafeFieldsDoNot)
{
	Settings stored;
	stored.explicitSettings = true;
	stored.monitor = 0;
	stored.monitorName = "\\\\.\\DISPLAY1";
	stored.uiScalePercent = DisplayGeometry::kAutoUiScale;
	Settings requested = stored;
	requested.vsync = true;
	requested.smoothMotion = true;
	requested.enhancedHud = true;
	PersistPlan plan = planPersistence(stored, stored, requested);
	EXPECT_FALSE(plan.riskyChanged);
	EXPECT_TRUE(plan.immediate.vsync && plan.immediate.smoothMotion && plan.immediate.enhancedHud);
	// mode, monitor and UI scale are risky
	requested = stored;
	requested.mode = WindowMode::Borderless;
	EXPECT_TRUE(planPersistence(stored, stored, requested).riskyChanged);
	requested = stored;
	requested.monitor = 1;
	requested.monitorName = "\\\\.\\DISPLAY2";
	plan = planPersistence(stored, stored, requested);
	EXPECT_TRUE(plan.riskyChanged);
	EXPECT_EQ(plan.immediate.monitorName, "\\\\.\\DISPLAY1");
	EXPECT_EQ(plan.afterKeep.monitorName, "\\\\.\\DISPLAY2");
	requested = stored;
	requested.uiScalePercent = 150;
	plan = planPersistence(stored, stored, requested);
	EXPECT_TRUE(plan.riskyChanged);
	EXPECT_EQ(plan.immediate.uiScalePercent, DisplayGeometry::kAutoUiScale);
	EXPECT_EQ(plan.afterKeep.uiScalePercent, 150);
	// nothing changed: nothing risky, nothing different
	plan = planPersistence(stored, stored, stored);
	EXPECT_FALSE(plan.riskyChanged);
}

// ---- the resolution list -------------------------------------------------------------------------------------------------------

TEST(DisplayResolutionList, CoversEveryModernAspectFamilyDeduplicatedAndSorted)
{
	Monitor huge = monitor(L"\\\\.\\DISPLAY1", 0, 7680, 4320, true);
	const std::vector<Size> list = sizeChoices(huge, WindowMode::Windowed, 16, 39);
	auto has = [&](Size s) { return std::find(list.begin(), list.end(), s) != list.end(); };
	for (Size s : {Size{800, 600}, Size{1280, 1024}, Size{1920, 1200}, Size{1920, 1080}, Size{2560, 1440}, Size{3440, 1440},
	               Size{5120, 1440}, Size{2560, 1080}, Size{3840, 1080}})
		EXPECT_TRUE(has(s)) << s.width << "x" << s.height;
	std::set<std::string> families;
	for (const Size &s : list) families.insert(DisplayModes::aspectFamily(s));
	for (const char *f : {"4:3", "5:4", "16:10", "16:9", "21:9", "32:9"})
		EXPECT_TRUE(families.count(f)) << f;
	for (size_t i = 1; i < list.size(); ++i)                          // sorted and de-duplicated
		EXPECT_TRUE(list[i - 1].width < list[i].width || (list[i - 1].width == list[i].width && list[i - 1].height < list[i].height));
	// the largest is capped by the work area minus the frame
	for (const Size &s : list) { EXPECT_LE(s.width, 7680 - 16); EXPECT_LE(s.height, 4320 - 40 - 39); }
}

TEST(DisplayResolutionList, WindowedFiltersByTheWorkAreaAndBorderlessOffersOnlyTheDesktop)
{
	const Monitor small = monitor(L"\\\\.\\DISPLAY1", 0, 1366, 768, true);
	const std::vector<Size> windowedList = sizeChoices(small, WindowMode::Windowed, 16, 39, Size{1280, 720});
	for (const Size &s : windowedList)
		if (s != Size{1280, 720})                                   // the current size is always listed, even when it does not fit
		{ EXPECT_LE(s.width, 1366 - 16); EXPECT_LE(s.height, 768 - 40 - 39); }
	EXPECT_TRUE(std::find(windowedList.begin(), windowedList.end(), Size{1280, 720}) != windowedList.end());   // current is always listed
	EXPECT_TRUE(std::find(windowedList.begin(), windowedList.end(), Size{1920, 1080}) == windowedList.end());
	const std::vector<Size> borderless = sizeChoices(small, WindowMode::Borderless, 16, 39);
	ASSERT_EQ(borderless.size(), 1u);
	EXPECT_EQ(borderless[0], (Size{1366, 768}));                      // no fake modes in borderless
}

TEST(DisplayResolutionList, DriverModesAreIncludedWithoutDuplicates)
{
	Monitor m = monitor(L"\\\\.\\DISPLAY1", 0, 3840, 2160, true);
	DisplayModes::Mode a; a.width = 3000; a.height = 1800; a.refreshHz = 60;
	DisplayModes::Mode b = a; b.refreshHz = 144;                      // same size, another refresh: one entry
	DisplayModes::Mode c; c.width = 1920; c.height = 1080; c.refreshHz = 60;   // already a standard size
	m.modes = DisplayModes::filterModes({a, b, c});
	const std::vector<Size> list = sizeChoices(m, WindowMode::Windowed, 16, 39);
	EXPECT_EQ(std::count(list.begin(), list.end(), Size{3000, 1800}), 1);
	EXPECT_EQ(std::count(list.begin(), list.end(), Size{1920, 1080}), 1);
}

TEST(DisplayResolutionList, LabelsCarryTheAspectFamily)
{
	EXPECT_EQ(resolutionChoiceLabel({1920, 1080}), "1920 x 1080  (16:9)");
	EXPECT_EQ(resolutionChoiceLabel({3440, 1440}), "3440 x 1440  (21:9)");
	EXPECT_EQ(resolutionChoiceLabel({5120, 1440}), "5120 x 1440  (32:9)");
	EXPECT_EQ(resolutionChoiceLabel({1280, 1024}), "1280 x 1024  (5:4)");
	EXPECT_EQ(resolutionChoiceLabel({1920, 1200}), "1920 x 1200  (16:10)");
	EXPECT_EQ(resolutionChoiceLabel({800, 600}), "800 x 600  (4:3)");
}

TEST(DisplayResolutionList, RefreshRatesAreOnlyMeaningfulForRealDriverModes)
{
	// Windowed and borderless present at the desktop rate (resolve reports it); a refresh selector would control nothing.
	Environment env;
	env.monitors = {monitor(L"\\\\.\\DISPLAY1", 0, 2560, 1440, true, 144)};
	Settings s;
	s.refreshHz = 60;                                              // a stored refresh cannot change the presentation rate
	s.mode = WindowMode::Borderless;
	EXPECT_EQ(resolve(s, env).refreshHz, 144);
	s.mode = WindowMode::Windowed;
	EXPECT_EQ(resolve(s, env).refreshHz, 144);
	s.mode = WindowMode::Exclusive;                                // exclusive fullscreen is not offered: reported as borderless
	const Resolved r = resolve(s, env);
	EXPECT_EQ(r.mode, WindowMode::Borderless);
	EXPECT_FALSE(r.notes.empty());
}

// ---- geometry changes: HUD layouts after a display change ---------------------------------------------------------------------

TEST(DisplayGeometryChanges, TheEnhancedHudFollowsEveryResolutionChangeAndBackToOriginal)
{
	EnhancedHud::setRequested(true);
	const Size surfaces[] = {{1920, 1080}, {800, 600}, {5120, 1440}, {3440, 1440}, {1280, 1024}, {2560, 1440}, {1920, 1200}};
	int previousWidth = 0;
	for (const Size &s : surfaces)
	{
		const DisplayGeometry::Layout layout = DisplayGeometry::computeLayout(s, DisplayGeometry::kAutoUiScale);
		ASSERT_TRUE(EnhancedHud::update(true, false, "America", s.width, s.height, layout.scale())) << s.width << "x" << s.height;
		const EnhancedHudLayout::Result &l = EnhancedHud::layout();
		EXPECT_TRUE(l.valid);
		EXPECT_EQ(l.island.bottom(), s.height);                       // bottom-flush at every size
		EXPECT_GE(l.island.x, 0);
		EXPECT_LE(l.island.right(), s.width);
		EXPECT_LE(std::abs(l.island.x - (s.width - l.island.right())), 1);   // centred
		previousWidth = l.island.w;
	}
	EXPECT_GT(previousWidth, 0);
	// back to Original after display changes: Enhanced off leaves no layout behind
	EnhancedHud::setRequested(false);
	EXPECT_FALSE(EnhancedHud::update(true, false, "America", 1920, 1080, 1.5));
	EXPECT_FALSE(EnhancedHud::active());
	EXPECT_FALSE(EnhancedHud::layout().valid);
	// and Enhanced again after another change
	EnhancedHud::setRequested(true);
	EXPECT_TRUE(EnhancedHud::update(true, false, "GLA", 2560, 1440, 1.0));
	EnhancedHud::setRequested(false);
	EnhancedHud::deactivate();
}

TEST(DisplayGeometryChanges, UiScaleCombinesWithTheNewSurfaceWithoutLeavingTheCanvas)
{
	for (const Size &s : {Size{800, 600}, Size{1920, 1080}, Size{5120, 1440}, Size{1111, 777}})
		for (int percent : {DisplayGeometry::kAutoUiScale, 75, 100, 150, 200, 300})
		{
			const DisplayGeometry::Layout l = DisplayGeometry::computeLayout(s, percent);
			EXPECT_LE(l.canvas.right(), s.width) << s.width << "x" << s.height << " " << percent;
			EXPECT_LE(l.canvas.bottom(), s.height) << s.width << "x" << s.height << " " << percent;
			EXPECT_GE(l.canvas.x, 0);
		}
}
