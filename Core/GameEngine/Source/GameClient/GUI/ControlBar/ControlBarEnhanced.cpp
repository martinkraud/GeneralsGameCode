/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 TheSuperHackers
**	SPDX-License-Identifier: GPL-3.0-or-later
*/

// FILE: ControlBarEnhanced.cpp ////////////////////////////////////////////////
//
// Enhanced gameplay HUD (USA prototype): presentation only.
//
// The control bar keeps every window, callback, command and piece of state it has. When the Enhanced presentation is
// active (EnhancedHud::active(): requested, modern display, USA side) the existing windows are *moved* into a bounded,
// bottom-flush, centred island described by Common/EnhancedHudLayout.h, and a modular procedural frame is drawn behind
// them instead of the 800 x 600 scheme art. Everything is recorded before it is moved, so the Original geometry can be
// restored exactly. Nothing here executes a command or reads gameplay state.
//
//-----------------------------------------------------------------------------

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/DisplayRuntime.h"
#include "Common/EnhancedHud.h"
#include "Common/EnhancedHudWindows.h"
#include "Common/EnhancedHudImageFit.h"
#include "GameClient/EnhancedHudImage.h"
#include "GameClient/Image.h"
#include "Common/NameKeyGenerator.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Display.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/Color.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/Mouse.h"
#include "Common/Player.h"
#include "GameLogic/RankInfo.h"
#include "Common/Radar.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <set>
#include <string>

namespace
{

struct Original
{
	ICoord2D pos;      // relative to the parent
	ICoord2D size;
	Bool hidden;
};

// Windows of ControlBar.wnd moved by the Enhanced layout (valid while the master window is the same object).
std::map<GameWindow *, Original> s_bar;
GameWindow *s_barMaster = nullptr;
// Windows of the special power shortcut layout (rebuilt per match; cleared when it is recreated).
std::map<GameWindow *, Original> s_powers;
GameWindow *s_powersParent = nullptr;

void remember( std::map<GameWindow *, Original> &store, GameWindow *win )
{
	if( !win || store.find( win ) != store.end() )
		return;
	Original o;
	win->winGetPosition( &o.pos.x, &o.pos.y );
	win->winGetSize( &o.size.x, &o.size.y );
	o.hidden = win->winIsHidden() ? TRUE : FALSE;
	store[ win ] = o;
}

GameWindow *barWindow( const char *name )
{
	AsciiString full( "ControlBar.wnd:" );
	full.concat( name );
	return TheWindowManager->winGetWindowFromId( nullptr, TheNameKeyGenerator->nameToKey( full ) );
}

void restoreStore( std::map<GameWindow *, Original> &store )
{
	for( std::map<GameWindow *, Original>::iterator it = store.begin(); it != store.end(); ++it )
	{
		GameWindow *win = it->first;
		const Original &o = it->second;
		win->winSetPosition( o.pos.x, o.pos.y );
		win->winSetSize( o.size.x, o.size.y );
		win->winHide( o.hidden );
	}
	store.clear();
}

}  // namespace

namespace
{
void powersPanelDraw( GameWindow *window, WinInstanceData *instData );
GameWinDrawFunc s_powersOriginalDraw = nullptr;
struct SlotArt { const Image *enabled; const Image *disabled; };
std::map<GameWindow *, SlotArt> s_powerSlotArt;     // retail frame art of the shortcut slots (restored for Original)
}

//-----------------------------------------------------------------------------
// Draw-time image fitting (windows registered by the Enhanced layout only).
namespace
{
std::set<GameWindow *> s_aspectFit;
std::set<GameWindow *> s_gauges;
}

void EnhancedHudImage::clear()
{
	s_aspectFit.clear();
	s_gauges.clear();
}

void EnhancedHudImage::registerAspectFit( GameWindow *window )
{
	if( window )
		s_aspectFit.insert( window );
}

void EnhancedHudImage::registerGauge( GameWindow *window )
{
	if( window )
		s_gauges.insert( window );
}

void EnhancedHudImage::fitImage( GameWindow *window, const Image *image, ICoord2D *start, ICoord2D *end )
{
	if( !image || !start || !end || s_aspectFit.empty() || !EnhancedHud::active() || s_aspectFit.find( window ) == s_aspectFit.end() )
		return;
	const EnhancedHudLayout::Rect area = { start->x, start->y, end->x - start->x, end->y - start->y };
	const EnhancedHudLayout::Rect r = EnhancedHudImageFit::fit( EnhancedHudImageFit::Class::AspectFit, area,
	                                                           image->getImageWidth(), image->getImageHeight() );
	if( r.empty() )
		return;
	start->x = r.x;
	start->y = r.y;
	end->x = r.right();
	end->y = r.bottom();
}

Real EnhancedHudImage::gaugeScale( GameWindow *window, Int windowCross, Int imageCross )
{
	if( s_gauges.empty() || !EnhancedHud::active() || s_gauges.find( window ) == s_gauges.end() )
		return 1.0f;
	return (Real)EnhancedHudImageFit::gaugeScale( windowCross, imageCross );
}

