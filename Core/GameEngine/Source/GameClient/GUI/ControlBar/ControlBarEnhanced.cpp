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
#include "Common/NameKeyGenerator.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Display.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/Color.h"

#include <algorithm>
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

// Moves/sizes a window so that its screen rectangle is `rect` (positions are parent relative).
void placeAbsolute( GameWindow *win, const EnhancedHudLayout::Rect &rect )
{
	if( !win )
		return;
	Int px = 0, py = 0;
	if( GameWindow *parent = win->winGetParent() )
		parent->winGetScreenPosition( &px, &py );
	win->winSetPosition( rect.x - px, rect.y - py );
	win->winSetSize( rect.w, rect.h );
}

// Scales a window subtree around its parent origin by `factor`, from the remembered originals (idempotent).
void scaleSubtree( std::map<GameWindow *, Original> &store, GameWindow *win, double factor )
{
	for( GameWindow *child = win->winGetChild(); child; child = child->winGetNext() )
	{
		remember( store, child );
		const Original &o = store[ child ];
		child->winSetPosition( (Int)(o.pos.x * factor + 0.5), (Int)(o.pos.y * factor + 0.5) );
		child->winSetSize( (Int)(o.size.x * factor + 0.5), (Int)(o.size.y * factor + 0.5) );
		scaleSubtree( store, child, factor );
	}
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

	// 1. the master window becomes the island (bottom flush, centred, bounded); everything else is its descendant
	remember( s_bar, master );
	master->winSetPosition( layout.island.x, layout.island.y );
	master->winSetSize( layout.island.w, layout.island.h );
	m_defaultControlBarPosition.x = layout.island.x;
	m_defaultControlBarPosition.y = layout.island.y;

	// 2. windows with an explicit Enhanced placement
	int count = 0;
	const EnhancedHudLayout::NamedPlacement *table = EnhancedHudLayout::placements( count );
	for( int i = 0; i < count; ++i )
	{
		GameWindow *win = barWindow( table[ i ].window );
		if( !win )
			continue;
		remember( s_bar, win );
		if( table[ i ].hide )
		{
			win->winHide( TRUE );
			continue;
		}
		placeAbsolute( win, EnhancedHudLayout::toPhysical( layout, table[ i ].dst ) );
	}

	// 3. the info group and the command group (translations / the 1.10 x grid), containers before children
	auto placeGroup = [&]( const EnhancedHudLayout::GroupWindow *group, int groupCount )
	{
		for( int i = 0; i < groupCount; ++i )
		{
			EnhancedHudLayout::Rect dst;
			Bool hide = FALSE;
			if( !EnhancedHudLayout::destinationFor( group[ i ].window, dst, hide ) )
				continue;
			GameWindow *win = barWindow( group[ i ].window );
			if( !win )
				continue;
			remember( s_bar, win );
			placeAbsolute( win, EnhancedHudLayout::toPhysical( layout, dst ) );
		}
	};
	int groupCount = 0;
	const EnhancedHudLayout::GroupWindow *info = EnhancedHudLayout::infoGroup( groupCount );
	placeGroup( info, groupCount );
	const EnhancedHudLayout::GroupWindow *command = EnhancedHudLayout::commandGroup( groupCount );
	placeGroup( command, groupCount );

	// 4. decorative retail art windows have no Enhanced counterpart: hide the childless ones so they can neither draw
	//    nor take clicks outside the island (they were authored for the 800 px band)
	for( GameWindow *child = master->winGetChild(); child; child = child->winGetNext() )
	{
		if( child->winGetChild() != nullptr || child->winIsHidden() )
			continue;
		const Bool art = child->winGetWindowId() == TheNameKeyGenerator->nameToKey( "ControlBar.wnd:Munkee" ) ||
		                 child->winGetWindowId() == TheNameKeyGenerator->nameToKey( "ControlBar.wnd:CenterBackground" ) ||
		                 child->winGetWindowId() == TheNameKeyGenerator->nameToKey( "ControlBar.wnd:" );
		if( art )
		{
			remember( s_bar, child );
			child->winHide( TRUE );
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
	remember( s_powers, parent );
	const Original &o = s_powers[ parent ];
	parent->winSetPosition( layout.powerColumn.x, layout.powerColumn.y );
	// the column is scaled uniformly: its children keep their relationships (original sizes were created at the
	// modern layout scale; the island scale may be smaller on narrow surfaces)
	const double layoutScale = DisplayRuntime::uiScale() > 0.0 ? DisplayRuntime::uiScale() : 1.0;
	const double factor = layout.scale / layoutScale;
	parent->winSetSize( (Int)(o.size.x * factor + 0.5), (Int)(o.size.y * factor + 0.5) );
	scaleSubtree( s_powers, parent, factor );
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
// The modular procedural frame (replaces the scheme art while Enhanced is active): left cap, radar frame, info frame,
// command frame, XP/right cap, dividers. Colours are USA: dark steel with restrained blue accents.
void drawEnhancedHudFrame()
{
	if( !EnhancedHud::active() || !TheDisplay )
		return;
	const EnhancedHudLayout::Result &l = EnhancedHud::layout();
	const Int t = std::max( 1, (Int)( l.scale * 1.5 + 0.5 ) );     // line thickness follows the UI scale
	const Color steelDark = GameMakeColor( 14, 18, 26, 255 );
	const Color steel = GameMakeColor( 26, 32, 44, 255 );
	const Color panel = GameMakeColor( 34, 42, 58, 255 );
	const Color accent = GameMakeColor( 64, 112, 196, 255 );
	const Color accentDim = GameMakeColor( 38, 66, 120, 255 );
	const Color highlight = GameMakeColor( 92, 112, 148, 255 );
	const Color shadow = GameMakeColor( 6, 8, 12, 255 );

	// base plate (the island), then the four panels inset from it
	TheDisplay->drawFillRect( l.island.x, l.island.y, l.island.w, l.island.h, steelDark );
	const EnhancedHudLayout::Rect panels[ 4 ] = { l.radarFrame, l.infoFrame, l.commandFrame, l.xpFrame };
	for( int i = 0; i < 4; ++i )
	{
		const EnhancedHudLayout::Rect &p = panels[ i ];
		TheDisplay->drawFillRect( p.x, p.y, p.w, p.h, steel );
		TheDisplay->drawFillRect( p.x + t, p.y + t, p.w - 2 * t, p.h - 2 * t, panel );
		TheDisplay->drawLine( p.x + t, p.y + t, p.x + p.w - t, p.y + t, 1, highlight );      // top bevel
		TheDisplay->drawLine( p.x + t, p.y + p.h - t, p.x + p.w - t, p.y + p.h - t, 1, shadow );
		TheDisplay->drawOpenRect( p.x, p.y, p.w, p.h, 1, accentDim );
	}
	// dividers
	for( int i = 0; i < 3; ++i )
	{
		TheDisplay->drawFillRect( l.dividerX[ i ] - t, l.island.y + t, 2 * t, l.island.h - t, shadow );
		TheDisplay->drawLine( l.dividerX[ i ], l.island.y + t, l.dividerX[ i ], l.island.bottom(), 1, highlight );
	}
	// top accent edge and the two end caps
	TheDisplay->drawFillRect( l.island.x, l.island.y, l.island.w, t, accent );
	const Int cap = std::max( l.island.h / 3, 4 );
	TheDisplay->drawFillRect( l.island.x, l.island.y, t, cap, accent );
	TheDisplay->drawFillRect( l.island.right() - t, l.island.y, t, cap, accent );
	// special power column backing (side buttons)
	if( !l.powerColumn.empty() )
	{
		TheDisplay->drawFillRect( l.powerColumn.x, l.powerColumn.y, l.powerColumn.w, l.powerColumn.h, steelDark );
		TheDisplay->drawOpenRect( l.powerColumn.x, l.powerColumn.y, l.powerColumn.w, l.powerColumn.h, 1, accentDim );
		TheDisplay->drawFillRect( l.powerColumn.x, l.powerColumn.y, l.powerColumn.w, t, accent );
	}
}
