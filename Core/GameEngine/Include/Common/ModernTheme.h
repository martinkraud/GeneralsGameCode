// SPDX-License-Identifier: GPL-3.0-or-later
// Modern Experience design system (UI.2, revised by the Modern Frontend milestone): one Generals visual language, controlled title variants (pure data, engine-free).
//
// Everything a screen needs to LOOK like the Modern Generals frontend comes from here as tokens; screens never carry colours, font names, sizes or shape constants of their own.
//   * Typography is expressed as ROLES. Audit result (docs/modernization/MODERN_UI_3_FRONTEND.md): the retail "Generals" font family is not installed anywhere (no TTF in the game or in Windows), so the retail
//     menus were always drawn with the GDI substitute. The Modern roles therefore name real, legal runtime fonts as an ordered candidate list; the binding layer picks the first one that is really installed
//     (GDI substitution is detected, never silently accepted) and logs the choice.
//   * Colours are a palette of semantic slots; a title variant (Generals, Zero Hour) replaces the accent only.
//   * Shape: chamfered (cut-corner) panels and bars, hairlines, accent ticks - structural, industrial, not rounded.
#pragma once

#include <cstdint>
#include <type_traits>

namespace ModernTheme
{
struct Color
{
	std::uint8_t r = 0, g = 0, b = 0, a = 255;
	std::uint32_t argb() const { return (std::uint32_t(a) << 24) | (std::uint32_t(r) << 16) | (std::uint32_t(g) << 8) | std::uint32_t(b); }
	Color withAlpha(std::uint8_t alpha) const { Color c = *this; c.a = alpha; return c; }
};
constexpr Color rgba(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255) { return Color{ r, g, b, a }; }

enum class Title : std::uint8_t { Generals = 0, ZeroHour = 1, Count };

// ---- typography ----------------------------------------------------------------------------------------------------------------------------------------------------------------
enum class TypeRole : std::uint8_t
{
	Title,            // screen title
	MajorHeading,     // a major block heading (page title inside a screen)
	SectionHeading,   // a group heading inside a screen
	MenuPrimary,      // navigation action (main menu, category rail)
	MenuSecondary,    // lesser action (Back, Exit, footer actions)
	Body,             // descriptive text
	Label,            // setting label
	Value,            // setting value (what a control shows)
	Helper,           // helper / tooltip / footer text
	Status,           // warning / status line
	Count
};

constexpr int kFontCandidates = 3;

struct TypeStyle
{
	const char *families[kFontCandidates];   // ordered candidates, the first installed one is used (nullptr ends the list)
	int referenceSize;                       // point size at UI scale 100 %
	bool bold;
	bool upper;                              // set in capitals (the labels keep their GameText case otherwise)
};

inline const TypeStyle &type(TypeRole role)
{
	// Bahnschrift: the DIN-style industrial sans bundled with Windows 10 / 11 (technical, signage / vehicle-plate feel, fits the field-hardware direction); Segoe UI and Arial are the installed fallbacks.
	static const TypeStyle table[static_cast<int>(TypeRole::Count)] = {
		{ { "Bahnschrift", "Segoe UI", "Arial" }, 26, true,  true  },   // Title
		{ { "Bahnschrift", "Segoe UI", "Arial" }, 19, true,  true  },   // MajorHeading
		{ { "Bahnschrift", "Segoe UI", "Arial" }, 13, true,  true  },   // SectionHeading
		{ { "Bahnschrift", "Segoe UI", "Arial" }, 18, true,  true  },   // MenuPrimary
		{ { "Bahnschrift", "Segoe UI", "Arial" }, 14, true,  true  },   // MenuSecondary
		{ { "Bahnschrift", "Segoe UI", "Arial" }, 12, false, false },   // Body
		{ { "Bahnschrift", "Segoe UI", "Arial" }, 12, false, false },   // Label
		{ { "Bahnschrift", "Segoe UI", "Arial" }, 12, true,  false },   // Value
		{ { "Bahnschrift", "Segoe UI", "Arial" }, 10, false, false },   // Helper
		{ { "Bahnschrift", "Segoe UI", "Arial" }, 11, true,  true  },   // Status
	};
	return table[static_cast<int>(role)];
}

// ---- palette -------------------------------------------------------------------------------------------------------------------------------------------------------------------
struct Palette
{
	// surfaces
	Color scrim;           // the dark band behind the navigation: the shell map stays readable
	Color panel;           // a content panel
	Color panelRaised;     // a sub-panel / selected row base
	Color field;           // the inside of an input (combo value, list, entry)
	// lines
	Color hairline;        // quiet separators
	Color edge;            // panel / field outline
	Color accent;          // title variant accent (ticks, rules, selection, primary action)
	Color accentSoft;      // accent at low alpha: hover / selection wash
	// text
	Color text;            // primary text
	Color textSecondary;   // labels
	Color textMuted;       // helper text
	Color textDisabled;
	Color textOnAccent;    // text on a filled accent surface
	// semantic
	Color danger;          // exit / destructive
	Color warning;
	Color ok;
	Color focus;           // focus bracket / ring
};

struct Variant
{
	const char *name;
	Palette palette;
	const char *logoImage;     // mapped image name
	int logoWidth, logoHeight; // source size, for the aspect ratio
};

inline const Variant &variant(Title title)
{
	// Shared base: steel / charcoal field-hardware surfaces, warm-white text. Only the accent differs between the titles (Generals sand-gold, Zero Hour signal orange).
	static const Variant table[static_cast<int>(Title::Count)] = {
		{ "Generals",
		  { rgba(6, 10, 16, 176), rgba(12, 18, 27, 214), rgba(24, 33, 46, 224), rgba(5, 8, 13, 232),
		    rgba(72, 86, 104, 255), rgba(116, 132, 152, 255), rgba(214, 178, 74, 255), rgba(214, 178, 74, 46),
		    rgba(236, 240, 244, 255), rgba(178, 190, 204, 255), rgba(128, 142, 158, 255), rgba(88, 98, 112, 255), rgba(14, 16, 18, 255),
		    rgba(214, 84, 70, 255), rgba(236, 170, 60, 255), rgba(120, 200, 110, 255), rgba(238, 206, 112, 255) },
		  "GeneralsLogo", 370, 120 },
		{ "ZeroHour",
		  { rgba(6, 10, 16, 176), rgba(12, 18, 27, 214), rgba(24, 33, 46, 224), rgba(5, 8, 13, 232),
		    rgba(72, 86, 104, 255), rgba(116, 132, 152, 255), rgba(232, 118, 40, 255), rgba(232, 118, 40, 46),
		    rgba(236, 240, 244, 255), rgba(178, 190, 204, 255), rgba(128, 142, 158, 255), rgba(88, 98, 112, 255), rgba(18, 12, 8, 255),
		    rgba(214, 84, 70, 255), rgba(236, 170, 60, 255), rgba(120, 200, 110, 255), rgba(255, 168, 84, 255) },
		  "GeneralsLogo", 370, 120 },
	};
	return table[static_cast<int>(title)];
}

// ---- metrics (reference units; the layout multiplies by the UI scale) ------------------------------------------------------------------------------------------------------------
struct Metrics
{
	// main menu
	int panelPreferredWidth = 380;
	int panelMinWidth = 300;
	int panelMaxWidth = 440;
	int panelPadding = 22;
	int itemHeight = 46;        // a navigation item
	int itemGap = 4;
	int headingHeight = 28;
	int ruleThickness = 2;
	int footnoteHeight = 16;
	int edgeMargin = 28;        // minimum distance of the band from the physical screen edge
	int bracketLength = 14;     // corner bracket / focus bracket arm
	int panelMaxHeightPercent = 70;   // the navigation band never takes more than this share of the surface height (the UI scale is reduced to fit)
	// shape
	int chamfer = 12;           // cut-corner size of panels and bars
	int chamferSmall = 6;       // cut-corner size of buttons / fields
	int accentTick = 4;         // width of a selection / accent tick
	// settings screens
	int railWidth = 220;        // category rail
	int rowHeight = 36;         // a setting row
	int controlHeight = 30;     // a control inside a row
	int controlMinWidth = 190;
	int controlMaxWidth = 320;
	int actionHeight = 38;      // a bottom action button
	int contentMaxWidth = 1180; // widest content area of a settings / setup screen (reference units)
};
inline const Metrics &metrics() { static const Metrics m; return m; }

// Largest aspect ratio (width / height) the composition may span: on a wider surface the content is composed inside a centred box of this aspect and the
// remaining width is left to the shell map. An owner-adjustable token.
constexpr double kCompositionMaxAspect = 16.0 / 9.0;

static_assert(std::is_trivially_copyable<Palette>::value && std::is_trivially_copyable<Metrics>::value, "tokens are plain data");
} // namespace ModernTheme