//-----------------------------------------------------------------------------
void ControlBar::applyEnhancedLayout()
{
	if( !EnhancedHud::active() || !TheWindowManager )
		return;
	GameWindow *master = m_contextParent[ CP_MASTER ];
	if( !master )
		return;
	if( s_barMaster != master )
	{
		s_bar.clear();        // the control bar was rebuilt: stale entries must never be applied
		s_barMaster = master;
	}
	const EnhancedHudLayout::Result &layout = EnhancedHud::layout();

	// 1.-3. the master window becomes the island (bottom flush, centred, bounded), the table windows are placed and the
	//       containers are fitted around their children so the interactive regions equal the drawn ones. This is the same
	//       code the hit-test unit tests run (Common/EnhancedHudWindows.h).
	m_defaultControlBarPosition.x = layout.island.x;
	m_defaultControlBarPosition.y = layout.island.y;
	EnhancedHudWindows::apply<GameWindow>( master, layout,
		[]( const char *name ) { return barWindow( name ); },
		[]( GameWindow *win ) { remember( s_bar, win ); },
		[]( GameWindow *win )
		{
			// decorative retail art windows: Munkee, CenterBackground and the unnamed art panels
			const Int id = win->winGetWindowId();
			return id == TheNameKeyGenerator->nameToKey( "ControlBar.wnd:Munkee" ) ||
			       id == TheNameKeyGenerator->nameToKey( "ControlBar.wnd:CenterBackground" ) ||
			       id == TheNameKeyGenerator->nameToKey( "ControlBar.wnd:" );
		} );

	// The retail bitmaps of the radar utility buttons and of the XP strip were authored for 30 x 22 / 21 x 107 pixels and
	// are visibly low resolution at the Enhanced scale: Enhanced draws procedural plates and a rank ladder instead (see
	// drawEnhancedHudFrame). The scheme re-applies the retail images on every init, so Original is unaffected.
	static const char *const clearedButtons[] = { "ButtonOptions", "ButtonIdleWorker", "ButtonPlaceBeacon", "PopupCommunicator" };
	for( int i = 0; i < 4; ++i )
	{
		if( GameWindow *win = barWindow( clearedButtons[ i ] ) )
		{
			GadgetButtonSetEnabledImage( win, nullptr );
			GadgetButtonSetHiliteImage( win, nullptr );
			GadgetButtonSetHiliteSelectedImage( win, nullptr );
			GadgetButtonSetDisabledImage( win, nullptr );
		}
	}
	if( GameWindow *win = barWindow( "ExpBarForeground" ) )
		win->winSetEnabledImage( 0, nullptr );
	// The attack warning lamp (retail SUUAttackI/H, 22 x 24 px) keeps its window and its enable-toggle flashing (the
	// disabled state is the lit state); only its tiny bitmap is replaced by a procedural lamp.
	if( GameWindow *win = barWindow( "WinUAttack" ) )
	{
		win->winSetEnabledImage( 0, nullptr );
		win->winSetDisabledImage( 0, nullptr );
	}

	// image fitting policy (Common/EnhancedHudImageFit.h): which reused retail bitmaps are aspect-fitted / gauges
	EnhancedHudImage::clear();
	{
		int n = 0;
		for( const EnhancedHudImageFit::Policy *p = EnhancedHudImageFit::policies( n ); n-- > 0; ++p )
		{
			if( p->cls != EnhancedHudImageFit::Class::AspectFit && p->cls != EnhancedHudImageFit::Class::Gauge )
				continue;
			if( std::strncmp( p->window, "Power:", 6 ) == 0 )
				continue;
			if( GameWindow *win = barWindow( p->window ) )
			{
				if( p->cls == EnhancedHudImageFit::Class::Gauge )
					EnhancedHudImage::registerGauge( win );
				else
					EnhancedHudImage::registerAspectFit( win );
			}
		}
	}

	applyEnhancedPowerShortcutLayout();
}

//-----------------------------------------------------------------------------
void ControlBar::restoreOriginalLayout()
{
	// A strict no-op unless the Enhanced layout was applied: the Original presentation is never touched.
	if( s_barMaster == nullptr && s_powersParent == nullptr )
		return;
	EnhancedHudImage::clear();
	if( s_barMaster && s_barMaster == m_contextParent[ CP_MASTER ] && !s_bar.empty() )
	{
		const std::map<GameWindow *, Original>::const_iterator master = s_bar.find( s_barMaster );
		if( master != s_bar.end() )
		{
			m_defaultControlBarPosition.x = master->second.pos.x;   // the position captured before the island was applied
			m_defaultControlBarPosition.y = master->second.pos.y;
		}
		restoreStore( s_bar );
	}
	else
		s_bar.clear();
	if( s_powersParent && s_powersParent == m_specialPowerShortcutParent && !s_powers.empty() )
	{
		if( s_powersOriginalDraw )
			s_powersParent->winSetDrawFunc( s_powersOriginalDraw );
		for( std::map<GameWindow *, SlotArt>::const_iterator it = s_powerSlotArt.begin(); it != s_powerSlotArt.end(); ++it )
		{
			it->first->winSetEnabledImage( 0, it->second.enabled );
			it->first->winSetDisabledImage( 0, it->second.disabled );
		}
		s_powerSlotArt.clear();
		restoreStore( s_powers );
	}
	else
		s_powers.clear();
	s_barMaster = nullptr;
	s_powersParent = nullptr;
	s_powersOriginalDraw = nullptr;
}

//-----------------------------------------------------------------------------
void ControlBar::applyEnhancedPowerShortcutLayout()
{
	if( !EnhancedHud::active() )
		return;
	GameWindow *parent = m_specialPowerShortcutParent;
	if( !parent )
		return;
	if( s_powersParent != parent )
	{
		s_powers.clear();     // the shortcut layout is created per match
		s_powerSlotArt.clear();
		s_powersParent = parent;
		s_powersOriginalDraw = nullptr;
	}
	const EnhancedHudLayout::Result &layout = EnhancedHud::layout();
	// the Global Powers frame is drawn by the shortcut window itself (so it slides/hides with it)
	if( s_powersOriginalDraw == nullptr )
		s_powersOriginalDraw = parent->winGetDrawFunc();
	parent->winSetDrawFunc( powersPanelDraw );
	const EnhancedHudLayout::Rect panel = layout.powerPanel;
	// the panel is scaled uniformly: its children keep their relationships (original sizes were created at the modern
	// layout scale; the island scale may be smaller on narrow surfaces)
	const double layoutScale = DisplayRuntime::uiScale() > 0.0 ? DisplayRuntime::uiScale() : 1.0;
	const double factor = layout.scale / layoutScale;
	auto original = []( GameWindow *win, int *out )
	{
		remember( s_powers, win );
		const Original &o = s_powers[ win ];
		out[ 0 ] = o.pos.x; out[ 1 ] = o.pos.y; out[ 2 ] = o.size.x; out[ 3 ] = o.size.y;
	};
	auto rememberWin = []( GameWindow *win ) { remember( s_powers, win ); };
	if( layout.powerHorizontal )
	{
		// compact fallback: the same five shortcut buttons in a row (slot = the button's own index)
		EnhancedHudWindows::applyPowerRow<GameWindow>( parent, panel, factor, original, rememberWin,
			[this]( GameWindow *child )
			{
				for( int i = 0; i < MAX_SPECIAL_POWER_SHORTCUTS; ++i )
					if( m_specialPowerShortcutButtonParents[ i ] == child )
						return i;
				return -1;
			} );
	}
	else
		EnhancedHudWindows::applyPowerColumn<GameWindow>( parent, panel, factor, original, rememberWin );
	// the retail frame art of each slot (59 x 56 box) is replaced by the Enhanced plate: remember it, then clear it
	for( GameWindow *slot = parent->winGetChild(); slot; slot = slot->winGetNext() )
	{
		if( s_powerSlotArt.find( slot ) == s_powerSlotArt.end() )
		{
			SlotArt art = { slot->winGetEnabledImage( 0 ), slot->winGetDisabledImage( 0 ) };
			s_powerSlotArt[ slot ] = art;
		}
		slot->winSetEnabledImage( 0, nullptr );
		slot->winSetDisabledImage( 0, nullptr );
	}
	// special power icons: aspect-fitted inside their (full-plate) buttons
	for( int i = 0; i < MAX_SPECIAL_POWER_SHORTCUTS; ++i )
		EnhancedHudImage::registerAspectFit( m_specialPowerShortcutButtons[ i ] );
}

