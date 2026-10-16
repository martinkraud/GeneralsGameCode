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
#include <map>
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

	applyEnhancedPowerShortcutLayout();
}

//-----------------------------------------------------------------------------
void ControlBar::restoreOriginalLayout()
{
	// A strict no-op unless the Enhanced layout was applied: the Original presentation is never touched.
	if( s_barMaster == nullptr && s_powersParent == nullptr )
		return;
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
		restoreStore( s_powers );
	else
		s_powers.clear();
	s_barMaster = nullptr;
	s_powersParent = nullptr;
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
	}
	const EnhancedHudLayout::Result &layout = EnhancedHud::layout();
	const EnhancedHudLayout::Rect column = layout.powerColumn;
	// the column is scaled uniformly: its children keep their relationships (original sizes were created at the
	// modern layout scale; the island scale may be smaller on narrow surfaces)
	const double layoutScale = DisplayRuntime::uiScale() > 0.0 ? DisplayRuntime::uiScale() : 1.0;
	const double factor = layout.scale / layoutScale;
	EnhancedHudWindows::applyPowerColumn<GameWindow>( parent, column, factor,
		[]( GameWindow *win, int *out )
		{
			remember( s_powers, win );
			const Original &o = s_powers[ win ];
			out[ 0 ] = o.pos.x; out[ 1 ] = o.pos.y; out[ 2 ] = o.size.x; out[ 3 ] = o.size.y;
		},
		[]( GameWindow *win ) { remember( s_powers, win ); } );
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
	Color steelDark, steel, panel, panelLow, accent, accentDim, highlight, shadow, icon, iconDim, lit, litDim;
};

HudPalette makePalette()
{
	HudPalette c;
	c.steelDark = GameMakeColor( 14, 18, 26, 255 );
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
GameWindow *s_hudWin[ 6 ];     // radar, options, idle worker, beacon, communicator, XP bar foreground
const char *const s_hudWinNames[ 6 ] = { "LeftHUD", "ButtonOptions", "ButtonIdleWorker", "ButtonPlaceBeacon",
                                         "PopupCommunicator", "ExpBarForeground" };

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

}  // namespace

