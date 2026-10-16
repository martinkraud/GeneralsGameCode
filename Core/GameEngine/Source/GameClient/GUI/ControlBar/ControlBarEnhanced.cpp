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
// The modular procedural frame (replaces the scheme art while Enhanced is active): left cap, radar bezel with its
// utility plates, info frame, command console, cash panel, General/XP region with its rank ladder, dividers and the
// special-power column joined to the island. Colours are USA: dark steel with restrained blue accents.
namespace
{

typedef EnhancedHudLayout::Rect HRect;

struct HudPalette
{
	Color steelDark, steelMid, steel, panel, panelLow, well, wellEdge, accent, accentDim, highlight, shadow, icon, iconDim, lit, litDim;
};

HudPalette makePalette()
{
	HudPalette c;
	c.steelDark = GameMakeColor( 14, 18, 26, 255 );
	c.steelMid = GameMakeColor( 20, 25, 36, 255 );
	c.well = GameMakeColor( 11, 15, 22, 255 );
	c.wellEdge = GameMakeColor( 36, 46, 66, 255 );
	c.steel = GameMakeColor( 26, 32, 44, 255 );
	c.panel = GameMakeColor( 34, 42, 58, 255 );
	c.panelLow = GameMakeColor( 22, 28, 40, 255 );
	c.accent = GameMakeColor( 64, 112, 196, 255 );
	c.accentDim = GameMakeColor( 38, 66, 120, 255 );
	c.highlight = GameMakeColor( 92, 112, 148, 255 );
	c.shadow = GameMakeColor( 6, 8, 12, 255 );
	c.icon = GameMakeColor( 196, 212, 238, 255 );
	c.iconDim = GameMakeColor( 92, 104, 124, 255 );
	c.lit = GameMakeColor( 232, 196, 84, 255 );
	c.litDim = GameMakeColor( 52, 58, 72, 255 );
	return c;
}

// A bevelled inset plate: outline, body, top highlight, bottom shadow.
void drawPlate( const HRect &r, Int t, Color body, Color outline, const HudPalette &c )
{
	if( r.w <= 2 * t || r.h <= 2 * t )
		return;
	TheDisplay->drawFillRect( r.x, r.y, r.w, r.h, outline );
	TheDisplay->drawFillRect( r.x + t, r.y + t, r.w - 2 * t, r.h - 2 * t, body );
	TheDisplay->drawLine( r.x + t, r.y + t, r.x + r.w - t, r.y + t, 1, c.highlight );
	TheDisplay->drawLine( r.x + t, r.y + r.h - t, r.x + r.w - t, r.y + r.h - t, 1, c.shadow );
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

GameWindow *s_hudMasterForLookup = nullptr;
const int kHudWinCount = 8 + 12;
GameWindow *s_hudWin[ kHudWinCount ];     // 0 radar, 1-4 utility buttons, 5 XP trough, 6 attack lamp, 7 CommandWindow, 8.. command buttons
const char *const s_hudWinNames[ kHudWinCount ] = { "LeftHUD", "ButtonOptions", "ButtonIdleWorker", "ButtonPlaceBeacon",
                                         "PopupCommunicator", "ExpBarForeground", "WinUAttack", "CommandWindow",
                                         "ButtonCommand01", "ButtonCommand02", "ButtonCommand03", "ButtonCommand04",
                                         "ButtonCommand05", "ButtonCommand06", "ButtonCommand07", "ButtonCommand08",
                                         "ButtonCommand09", "ButtonCommand10", "ButtonCommand11", "ButtonCommand12" };

// The "under attack" warning lamp. The retail window still flashes by toggling its enable state (disabled = lit); this only
// replaces its 22 x 24 px bitmap with a strong, clean lamp: a solid red plate with a white warning triangle when lit, a
// dark idle lamp otherwise.
bool drawAttackLamp( GameWindow *win, const HudPalette &c, bool *lit )
{
	*lit = false;
	if( !win || win->winIsHidden() )
		return false;
	Int x = 0, y = 0, w = 0, h = 0;
	win->winGetScreenPosition( &x, &y );
	win->winGetSize( &w, &h );
	const bool on = !BitIsSet( win->winGetStatus(), WIN_STATUS_ENABLED );
	*lit = on;
	const Color red = GameMakeColor( 224, 24, 24, 255 );
	const Color redDark = GameMakeColor( 70, 14, 14, 255 );
	const Color white = GameMakeColor( 255, 255, 255, 255 );
	const HRect r = { x, y, w, h };
	drawPlate( r, 1, on ? red : redDark, on ? white : c.accentDim, c );
	// warning triangle with an exclamation mark
	const Int cx = x + w / 2, top = y + std::max( 2, h / 6 ), bottom = y + h - std::max( 2, h / 6 );
	const Int half = std::max( 3, ( bottom - top ) * 6 / 10 );
	const Color tri = on ? white : GameMakeColor( 120, 40, 40, 255 );
	for( Int row = top; row <= bottom; ++row )
	{
		const Int span = half * ( row - top ) / std::max( 1, bottom - top );
		TheDisplay->drawLine( cx - span, row, cx + span, row, 1, tri );
	}
	const Color mark = on ? red : redDark;
	const Int mh = std::max( 2, ( bottom - top ) / 3 );
	TheDisplay->drawFillRect( cx - 1, top + ( bottom - top ) / 3, std::max( 2, w / 14 ), mh, mark );
	TheDisplay->drawFillRect( cx - 1, bottom - std::max( 2, ( bottom - top ) / 6 ) - 1, std::max( 2, w / 14 ), std::max( 2, ( bottom - top ) / 8 ), mark );
	return on;
}

void drawUtilityPlate( GameWindow *win, Glyph glyph, Int t, const HudPalette &c )
{
	if( !win || win->winIsHidden() )
		return;
	Int x = 0, y = 0, w = 0, h = 0;
	win->winGetScreenPosition( &x, &y );
	win->winGetSize( &w, &h );
	const HRect r = { x, y, w, h };
	bool hover = false, down = false;
	if( TheMouse )
	{
		const MouseIO *io = TheMouse->getMouseStatus();
		hover = io->pos.x >= x && io->pos.x <= x + w && io->pos.y >= y && io->pos.y <= y + h;
		down = hover && io->leftState == MBS_Down;
	}
	const bool enabled = BitIsSet( win->winGetStatus(), WIN_STATUS_ENABLED ) ? true : false;
	const Color body = down ? c.panelLow : ( hover ? c.highlight : c.panel );
	drawPlate( r, t, enabled ? body : c.steel, enabled ? c.accentDim : c.shadow, c );
	if( hover && enabled )
		TheDisplay->drawOpenRect( r.x, r.y, r.w, r.h, 1, c.accent );
	drawGlyph( glyph, r, enabled ? c.icon : c.iconDim );
}

// Recessed area (a well): dark body, a shadow line on top and a faint lit lip at the bottom.
void drawWell( const HRect &r, const HudPalette &c )
{
	if( r.w < 3 || r.h < 3 )
		return;
	TheDisplay->drawFillRect( r.x, r.y, r.w, r.h, c.well );
	TheDisplay->drawLine( r.x, r.y, r.x + r.w, r.y, 1, c.shadow );
	TheDisplay->drawLine( r.x, r.y + r.h, r.x + r.w, r.y + r.h, 1, c.wellEdge );
}

// A module of the console: panel body, one hairline outline, a top highlight and a short accent cap.
void drawModule( const HRect &r, Int t, const HudPalette &c )
{
	if( r.w < 4 || r.h < 4 )
		return;
	TheDisplay->drawFillRect( r.x, r.y, r.w, r.h, c.panel );
	TheDisplay->drawOpenRect( r.x, r.y, r.w, r.h, 1, c.accentDim );
	TheDisplay->drawLine( r.x + 1, r.y + 1, r.x + r.w - 1, r.y + 1, 1, c.highlight );
	TheDisplay->drawFillRect( r.x + t, r.y, std::max( 8, r.w / 7 ), std::max( 2, t ), c.accent );
}

bool windowChainShown( GameWindow *win )
{
	for( ; win; win = win->winGetParent() )
		if( win->winIsHidden() )
			return false;
	return win == nullptr;
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

// The Global Powers surface. It is the draw function of the (independent, top-level) shortcut parent window while Enhanced
// is active, so its frame follows that window exactly: position, slide-in animation, hidden state. The five slots are the
// existing ButtonParent/ButtonCommand windows; ready (enabled) slots get an accent outline, cooldown stays the button's own.
void powersPanelDraw( GameWindow *window, WinInstanceData * )
{
	if( !EnhancedHud::active() || !TheDisplay || !window )
		return;
	const EnhancedHudLayout::Result &l = EnhancedHud::layout();
	const HudPalette c = makePalette();
	const Int t = std::max( 1, (Int)( l.scale * 1.5 + 0.5 ) );
	const Int pad = std::max( 2, (Int)( EnhancedHudLayout::kPowerPad * l.scale + 0.5 ) );
	const HRect frame = inflate( windowRect( window ), pad );
	TheDisplay->drawFillRect( frame.x, frame.y, frame.w, frame.h, c.steelDark );
	drawModule( inflate( frame, -1 ), t, c );
	const Int g = std::max( 1, (Int)( 2 * l.scale + 0.5 ) );
	for( GameWindow *slot = window->winGetChild(); slot; slot = slot->winGetNext() )
	{
		if( slot->winIsHidden() )
			continue;
		const HRect cell = inflate( windowRect( slot ), -g );
		drawWell( cell, c );
		bool ready = false;
		for( GameWindow *button = slot->winGetChild(); button; button = button->winGetNext() )
			if( !button->winIsHidden() && BitIsSet( button->winGetStatus(), WIN_STATUS_ENABLED ) )
				ready = true;
		TheDisplay->drawOpenRect( cell.x, cell.y, cell.w, cell.h, 1, ready ? c.accent : c.accentDim );
	}
}

}  // namespace

void drawEnhancedHudFrame()
{
	if( !EnhancedHud::active() || !TheDisplay )
		return;
	EnhancedHudLayout::Result l = EnhancedHud::layout();
	const HudPalette c = makePalette();
	const Int t = std::max( 1, (Int)( l.scale * 1.5 + 0.5 ) );     // line thickness follows the UI scale
	const Int g = std::max( 2, (Int)( 4 * l.scale + 0.5 ) );       // gutter between a well and what it holds

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

	// 1. outer console: one dark plate with a lighter upper band and a single accent edge
	TheDisplay->drawFillRect( l.island.x, l.island.y, l.island.w, l.island.h, c.steelDark );
	TheDisplay->drawFillRect( l.island.x, l.island.y + t, l.island.w, l.island.h / 3, c.steelMid );
	TheDisplay->drawFillRect( l.island.x, l.island.y, l.island.w, t, c.accent );

	// 2. the four modules (no dividers: the gaps between modules are the dividers)
	drawModule( l.radarFrame, t, c );
	drawModule( l.infoFrame, t, c );
	drawModule( l.commandFrame, t, c );
	drawModule( l.xpFrame, t, c );

	// 3. radar module: bezel (red while the attack warning is lit), utility plates, warning lamp
	const bool lampLit = s_hudWin[ 6 ] && !s_hudWin[ 6 ]->winIsHidden() && !BitIsSet( s_hudWin[ 6 ]->winGetStatus(), WIN_STATUS_ENABLED );
	if( GameWindow *radar = s_hudWin[ 0 ] )
	{
		const HRect bezel = inflate( windowRect( radar ), std::max( 2, t + 1 ) );
		const Color red = GameMakeColor( 224, 24, 24, 255 );
		drawWell( bezel, c );
		TheDisplay->drawOpenRect( bezel.x, bezel.y, bezel.w, bezel.h, 1, lampLit ? red : c.accentDim );
		if( lampLit )
			TheDisplay->drawOpenRect( bezel.x - 1, bezel.y - 1, bezel.w + 2, bezel.h + 2, 1, red );
	}
	drawUtilityPlate( s_hudWin[ 1 ], GLYPH_OPTIONS, 1, c );
	drawUtilityPlate( s_hudWin[ 2 ], GLYPH_WORKER, 1, c );
	drawUtilityPlate( s_hudWin[ 3 ], GLYPH_BEACON, 1, c );
	drawUtilityPlate( s_hudWin[ 4 ], GLYPH_COMMS, 1, c );
	{
		bool lit = false;
		drawAttackLamp( s_hudWin[ 6 ], c, &lit );
	}

	// 4. unit information module: the retail selection block sits in one recessed well
	{
		EnhancedHudLayout::Rect dst;
		bool hide = false;
		if( EnhancedHudLayout::destinationFor( "WinUnitSelected", dst, hide ) )
			drawWell( inflate( EnhancedHudLayout::toPhysical( l, dst ), g / 2 ), c );
	}

	// 5. command module: resource header (cash + power), then the command bay with its twelve fixed slots. The slots are
	//    always drawn, so a context with two commands reads as a bay with ten free slots rather than as icons floating on a
	//    blank plate; the commands keep their retail slot indices.
	{
		const Int cashGutter = std::max( 2, g / 2 );
		const HRect strip = { l.cashFrame.x - cashGutter, l.cashFrame.y - cashGutter,
		                      l.commandFrame.right() - ( l.cashFrame.x - l.commandFrame.x ) - ( l.cashFrame.x - cashGutter ) + cashGutter,
		                      l.cashFrame.h + 2 * cashGutter };
		drawWell( strip, c );
		if( !l.cashFrame.empty() )
		{
			drawPlate( l.cashFrame, 1, c.shadow, c.accentDim, c );
			TheDisplay->drawFillRect( l.cashFrame.x + 1, l.cashFrame.y + 1, std::max( 2, t ), l.cashFrame.h - 2, c.accent );
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
		drawWell( inflate( bay, g ), c );
		const bool commandsShown = s_hudWin[ 7 ] && windowChainShown( s_hudWin[ 7 ] );
		if( commandsShown )
		{
			const Int ig = std::max( 1, g / 2 );
			for( int i = 0; i < 12; ++i )
			{
				GameWindow *btn = s_hudWin[ 8 + i ];
				const bool occupied = btn && !btn->winIsHidden();
				const HRect slot = inflate( cells[ i ], ig );
				if( occupied )
					drawPlate( slot, 1, c.panel, c.accentDim, c );
				else
				{
					TheDisplay->drawFillRect( slot.x, slot.y, slot.w, slot.h, c.panelLow );
					TheDisplay->drawOpenRect( slot.x, slot.y, slot.w, slot.h, 1, c.steel );
				}
			}
		}
	}

	// 6. General module (one piece): XP trough with scale ticks, the General Powers entry plate and the rank grid
	if( GameWindow *bar = s_hudWin[ 5 ] )
	{
		const HRect r = windowRect( bar );
		drawWell( r, c );
		for( int k = 1; k < 5; ++k )
		{
			const Int y = r.y + r.h - k * r.h / 5;
			TheDisplay->drawLine( r.x + r.w - r.w / 4, y, r.x + r.w - 1, y, 1, c.accentDim );
		}
	}
	{
		EnhancedHudLayout::Rect dst;
		bool hide = false;
		if( EnhancedHudLayout::destinationFor( "ButtonGeneral", dst, hide ) )
			drawWell( inflate( EnhancedHudLayout::toPhysical( l, dst ), std::max( 1, g / 2 ) ), c );

		Int rank = 1, levels = 1;
		if( TheControlBar )
			if( Player *player = TheControlBar->getCurrentlyViewedPlayer() )
				rank = player->getRankLevel();
		if( TheRankInfoStore )
			levels = TheRankInfoStore->getRankLevelCount();
		const int slots = EnhancedHudLayout::rankSlotCount( levels );
		for( int i = 0; i < slots; ++i )
		{
			const bool reached = rank >= i + 2;
			const HRect slot = EnhancedHudLayout::toPhysical( l, EnhancedHudLayout::rankSlot( i ) );
			const HRect star = EnhancedHudLayout::toPhysical( l, EnhancedHudLayout::rankStar( i ) );
			if( reached )
				drawPlate( slot, 1, c.panelLow, c.accent, c );
			else
				drawWell( slot, c );
			fillStar( star.x + star.w / 2, star.y + star.h / 2, std::max( 2, star.h / 2 ), reached ? c.lit : c.litDim );
		}
	}
	// (the Global Powers surface is not part of the island: see powersPanelDraw, drawn by the shortcut window itself)
}