//-----------------------------------------------------------------------------
Bool ControlBar::getEnhancedTooltipPosition( Int width, Int height, Int *x, Int *y ) const
{
	if( !EnhancedHud::active() || !x || !y || !TheDisplay )
		return FALSE;
	const EnhancedHudLayout::Result &layout = EnhancedHud::layout();
	const Int surfaceW = (Int)TheDisplay->getWidth();
	Int px = layout.commandFrame.x + ( layout.commandFrame.w - width ) / 2;
	px = std::max( 0, std::min( px, surfaceW - width ) );
	Int py = layout.island.y - height - 4;
	py = std::max( 0, py );
	*x = px;
	*y = py;
	return TRUE;
}

//-----------------------------------------------------------------------------
// The procedural USA console (replaces the scheme art while Enhanced is active). ONE renderer: every colour and every shape
// decision (corner profile, fasteners, seams, vents, ticks, state rails, bezel and well depth) comes from the active
// EnhancedHudTheme; geometry comes from EnhancedHudLayout; nothing here reads or changes gameplay state except the read-only
// rank/radar/window states that the retail widgets already show.
namespace
{

typedef EnhancedHudLayout::Rect HRect;

struct HudPalette
{
	Color steelDark, steelMid, steel, panel, panelLow, well, wellEdge, accent, accentDim, highlight, shadow, icon, iconDim,
	      lit, litDim, ready, warning, warningDark, warningInk, outline;
	const EnhancedHudTheme::Theme *theme;
};

Color themeColor( const EnhancedHudTheme::Rgba &c )
{
	return GameMakeColor( c.r, c.g, c.b, c.a );
}

HudPalette makePalette()
{
	const EnhancedHudTheme::Theme &t = EnhancedHud::theme();
	HudPalette c;
	c.steelDark = themeColor( t.plate );
	c.steelMid = themeColor( t.plateBand );
	c.steel = themeColor( t.plateBand );
	c.panel = themeColor( t.module );
	c.panelLow = themeColor( t.moduleLow );
	c.well = themeColor( t.well );
	c.wellEdge = themeColor( t.wellEdge );
	c.outline = themeColor( t.outline );
	c.accent = themeColor( t.accent );
	c.accentDim = themeColor( t.accentDim );
	c.highlight = themeColor( t.bevelLight );
	c.shadow = themeColor( t.bevelDark );
	c.icon = themeColor( t.icon );
	c.iconDim = themeColor( t.iconDim );
	c.lit = themeColor( t.rankLit );
	c.litDim = themeColor( t.rankDim );
	c.ready = themeColor( t.ready );
	c.warning = themeColor( t.warning );
	c.warningDark = themeColor( t.warningDark );
	c.warningInk = themeColor( t.warningInk );
	c.theme = &t;
	return c;
}

double s_drawScale = 1.0;      // physical pixels per reference unit of the frame being drawn

// A length in reference units as physical pixels (never below one pixel).
Int P( double ref )
{
	return std::max<Int>( 1, (Int)( ref * s_drawScale + 0.5 ) );
}

enum CornerMask { CM_TL = 1, CM_TR = 2, CM_BR = 4, CM_BL = 8, CM_ALL = 15, CM_TOP = 3 };

GameWindow *s_hudMasterForLookup = nullptr;
HRect inflate( const HRect &r, Int g );

// Filled rectangle with machined (45 degree) corners of `k` pixels on the corners selected by `mask`.
void fillChamfered( const HRect &r, Int k, int mask, Color color )
{
	if( r.w <= 0 || r.h <= 0 )
		return;
	k = std::min( k, std::min( r.w, r.h ) / 2 );
	if( k <= 1 || mask == 0 )
	{
		TheDisplay->drawFillRect( r.x, r.y, r.w, r.h, color );
		return;
	}
	const bool top = ( mask & ( CM_TL | CM_TR ) ) != 0, bottom = ( mask & ( CM_BL | CM_BR ) ) != 0;
	const Int midY = r.y + ( top ? k : 0 ), midH = r.h - ( top ? k : 0 ) - ( bottom ? k : 0 );
	if( midH > 0 )
		TheDisplay->drawFillRect( r.x, midY, r.w, midH, color );
	for( Int i = 0; i < k; ++i )
	{
		const Int cut = k - i;
		if( top )
		{
			const Int l = ( mask & CM_TL ) ? cut : 0, rr = ( mask & CM_TR ) ? cut : 0;
			TheDisplay->drawFillRect( r.x + l, r.y + i, r.w - l - rr, 1, color );
		}
		if( bottom )
		{
			const Int l = ( mask & CM_BL ) ? cut : 0, rr = ( mask & CM_BR ) ? cut : 0;
			TheDisplay->drawFillRect( r.x + l, r.y + r.h - 1 - i, r.w - l - rr, 1, color );
		}
	}
}

// One pixel outline following the same chamfered profile.
void edgeChamfered( const HRect &r, Int k, int mask, Color color )
{
	k = std::min( k, std::min( r.w, r.h ) / 2 );
	const Int x0 = r.x, y0 = r.y, x1 = r.x + r.w - 1, y1 = r.y + r.h - 1;
	const Int tl = ( mask & CM_TL ) ? k : 0, tr = ( mask & CM_TR ) ? k : 0, br = ( mask & CM_BR ) ? k : 0, bl = ( mask & CM_BL ) ? k : 0;
	TheDisplay->drawLine( x0 + tl, y0, x1 - tr, y0, 1, color );
	TheDisplay->drawLine( x0 + bl, y1, x1 - br, y1, 1, color );
	TheDisplay->drawLine( x0, y0 + tl, x0, y1 - bl, 1, color );
	TheDisplay->drawLine( x1, y0 + tr, x1, y1 - br, 1, color );
	if( tl ) TheDisplay->drawLine( x0, y0 + tl, x0 + tl, y0, 1, color );
	if( tr ) TheDisplay->drawLine( x1 - tr, y0, x1, y0 + tr, 1, color );
	if( br ) TheDisplay->drawLine( x1, y1 - br, x1 - br, y1, 1, color );
	if( bl ) TheDisplay->drawLine( x0 + bl, y1, x0, y1 - bl, 1, color );
}

// A machined plate: body, outline, light top/left edge and dark bottom/right edge. `pressed` swaps the edges (depressed).
void drawBevelPlate( const HRect &r, Int k, int mask, Color body, Color outline, bool pressed, const HudPalette &c )
{
	if( r.w < 4 || r.h < 4 )
		return;
	fillChamfered( r, k, mask, body );
	edgeChamfered( r, k, mask, outline );
	const Color lightEdge = pressed ? c.shadow : c.highlight, darkEdge = pressed ? c.highlight : c.shadow;
	const Int a = ( mask & CM_TL ) ? k : 1, b = ( mask & CM_TR ) ? k : 1, d = ( mask & CM_BL ) ? k : 1, e = ( mask & CM_BR ) ? k : 1;
	TheDisplay->drawLine( r.x + a + 1, r.y + 1, r.x + r.w - b - 1, r.y + 1, 1, lightEdge );
	TheDisplay->drawLine( r.x + 1, r.y + a + 1, r.x + 1, r.y + r.h - d - 1, 1, lightEdge );
	TheDisplay->drawLine( r.x + d + 1, r.y + r.h - 2, r.x + r.w - e - 1, r.y + r.h - 2, 1, darkEdge );
	TheDisplay->drawLine( r.x + r.w - 2, r.y + b + 1, r.x + r.w - 2, r.y + r.h - e - 1, 1, darkEdge );
}

// A recessed bay: dark body, inner shadow on the top/left (depth set by the theme), faint lit lip on the bottom/right.
void drawRecess( const HRect &r, const HudPalette &c, Int depth = -1 )
{
	if( r.w < 3 || r.h < 3 )
		return;
	if( depth < 0 )
		depth = P( c.theme->wellDepthRef );
	depth = std::min( depth, std::min( r.w, r.h ) / 3 );
	TheDisplay->drawFillRect( r.x, r.y, r.w, r.h, c.well );
	TheDisplay->drawFillRect( r.x, r.y, r.w, depth, c.shadow );
	TheDisplay->drawFillRect( r.x, r.y, depth, r.h, c.shadow );
	TheDisplay->drawLine( r.x + 1, r.y + r.h - 1, r.x + r.w, r.y + r.h - 1, 1, c.wellEdge );
	TheDisplay->drawLine( r.x + r.w - 1, r.y + 1, r.x + r.w - 1, r.y + r.h, 1, c.wellEdge );
}

// Small screw head (square with a slot).
void drawScrew( Int cx, Int cy, const HudPalette &c )
{
	const Int rad = std::max<Int>( 1, P( 1.4 ) );
	TheDisplay->drawFillRect( cx - rad, cy - rad, 2 * rad + 1, 2 * rad + 1, c.wellEdge );
	TheDisplay->drawLine( cx - rad, cy - rad, cx + rad, cy - rad, 1, c.highlight );
	TheDisplay->drawLine( cx - rad + 1, cy, cx + rad - 1, cy, 1, c.shadow );
}

// Short group of vent lines.
void drawVents( Int x, Int y, Int w, Int h, int lines, const HudPalette &c )
{
	if( lines < 1 || w < 4 || h < 2 )
		return;
	const Int step = std::max<Int>( 2, h / lines );
	for( int i = 0; i < lines; ++i )
	{
		TheDisplay->drawLine( x, y + i * step, x + w, y + i * step, 1, c.shadow );
		TheDisplay->drawLine( x, y + i * step + 1, x + w, y + i * step + 1, 1, c.accentDim );
	}
}

// A module of the console: chamfered panel with bevel, a header rail, a short accent cap (all from the theme).
void drawModule( const HRect &r, const HudPalette &c, bool ventsRight )
{
	const EnhancedHudTheme::Theme &th = *c.theme;
	if( r.w < 8 || r.h < 8 )
		return;
	const Int k = th.moduleCorner == EnhancedHudTheme::Corner::Chamfer ? P( th.moduleCornerRef ) : 0;
	if( th.bevelModules )
		drawBevelPlate( r, k, CM_ALL, c.panel, c.outline, false, c );
	else
	{
		fillChamfered( r, k, CM_ALL, c.panel );
		edgeChamfered( r, k, CM_ALL, c.outline );
	}
	// inset header rail under the top edge
	const Int rail = r.y + P( 6.5 );
	TheDisplay->drawLine( r.x + P( 7 ), rail, r.x + r.w * 6 / 10, rail, 1, c.shadow );
	TheDisplay->drawLine( r.x + P( 7 ), rail + 1, r.x + r.w * 6 / 10, rail + 1, 1, c.wellEdge );
	if( th.moduleCaps )
		TheDisplay->drawFillRect( r.x + k + P( 2 ), r.y + 1, std::max<Int>( 8, r.w * th.capPermille / 1000 ), std::max<Int>( 2, P( 1.5 ) ), c.accent );
	if( th.vents && ventsRight )
		drawVents( r.x + r.w - P( 30 ), r.y + P( 4 ), P( 20 ), P( 8 ), 3, c );
}

// Filled five point star (scanline, even-odd).
void fillStar( Int cx, Int cy, Int outer, Color color )
{
	if( outer < 2 )
	{
		TheDisplay->drawFillRect( cx - 1, cy - 1, 3, 3, color );
		return;
	}
	const double inner = outer * 0.42, pi = 3.14159265358979323846;
	double vx[ 10 ], vy[ 10 ];
	for( int i = 0; i < 10; ++i )
	{
		const double a = -pi / 2.0 + i * pi / 5.0;
		const double r = ( i % 2 == 0 ) ? outer : inner;
		vx[ i ] = cx + std::cos( a ) * r;
		vy[ i ] = cy + std::sin( a ) * r;
	}
	for( Int y = cy - outer; y <= cy + outer; ++y )
	{
		double xs[ 10 ];
		int n = 0;
		const double yy = y + 0.5;
		for( int i = 0; i < 10; ++i )
		{
			const int j = ( i + 1 ) % 10;
			if( ( vy[ i ] <= yy && vy[ j ] > yy ) || ( vy[ j ] <= yy && vy[ i ] > yy ) )
				xs[ n++ ] = vx[ i ] + ( yy - vy[ i ] ) * ( vx[ j ] - vx[ i ] ) / ( vy[ j ] - vy[ i ] );
		}
		std::sort( xs, xs + n );
		for( int k = 0; k + 1 < n; k += 2 )
			TheDisplay->drawLine( (Int)( xs[ k ] + 0.5 ), y, (Int)( xs[ k + 1 ] + 0.5 ), y, 1, color );
	}
}

enum Glyph { GLYPH_OPTIONS, GLYPH_WORKER, GLYPH_BEACON, GLYPH_COMMS };

void drawGlyph( Glyph glyph, const HRect &r, Color color )
{
	const Int m = std::max( 3, r.h / 4 );
	const Int x = r.x + m, y = r.y + m, w = r.w - 2 * m, h = r.h - 2 * m;
	if( w < 4 || h < 4 )
		return;
	switch( glyph )
	{
		case GLYPH_OPTIONS:      // three bars
			for( int i = 0; i < 3; ++i )
				TheDisplay->drawFillRect( x, y + ( h - 2 ) * i / 2, w, std::max( 2, h / 5 ), color );
			break;
		case GLYPH_WORKER:       // head and shoulders
			TheDisplay->drawFillRect( x + w / 2 - h / 5, y, std::max( 2, 2 * h / 5 ), std::max( 2, 2 * h / 5 ), color );
			TheDisplay->drawFillRect( x + w / 6, y + h / 2, 2 * w / 3, h - h / 2, color );
			break;
		case GLYPH_BEACON:       // pole and pennant
			TheDisplay->drawFillRect( x + w / 4, y, std::max( 2, w / 8 ), h, color );
			for( Int i = 0; i < h / 2; ++i )
				TheDisplay->drawLine( x + w / 4, y + i, x + w / 4 + w / 2 - i * w / h, y + i, 1, color );
			break;
		case GLYPH_COMMS:        // speech box with a tail and two text bars
			TheDisplay->drawOpenRect( x, y, w, h * 3 / 4, std::max( 1, h / 8 ), color );
			TheDisplay->drawFillRect( x + w / 6, y + h * 3 / 4, std::max( 2, w / 6 ), h / 4, color );
			TheDisplay->drawFillRect( x + w / 5, y + h / 5, w * 3 / 5, std::max( 1, h / 10 ), color );
			TheDisplay->drawFillRect( x + w / 5, y + h * 2 / 5, w * 2 / 5, std::max( 1, h / 10 ), color );
			break;
	}
}

const int kHudWinCount = 8 + 12;
GameWindow *s_hudWin[ kHudWinCount ];     // 0 radar, 1-4 utility buttons, 5 XP trough, 6 attack lamp, 7 CommandWindow, 8.. command buttons
const char *const s_hudWinNames[ kHudWinCount ] = { "LeftHUD", "ButtonOptions", "ButtonIdleWorker", "ButtonPlaceBeacon",
                                         "PopupCommunicator", "ExpBarForeground", "WinUAttack", "CommandWindow",
                                         "ButtonCommand01", "ButtonCommand02", "ButtonCommand03", "ButtonCommand04",
                                         "ButtonCommand05", "ButtonCommand06", "ButtonCommand07", "ButtonCommand08",
                                         "ButtonCommand09", "ButtonCommand10", "ButtonCommand11", "ButtonCommand12" };

bool windowChainShown( GameWindow *win )
{
	for( ; win; win = win->winGetParent() )
		if( win->winIsHidden() )
			return false;
	return true;
}

HRect windowRect( GameWindow *win )
{
	Int x = 0, y = 0, w = 0, h = 0;
	win->winGetScreenPosition( &x, &y );
	win->winGetSize( &w, &h );
	return HRect{ x, y, w, h };
}

HRect inflate( const HRect &r, Int g )
{
	return HRect{ r.x - g, r.y - g, r.w + 2 * g, r.h + 2 * g };
}

// Pointer interaction state of a rectangle, from the live mouse (the retail window logic itself is untouched).
void pointerState( const HRect &r, bool *hover, bool *down )
{
	*hover = *down = false;
	if( !TheMouse )
		return;
	const MouseIO *io = TheMouse->getMouseStatus();
	*hover = io->pos.x >= r.x && io->pos.x <= r.x + r.w && io->pos.y >= r.y && io->pos.y <= r.y + r.h;
	*down = *hover && io->leftState == MBS_Down;
}

// The "under attack" warning lamp (accepted design). The retail window still flashes by toggling its enable state
// (disabled = lit); this draws the lamp: a solid red plate with a white warning triangle when lit, dark idle lamp otherwise.
bool drawAttackLamp( GameWindow *win, const HudPalette &c, bool *lit )
{
	*lit = false;
	if( !win || win->winIsHidden() )
		return false;
	const HRect r = windowRect( win );
	const bool on = !BitIsSet( win->winGetStatus(), WIN_STATUS_ENABLED );
	*lit = on;
	const Color red = c.warning, redDark = c.warningDark, white = c.warningInk;
	// housing: the lamp is built into the console
	const HRect housing = inflate( r, P( 2 ) );
	drawRecess( housing, c, P( 1.5 ) );
	if( c.theme->fasteners )
	{
		drawScrew( housing.x + P( 1.5 ), housing.y + housing.h / 2, c );
		drawScrew( housing.x + housing.w - P( 1.5 ) - 1, housing.y + housing.h / 2, c );
	}
	drawBevelPlate( r, 0, 0, on ? red : redDark, on ? white : c.accentDim, false, c );
	const Int cx = r.x + r.w / 2, top = r.y + std::max<Int>( 2, r.h / 6 ), bottom = r.y + r.h - std::max<Int>( 2, r.h / 6 );
	const Int half = std::max<Int>( 3, ( bottom - top ) * 6 / 10 );
	const Color tri = on ? white : c.iconDim;
	for( Int row = top; row <= bottom; ++row )
	{
		const Int span = half * ( row - top ) / std::max<Int>( 1, bottom - top );
		TheDisplay->drawLine( cx - span, row, cx + span, row, 1, tri );
	}
	const Color mark = on ? red : redDark;
	const Int mh = std::max<Int>( 2, ( bottom - top ) / 3 );
	TheDisplay->drawFillRect( cx - 1, top + ( bottom - top ) / 3, std::max<Int>( 2, r.w / 14 ), mh, mark );
	TheDisplay->drawFillRect( cx - 1, bottom - std::max<Int>( 2, ( bottom - top ) / 6 ) - 1, std::max<Int>( 2, r.w / 14 ), std::max<Int>( 2, ( bottom - top ) / 8 ), mark );
	return on;
}

// A radar utility control drawn as a hardware key: machined plate, status LED strip, glyph. Hover = blue rim and lit LED,
// pressed = depressed plate, disabled = muted.
void drawUtilityKey( GameWindow *win, Glyph glyph, const HudPalette &c )
{
	if( !win || win->winIsHidden() )
		return;
	const HRect r = windowRect( win );
	bool hover, down;
	pointerState( r, &hover, &down );
	const bool enabled = BitIsSet( win->winGetStatus(), WIN_STATUS_ENABLED ) ? true : false;
	const Int k = c.theme->plateCorner == EnhancedHudTheme::Corner::Chamfer ? P( c.theme->plateCornerRef ) : 0;
	const Color body = !enabled ? c.panelLow : ( down ? c.panelLow : ( hover ? c.panel : c.panel ) );
	drawBevelPlate( r, k, CM_ALL, body, enabled ? ( hover ? c.accent : c.outline ) : c.shadow, down && enabled, c );
	// LED strip along the top edge
	if( c.theme->stateRails )
	{
		const Int ledH = std::max<Int>( 1, P( 1.2 ) );
		TheDisplay->drawFillRect( r.x + k + 1, r.y + 2, r.w - 2 * k - 2, ledH, enabled ? ( hover ? c.ready : c.accentDim ) : c.shadow );
	}
	HRect glyphBox = inflate( r, -P( 1.5 ) );
	if( down && enabled ) { glyphBox.x += 1; glyphBox.y += 1; }
	drawGlyph( glyph, glyphBox, enabled ? c.icon : c.iconDim );
}

// The Global Powers surface: nothing but one compact machined plate per VISIBLE shortcut slot, with an even gap and no
// enclosing background. It is the draw function of the (independent, top-level) shortcut parent window while Enhanced is
// active, so it follows that window's position, slide-in and hidden state exactly. The slots are the existing ButtonParent /
// ButtonCommand windows (retail order); ready (enabled) plates get the blue state rail, others recede; cooldown sweep and
// tooltips are the button's own. The plates are mounted on the RIGHT edge: their outer side is square, the inner side
// carries the machined corner cuts. The retail frame art of the slot is not drawn in Enhanced (cleared on apply).
void powersPanelDraw( GameWindow *window, WinInstanceData * )
{
	if( !EnhancedHud::active() || !TheDisplay || !window )
		return;
	const EnhancedHudLayout::Result &l = EnhancedHud::layout();
	const HudPalette c = makePalette();
	s_drawScale = l.scale;
	const Int k = c.theme->plateCorner == EnhancedHudTheme::Corner::Chamfer ? P( c.theme->plateCornerRef ) : 0;
	for( GameWindow *slot = window->winGetChild(); slot; slot = slot->winGetNext() )
	{
		if( slot->winIsHidden() )
			continue;
		GameWindow *button = slot->winGetChild();
		if( !button || button->winIsHidden() )
			continue;
		const HRect plate = EnhancedHudLayout::powerPlateRect( windowRect( button ), l.scale );
		const bool ready = BitIsSet( button->winGetStatus(), WIN_STATUS_ENABLED ) ? true : false;
		bool hover, down;
		pointerState( plate, &hover, &down );
		// the cut corners face the battlefield (left); in the compact row both sides are cut
		const int mask = l.powerHorizontal ? CM_ALL : ( CM_TL | CM_BL );
		drawBevelPlate( plate, k, mask, c.steelDark, ready ? ( hover ? c.accent : c.ready ) : c.outline, down && ready, c );
		const HRect icon = inflate( plate, -std::max<Int>( 2, P( 1.5 ) ) );
		drawRecess( icon, c, P( 1.2 ) );
		if( c.theme->stateRails )
		{
			const Int railW = std::max<Int>( 2, P( 1.6 ) );
			const Int railX = l.powerHorizontal ? plate.x + k : plate.x + 1;
			TheDisplay->drawFillRect( railX, plate.y + k + 1, railW, plate.h - 2 * k - 2, ready ? c.ready : c.shadow );
		}
	}
}

}  // namespace