void drawEnhancedHudFrame()
{
	if( !EnhancedHud::active() || !TheDisplay )
		return;
	const EnhancedHudLayout::Result &l = EnhancedHud::layout();
	const HudPalette c = makePalette();
	const Int t = std::max( 1, (Int)( l.scale * 1.5 + 0.5 ) );     // line thickness follows the UI scale

	// base plate (the island), then the four panels inset from it
	TheDisplay->drawFillRect( l.island.x, l.island.y, l.island.w, l.island.h, c.steelDark );
	const HRect panels[ 4 ] = { l.radarFrame, l.infoFrame, l.commandFrame, l.xpFrame };
	for( int i = 0; i < 4; ++i )
	{
		const HRect &p = panels[ i ];
		TheDisplay->drawFillRect( p.x, p.y, p.w, p.h, c.steel );
		TheDisplay->drawFillRect( p.x + t, p.y + t, p.w - 2 * t, p.h - 2 * t, c.panel );
		TheDisplay->drawLine( p.x + t, p.y + t, p.x + p.w - t, p.y + t, 1, c.highlight );      // top bevel
		TheDisplay->drawLine( p.x + t, p.y + p.h - t, p.x + p.w - t, p.y + p.h - t, 1, c.shadow );
		TheDisplay->drawOpenRect( p.x, p.y, p.w, p.h, 1, c.accentDim );
	}
	// dividers
	for( int i = 0; i < 3; ++i )
	{
		TheDisplay->drawFillRect( l.dividerX[ i ] - t, l.island.y + t, 2 * t, l.island.h - t, c.shadow );
		TheDisplay->drawLine( l.dividerX[ i ], l.island.y + t, l.dividerX[ i ], l.island.bottom(), 1, c.highlight );
	}
	// top accent edge and the two end caps
	TheDisplay->drawFillRect( l.island.x, l.island.y, l.island.w, t, c.accent );
	const Int cap = std::max( l.island.h / 3, 4 );
	TheDisplay->drawFillRect( l.island.x, l.island.y, t, cap, c.accent );
	TheDisplay->drawFillRect( l.island.right() - t, l.island.y, t, cap, c.accent );

	// windows used for the control plates (looked up once per control bar)
	if( s_barMaster != s_hudMasterForLookup )
	{
		s_hudMasterForLookup = s_barMaster;
		for( int i = 0; i < 6; ++i )
			s_hudWin[ i ] = s_barMaster ? barWindow( s_hudWinNames[ i ] ) : nullptr;
	}

	// radar bezel: a dark recess around the (retail, functional) radar window
	if( GameWindow *radar = s_hudWin[ 0 ] )
	{
		Int x = 0, y = 0, w = 0, h = 0;
		radar->winGetScreenPosition( &x, &y );
		radar->winGetSize( &w, &h );
		const Int g = std::max( 2, t + 1 );
		drawPlate( HRect{ x - g, y - g, w + 2 * g, h + 2 * g }, 1, c.shadow, c.accentDim, c );
	}
	// utility buttons: procedural plates (the retail bitmaps are cleared for Enhanced)
	drawUtilityPlate( s_hudWin[ 1 ], GLYPH_OPTIONS, 1, c );
	drawUtilityPlate( s_hudWin[ 2 ], GLYPH_WORKER, 1, c );
	drawUtilityPlate( s_hudWin[ 3 ], GLYPH_BEACON, 1, c );
	drawUtilityPlate( s_hudWin[ 4 ], GLYPH_COMMS, 1, c );

	// cash panel: its own recessed display with an accent edge (the existing MoneyDisplay text sits inside)
	if( !l.cashFrame.empty() )
	{
		drawPlate( l.cashFrame, 1, c.shadow, c.accentDim, c );
		TheDisplay->drawFillRect( l.cashFrame.x + 1, l.cashFrame.y + 1, std::max( 2, t ), l.cashFrame.h - 2, c.accent );
	}

	// General / XP region: trough behind the XP bar and the rank ladder (the stars of the retail strip are replaced)
	if( GameWindow *bar = s_hudWin[ 5 ] )
	{
		Int x = 0, y = 0, w = 0, h = 0;
		bar->winGetScreenPosition( &x, &y );
		bar->winGetSize( &w, &h );
		drawPlate( HRect{ x, y, w, h }, 1, c.shadow, c.accentDim, c );
	}
	{
		Int rank = 1, levels = 1;
		if( TheControlBar )
			if( Player *player = TheControlBar->getCurrentlyViewedPlayer() )
				rank = player->getRankLevel();
		if( TheRankInfoStore )
			levels = TheRankInfoStore->getRankLevelCount();
		const int slots = EnhancedHudLayout::rankSlotCount( levels );
		const double s = l.island.w > 0 ? (double)l.island.w / EnhancedHudLayout::kIslandW : l.scale;
		for( int i = 0; i < slots; ++i )
		{
			const bool reached = rank >= i + 2;
			const HRect slot = EnhancedHudLayout::toPhysical( l, EnhancedHudLayout::rankSlot( i ) );
			const HRect star = EnhancedHudLayout::toPhysical( l, EnhancedHudLayout::rankStar( i ) );
			drawPlate( slot, 1, reached ? c.panelLow : c.steelDark, reached ? c.accent : c.accentDim, c );
			fillStar( star.x + star.w / 2, star.y + star.h / 2, std::max( 2, star.h / 2 ), reached ? c.lit : c.litDim );
			if( reached )
				TheDisplay->drawFillRect( star.right() + (Int)( 4 * s ), slot.y + slot.h / 2 - 1,
				                          slot.right() - star.right() - (Int)( 8 * s ), std::max( 2, (Int)( 2 * s ) ), c.accent );
		}
	}

	// special power column: part of the console (same plate, joined to the island edge and its accent line)
	if( !l.powerColumn.empty() )
	{
		const HRect &pc = l.powerColumn;
		TheDisplay->drawFillRect( pc.x, pc.y, pc.w, pc.h, c.steelDark );
		TheDisplay->drawFillRect( pc.x + t, pc.y + t, pc.w - 2 * t, pc.h - 2 * t, c.steel );
		TheDisplay->drawOpenRect( pc.x, pc.y, pc.w, pc.h, 1, c.accentDim );
		TheDisplay->drawFillRect( pc.x, pc.y, pc.w, t, c.accent );
		if( l.powerBesideIsland )
		{
			// the seam with the island: carry the base plate across so the column reads as part of the console
			TheDisplay->drawFillRect( pc.x - t, pc.bottom() - l.island.h + t, 2 * t, l.island.h - t, c.steel );
			TheDisplay->drawLine( pc.x, pc.bottom() - l.island.h + t, pc.x, pc.bottom(), 1, c.highlight );
		}
		else
			TheDisplay->drawFillRect( pc.x, pc.bottom() - t, pc.w, 2 * t, c.steel );
	}
}
