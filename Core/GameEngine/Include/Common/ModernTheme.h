// SPDX-License-Identifier: GPL-3.0-or-later
// UI.2 Modern Experience design system: one Generals visual language, controlled title variants (pure data, engine-free).
//
// Everything a screen needs to LOOK like Generals comes from here as tokens; screens never carry colours, font names or sizes of their own. A title variant (Generals, Zero Hour) only
// changes tokens (accent colour, logo, tagline); later variants (USA / China / GLA HUD) are further rows of the same tables. Typography is expressed as ROLES; each role maps to the
// retail font families already shipped with the game ("Generals", Arial, Times New Roman at the retail point sizes), so the recognizable character is kept and the mapping can evolve
// (a different family or size) in this one table without touching a screen.
#pragma once

#include <cstdint>
#include <type_traits>

namespace ModernTheme
{
struct Color
{
	std::uint8_t r = 0, g = 0, b = 0, a = 255;
	std::uint32_t argb() const { return (std::uint32_t(a) << 24) | (std::uint32_t(r) << 16) | (std::uint32_t(g) << 8) | std::uint32_t(b); }
};
constexpr Color rgba(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255) { return Color{ r, g, b, a }; }

enum class Title : std::uint8_t { Generals = 0, ZeroHour = 1, Count };

// ---- typography ----------------------------------------------------------------------------------------------------------------------------------------------------------------
enum class TypeRole : std::uint8_t
{
	Title,            // page / screen title
	SectionHeading,   // a group heading inside a screen
	MenuPrimary,      // main menu action
	MenuSecondary,    // lesser action (Back, Exit)
	Body,             // descriptive text
	Label,            // control label
	Helper,           // helper / status / footer text
	Count
};

struct TypeStyle
{
	const char *family;   // a retail font family
	int referenceSize;    // point size at UI scale 100 %
	bool bold;
};

inline const TypeStyle &type(TypeRole role)
{
	static const TypeStyle table[static_cast<int>(TypeRole::Count)] = {
		{ "Generals", 20, false },          // Title            (retail: "Generals 20", 28 windows)
		{ "Generals", 15, true  },          // SectionHeading
		{ "Generals", 15, false },          // MenuPrimary      (retail main-menu button font)
		{ "Generals", 15, false },          // MenuSecondary
		{ "Arial",    10, false },          // Body             (retail: Arial 10, the most common body font)
		{ "Arial",    10, true  },          // Label
		{ "Arial",     8, false },          // Helper
	};
	return table[static_cast<int>(role)];
}

// ---- palette -------------------------------------------------------------------------------------------------------------------------------------------------------------------
struct Palette
{
	Color panelFill;       // translucent command-console surface: the shell map stays readable behind it
	Color panelEdge;       // outer frame
	Color accent;          // title variant accent: heading rule, corner brackets, focus bracket
	Color rule;            // separators
	Color textPrimary;     // retail: enabled text white
	Color textHilite;      // retail: hilite lime (186,255,12)
	Color textDisabled;    // retail: disabled blue-grey
	Color textMuted;       // helper text
	Color focus;           // keyboard / hover focus bracket
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
	// Shared base: steel-blue console surface, white text, retail lime hilite. Only the accent (and the logo) differ between the titles.
	static const Variant table[static_cast<int>(Title::Count)] = {
		{ "Generals",
		  { rgba(8, 14, 26, 214), rgba(112, 128, 150, 255), rgba(212, 175, 72, 255), rgba(70, 86, 110, 255),
		    rgba(255, 255, 255, 255), rgba(186, 255, 12, 255), rgba(62, 64, 92, 255), rgba(150, 164, 184, 255), rgba(186, 255, 12, 255) },
		  "GeneralsLogo", 370, 120 },
		{ "ZeroHour",
		  { rgba(8, 14, 26, 214), rgba(112, 128, 150, 255), rgba(226, 112, 36, 255), rgba(70, 86, 110, 255),
		    rgba(255, 255, 255, 255), rgba(186, 255, 12, 255), rgba(62, 64, 92, 255), rgba(150, 164, 184, 255), rgba(255, 190, 70, 255) },
		  "GeneralsLogo", 370, 120 },
	};
	return table[static_cast<int>(title)];
}

// ---- metrics (reference units; the layout multiplies by the UI scale) ------------------------------------------------------------------------------------------------------------
struct Metrics
{
	int panelPreferredWidth = 360;
	int panelMinWidth = 300;
	int panelMaxWidth = 420;
	int panelPadding = 18;
	int itemHeight = 40;        // the retail button art is 40 high
	int itemGap = 8;
	int headingHeight = 30;
	int ruleThickness = 2;
	int footnoteHeight = 16;
	int edgeMargin = 28;        // minimum distance of the panel from the physical screen edge
	int bracketLength = 14;     // corner bracket / focus bracket arm
	int panelMaxHeightPercent = 62;   // the command panel is a sidebar, not a full-screen canvas: it never takes more than this share of the surface height (the UI scale is reduced to fit)
};
inline const Metrics &metrics() { static const Metrics m; return m; }

// Largest aspect ratio (width / height) the composition may span: on a wider surface the content is composed inside a centred box of this aspect and the
// remaining width is left to the shell map. 16:9 keeps a 21:9 / 32:9 menu from hugging the physical screen edge. An owner-adjustable token.
constexpr double kCompositionMaxAspect = 16.0 / 9.0;

static_assert(std::is_trivially_copyable<Palette>::value && std::is_trivially_copyable<Metrics>::value, "tokens are plain data");
} // namespace ModernTheme