void drawEnhancedHudFrame()
{
	if( !EnhancedHud::active() || !TheDisplay )
		return;
	EnhancedHudLayout::Result l = EnhancedHud::layout();
	const HudPalette c = makePalette();
	const EnhancedHudTheme::Theme &th = *c.theme;
	s_drawScale = l.scale;
	const Int t = P( 1.5 );                  // base line thickness follows the UI scale
	const Int g = std::max<Int>( 2, P( 4 ) );  // gutter between a well and what it holds

	// The frame follows the real control bar window (stage changes, hide/show slide): the windows are its children, so
	// the offset of the master from the layout position is exactly how far the whole console has been moved. The control
	// bar's own state stays the only authority; nothing here keeps a visibility state of its own.
	if( s_barMaster )
	{
		const HRect m = windowRect( s_barMaster );
		l = EnhancedHudLayout::translated( l, m.x - l.island.x, m.y - l.island.y );
	}

	// windows used for the control plates (looked up once per control bar)
	if( s_barMaster != s_hudMasterForLookup )
	{
		s_hudMasterForLookup = s_barMaster;
		for( int i = 0; i < kHudWinCount; ++i )
			s_hudWin[ i ] = s_barMaster ? barWindow( s_hudWinNames[ i ] ) : nullptr;
	}
	const HRect isl = l.island;
	const HRect mods[ 4 ] = { l.radarFrame, l.infoFrame, l.commandFrame, l.xpFrame };

	// 1. OUTER CONSOLE: one gunmetal shell with machined top corners, a lighter upper band, a lit top edge and an accent rail
	//    that is broken into one segment per module. The bottom is square and flush with the screen.
	{
		const Int k = th.shellCorner == EnhancedHudTheme::Corner::Chamfer ? P( th.shellCornerRef ) : 0;
		fillChamfered( isl, k, CM_TOP, c.steelDark );
		fillChamfered( HRect{ isl.x, isl.y + k, isl.w, isl.h / 3 }, 0, 0, c.steelMid );
		edgeChamfered( isl, k, CM_TOP, c.outline );
		TheDisplay->drawLine( isl.x + k + 1, isl.y + 1, isl.x + isl.w - k - 1, isl.y + 1, 1, c.highlight );
		const Int railY = isl.y + P( 2.5 );
		if( th.railBreaks )
		{
			for( int i = 0; i < 4; ++i )
				TheDisplay->drawFillRect( mods[ i ].x + P( 6 ), railY, mods[ i ].w - P( 12 ), std::max<Int>( 1, t / 2 + 1 ), c.accentDim );
		}
		else
			TheDisplay->drawFillRect( isl.x + 2 * k, railY, isl.w - 4 * k, std::max<Int>( 1, t / 2 + 1 ), c.accentDim );
		if( th.fasteners )
		{
			drawScrew( isl.x + k + P( 4 ), isl.y + P( 5 ), c );
			drawScrew( isl.x + isl.w - k - P( 4 ) - 1, isl.y + P( 5 ), c );
			drawScrew( isl.x + P( 4 ), isl.y + isl.h - P( 4 ), c );
			drawScrew( isl.x + isl.w - P( 4 ) - 1, isl.y + isl.h - P( 4 ), c );
		}
		if( th.seams )
		{
			// short machined seams in the gaps between modules, with a break in the middle
			for( int i = 0; i < 3; ++i )
			{
				const Int sx = ( mods[ i ].right() + mods[ i + 1 ].x ) / 2;
				const Int y0 = isl.y + P( 12 ), y1 = isl.y + isl.h - P( 10 ), ym = ( y0 + y1 ) / 2;
				TheDisplay->drawLine( sx, y0, sx, ym - P( 6 ), 1, c.shadow );
				TheDisplay->drawLine( sx, ym + P( 6 ), sx, y1, 1, c.shadow );
				TheDisplay->drawLine( sx + 1, y0, sx + 1, ym - P( 6 ), 1, c.wellEdge );
				TheDisplay->drawLine( sx + 1, ym + P( 6 ), sx + 1, y1, 1, c.wellEdge );
			}
		}
	}

	// 2. MODULES: structural panels inset from the shell (radar and General carry vent groups in their headers)
	drawModule( mods[ 0 ], c, true );
	drawModule( mods[ 1 ], c, false );
	drawModule( mods[ 2 ], c, false );
	drawModule( mods[ 3 ], c, true );

	// 3. RADAR MODULE. The radar keeps the map's aspect ratio and (by retail design, W3DRadar::draw) paints the unused part
	//    of its window opaque black. In Enhanced the radar WINDOW is fitted to the map's aspect inside the module's radar
	//    slot (uniform, centred), so the radar fills its window, the mouse mapping stays exact and the map is never
	//    stretched. Around the fitted radar sits a military display bezel: a deep recessed screen in a machined frame with
	//    corner fasteners and tick marks; the same bezel turns red while the attack warning is lit. Original keeps the retail
	//    window and its black bars.
	const bool lampLit = s_hudWin[ 6 ] && !s_hudWin[ 6 ]->winIsHidden() && !BitIsSet( s_hudWin[ 6 ]->winGetStatus(), WIN_STATUS_ENABLED );
	if( GameWindow *radar = s_hudWin[ 0 ] )
	{
		EnhancedHudLayout::Rect dst;
		bool hide = false;
		HRect slot = windowRect( radar );
		if( EnhancedHudLayout::destinationFor( "LeftHUD", dst, hide ) )
			slot = EnhancedHudLayout::toPhysical( l, dst );
		HRect fitted = slot;
		if( TheRadar )
		{
			const Region3D &extent = TheRadar->getMapExtent();
			if( extent.width() > 1.0f && extent.height() > 1.0f )
			{
				const HRect f = EnhancedHudImageFit::containFit( slot, (Int)( extent.width() + 0.5f ), (Int)( extent.height() + 0.5f ) );
				if( !f.empty() )
					fitted = f;
			}
		}
		const HRect now = windowRect( radar );
		if( now.x != fitted.x || now.y != fitted.y || now.w != fitted.w || now.h != fitted.h )
			EnhancedHudWindows::placeAbsolute( radar, fitted );
		drawRecess( slot, c, P( 1.5 ) );
		const Int bz = P( th.bezelRef );
		const HRect frame = inflate( fitted, bz );
		const Color frameEdge = lampLit ? c.warning : c.outline;
		drawBevelPlate( frame, P( 2.5 ), CM_ALL, lampLit ? c.warningDark : c.steelMid, frameEdge, false, c );
		if( lampLit )
			TheDisplay->drawOpenRect( frame.x - 1, frame.y - 1, frame.w + 2, frame.h + 2, 1, c.warning );
		drawRecess( inflate( fitted, 1 ), c, P( 1.2 ) );       // the screen itself sits deeper than the bezel
		if( th.fasteners )
		{
			const Int o = std::max<Int>( 2, bz / 2 );
			drawScrew( frame.x + o, frame.y + o, c );
			drawScrew( frame.x + frame.w - o - 1, frame.y + o, c );
			drawScrew( frame.x + o, frame.y + frame.h - o - 1, c );
			drawScrew( frame.x + frame.w - o - 1, frame.y + frame.h - o - 1, c );
		}
		if( th.tickMarks )
		{
			// graduation ticks along the bottom bezel edge
			const Int ty = frame.y + frame.h - std::max<Int>( 2, bz / 2 ) - 1;
			for( int i = 1; i < 8; ++i )
			{
				const Int tx = frame.x + frame.w * i / 8;
				TheDisplay->drawLine( tx, ty, tx, ty + ( i == 4 ? 2 : 1 ), 1, lampLit ? c.warning : c.accentDim );
			}
		}
	}
	drawUtilityKey( s_hudWin[ 1 ], GLYPH_OPTIONS, c );
	drawUtilityKey( s_hudWin[ 2 ], GLYPH_WORKER, c );
	drawUtilityKey( s_hudWin[ 3 ], GLYPH_BEACON, c );
	drawUtilityKey( s_hudWin[ 4 ], GLYPH_COMMS, c );
	{
		bool lit = false;
		drawAttackLamp( s_hudWin[ 6 ], c, &lit );
	}

	// 4. UNIT INFORMATION MODULE: the retail selection block is mounted in one equipment well with corner brackets
	{
		EnhancedHudLayout::Rect dst;
		bool hide = false;
		if( EnhancedHudLayout::destinationFor( "WinUnitSelected", dst, hide ) )
		{
			const HRect well = inflate( EnhancedHudLayout::toPhysical( l, dst ), g / 2 + 1 );
			drawRecess( well, c );
			if( th.cornerBrackets )
			{
				const Int len = P( 6 );
				const Color k = c.accentDim;
				const Int x1 = well.x + well.w - 1, y1 = well.y + well.h - 1;
				TheDisplay->drawLine( well.x, well.y, well.x + len, well.y, 1, k );     TheDisplay->drawLine( well.x, well.y, well.x, well.y + len, 1, k );
				TheDisplay->drawLine( x1, well.y, x1 - len, well.y, 1, k );             TheDisplay->drawLine( x1, well.y, x1, well.y + len, 1, k );
				TheDisplay->drawLine( well.x, y1, well.x + len, y1, 1, k );             TheDisplay->drawLine( well.x, y1, well.x, y1 - len, 1, k );
				TheDisplay->drawLine( x1, y1, x1 - len, y1, 1, k );                     TheDisplay->drawLine( x1, y1, x1, y1 - len, 1, k );
			}
		}
	}

	// 5. COMMAND MODULE: resource readout and power gauge share a header strip; below it the command bay: one recessed keypad
	//    with twelve fixed cells (retail slot indices). Empty cells are subdued wells, occupied cells are mounted machined
	//    plates with a state rail; hover = blue rim, pressed = depressed, disabled = muted.
	{
		const Int cg = std::max<Int>( 2, g / 2 );
		const HRect strip = { l.cashFrame.x - cg, l.cashFrame.y - cg,
		                      l.commandFrame.right() - ( l.cashFrame.x - l.commandFrame.x ) - ( l.cashFrame.x - cg ) + cg,
		                      l.cashFrame.h + 2 * cg };
		drawRecess( strip, c );
		if( th.tickMarks )
		{
			// header rail ticks under the power gauge side of the strip
			for( int i = 0; i < 12; ++i )
			{
				const Int tx = l.cashFrame.right() + P( 8 ) + i * ( strip.right() - l.cashFrame.right() - P( 16 ) ) / 12;
				TheDisplay->drawLine( tx, strip.y + strip.h - cg - 1, tx, strip.y + strip.h - 2, 1, i % 4 == 0 ? c.accent : c.accentDim );
			}
		}
		if( !l.cashFrame.empty() )
		{
			// compact military readout: dark inset, blue status edge, three status segments on the right
			drawRecess( l.cashFrame, c, P( 1.5 ) );
			TheDisplay->drawOpenRect( l.cashFrame.x, l.cashFrame.y, l.cashFrame.w, l.cashFrame.h, 1, c.outline );
			TheDisplay->drawFillRect( l.cashFrame.x + 1, l.cashFrame.y + 1, std::max<Int>( 2, t ), l.cashFrame.h - 2, c.accent );
			const Int segW = std::max<Int>( 2, P( 3 ) ), segH = std::max<Int>( 2, l.cashFrame.h / 5 );
			for( int i = 0; i < 3; ++i )
				TheDisplay->drawFillRect( l.cashFrame.right() - P( 4 ) - ( 3 - i ) * ( segW + 1 ), l.cashFrame.y + l.cashFrame.h / 2 - segH / 2, segW, segH, i < 2 ? c.ready : c.accentDim );
		}

		HRect cells[ 12 ];
		HRect bay = { 0, 0, 0, 0 };
		for( int i = 0; i < 12; ++i )
		{
			EnhancedHudLayout::Rect dst;
			bool hide = false;
			EnhancedHudLayout::destinationFor( s_hudWinNames[ 8 + i ], dst, hide );
			cells[ i ] = EnhancedHudLayout::toPhysical( l, dst );
			bay = ( i == 0 ) ? cells[ i ] : EnhancedHudWindows::unionOf( bay, cells[ i ] );
		}
		const HRect bayRect = inflate( bay, g );
		drawBevelPlate( inflate( bayRect, P( 1.5 ) ), P( 2.5 ), CM_ALL, c.panelLow, c.outline, false, c );
		drawRecess( bayRect, c, P( 2 ) );
		const bool commandsShown = s_hudWin[ 7 ] && windowChainShown( s_hudWin[ 7 ] );
		if( commandsShown )
		{
			const Int ig = std::max<Int>( 1, g / 2 );
			const Int k = th.plateCorner == EnhancedHudTheme::Corner::Chamfer ? P( th.plateCornerRef ) : 0;
			for( int i = 0; i < 12; ++i )
			{
				GameWindow *btn = s_hudWin[ 8 + i ];
				const bool occupied = btn && !btn->winIsHidden();
				const HRect cell = inflate( cells[ i ], ig );
				if( !occupied )
				{
					// subdued empty cell: shallow well with corner ticks only
					TheDisplay->drawFillRect( cell.x, cell.y, cell.w, cell.h, c.panelLow );
					TheDisplay->drawOpenRect( cell.x, cell.y, cell.w, cell.h, 1, c.steelMid );
					if( th.tickMarks )
					{
						const Int len = P( 3 );
						TheDisplay->drawLine( cell.x, cell.y, cell.x + len, cell.y, 1, c.wellEdge );
						TheDisplay->drawLine( cell.x, cell.y, cell.x, cell.y + len, 1, c.wellEdge );
						TheDisplay->drawLine( cell.x + cell.w - 1, cell.y + cell.h - 1, cell.x + cell.w - 1 - len, cell.y + cell.h - 1, 1, c.wellEdge );
						TheDisplay->drawLine( cell.x + cell.w - 1, cell.y + cell.h - 1, cell.x + cell.w - 1, cell.y + cell.h - 1 - len, 1, c.wellEdge );
					}
					continue;
				}
				const bool enabled = BitIsSet( btn->winGetStatus(), WIN_STATUS_ENABLED ) ? true : false;
				bool hover, down;
				pointerState( cell, &hover, &down );
				drawBevelPlate( cell, k, CM_ALL, enabled ? c.panel : c.panelLow, enabled ? ( hover ? c.accent : c.outline ) : c.steelMid, down && enabled, c );
				if( th.stateRails )
					TheDisplay->drawFillRect( cell.x + k + 1, cell.y + cell.h - 3, cell.w - 2 * k - 2, std::max<Int>( 1, P( 1 ) ), enabled ? ( hover ? c.ready : c.accentDim ) : c.shadow );
			}
		}
	}

	// 6. GENERAL MODULE: one compact officer/status unit - XP gauge with instrument ticks, the General Powers entry in its
	//    own well, and the rank grid (earned ranks in restrained gold on a mounted plate, unearned ones recede).
	if( GameWindow *bar = s_hudWin[ 5 ] )
	{
		const HRect r = windowRect( bar );
		drawRecess( r, c, P( 1.5 ) );
		if( th.tickMarks )
		{
			for( int k = 1; k < 10; ++k )
			{
				const Int y = r.y + r.h - k * r.h / 10;
				const bool major = k % 5 == 0;
				TheDisplay->drawLine( r.x + r.w - ( major ? r.w / 3 : r.w / 5 ), y, r.x + r.w - 1, y, 1, major ? c.accent : c.accentDim );
			}
		}
	}
	{
		EnhancedHudLayout::Rect dst;
		bool hide = false;
		if( EnhancedHudLayout::destinationFor( "ButtonGeneral", dst, hide ) )
		{
			const HRect gr = inflate( EnhancedHudLayout::toPhysical( l, dst ), std::max<Int>( 1, g / 2 ) );
			drawRecess( gr, c );
		}

		Int rank = 1, levels = 1;
		if( TheControlBar )
			if( Player *player = TheControlBar->getCurrentlyViewedPlayer() )
				rank = player->getRankLevel();
		if( TheRankInfoStore )
			levels = TheRankInfoStore->getRankLevelCount();
		const int slots = EnhancedHudLayout::rankSlotCount( levels );
		const Int k = th.plateCorner == EnhancedHudTheme::Corner::Chamfer ? P( th.plateCornerRef ) : 0;
		for( int i = 0; i < slots; ++i )
		{
			const bool reached = rank >= i + 2;
			const HRect slot = EnhancedHudLayout::toPhysical( l, EnhancedHudLayout::rankSlot( i ) );
			const HRect star = EnhancedHudLayout::toPhysical( l, EnhancedHudLayout::rankStar( i ) );
			if( reached )
			{
				drawBevelPlate( slot, k, CM_ALL, c.panelLow, c.lit, false, c );
				if( th.stateRails )
					TheDisplay->drawFillRect( slot.x + k + 1, slot.y + slot.h - 3, slot.w - 2 * k - 2, std::max<Int>( 1, P( 1 ) ), c.lit );
			}
			else
				drawRecess( slot, c, P( 1 ) );
			fillStar( star.x + star.w / 2, star.y + star.h / 2, std::max( 2, star.h / 2 ), reached ? c.lit : c.litDim );
		}
	}
	// (the Global Powers surface is not part of the island: see powersPanelDraw, drawn by the shortcut window itself)
}
