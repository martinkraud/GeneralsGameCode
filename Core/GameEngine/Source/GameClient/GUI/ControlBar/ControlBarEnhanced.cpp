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
#include "Common/EnhancedHudPanels.h"
#include "Common/Energy.h"
#include "Common/Science.h"
#include "Common/GlobalData.h"
#include "GameClient/DisplayString.h"
#include "GameClient/DisplayStringManager.h"
#include "GameClient/EnhancedHudOverview.h"
#include "GameClient/GameFont.h"
#include "GameClient/GameText.h"
#include "GameLogic/GameLogic.h"
#include "GameNetwork/GameInfo.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

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
void purchasePanelDraw( GameWindow *window, WinInstanceData *instData );
void releaseTexts();
Color themeColor( const EnhancedHudTheme::Rgba &c );
GameWinDrawFunc s_powersOriginalDraw = nullptr;
GameWindow *s_purchaseWindow = nullptr;            // the General Powers popup (skinned while Enhanced is active)
GameWinDrawFunc s_purchaseOriginalDraw = nullptr;
const Image *s_purchaseOriginalImage = nullptr;
std::map<GameWindow *, Original> s_purchase;       // retail geometry of the popup windows (restored for Original)
struct PopupArt { const Image *enabled; const Image *disabled; };
std::map<GameWindow *, PopupArt> s_purchaseArt;      // retail bitmaps of popup decoration windows
std::set<GameWindow *> s_purchaseHidden;           // retail tier header texts hidden by the Enhanced layout
GameWindow *s_powerMeter = nullptr;                // the PowerWindow gauge (segment overlay)
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

void EnhancedHudImage::drawGaugeSegments( GameWindow *window, Int x, Int y, Int w, Int h )
{
	if( !window || !EnhancedHud::active() || window != s_powerMeter || !TheDisplay || w <= 0 || h <= 0 )
		return;
	const Color separator = themeColor( EnhancedHud::theme().bevelDark );
	for( int i = 1; i < EnhancedHudPanels::kPowerSegments; ++i )
		TheDisplay->drawLine( x + EnhancedHudPanels::powerSegmentX( i, w ), y, x + EnhancedHudPanels::powerSegmentX( i, w ), y + h, 1, separator );
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

	// General Powers popup: the SAME retail windows are moved to a compact Enhanced layout (EnhancedHudPanels::popupTarget); the
	// retail background and decoration bitmaps are replaced by the Enhanced skin (all restored for Original) and the power
	// icons are aspect-fitted. Content, input, tooltips and purchase logic stay the retail windows'.
	if( GameWindow *gen = m_contextParent[ CP_PURCHASE_SCIENCE ] )
	{
		if( s_purchaseWindow != gen )
		{
			s_purchaseWindow = gen;
			s_purchaseOriginalDraw = gen->winGetDrawFunc();
			s_purchaseOriginalImage = nullptr;
			s_purchase.clear();
			s_purchaseArt.clear();
			s_purchaseHidden.clear();
		}
		if( const Image *img = gen->winGetEnabledImage( 0 ) )
			s_purchaseOriginalImage = img;
		gen->winSetEnabledImage( 0, nullptr );
		gen->winSetDrawFunc( purchasePanelDraw );
		const Int surfaceW = TheDisplay ? (Int)TheDisplay->getWidth() : 800, surfaceH = TheDisplay ? (Int)TheDisplay->getHeight() : 600;
		const double ui = DisplayRuntime::uiScale() > 0.0 ? DisplayRuntime::uiScale() : 1.0;
		const EnhancedHudPanels::PopupPlacement place = EnhancedHudPanels::popupPlacement( surfaceW, surfaceH, layout.island.y, ui );
		std::vector<GameWindow *> shownBefore;
		for( GameWindow *ch = gen->winGetChild(); ch; ch = ch->winGetNext() )
			if( !ch->winIsHidden() )
				shownBefore.push_back( ch );
		EnhancedHudWindows::applyPopupLayout<GameWindow>( gen, place.rect, place.scale,
			[]( GameWindow *w, int *out )
			{
				remember( s_purchase, w );
				const Original &o = s_purchase[ w ];
				out[ 0 ] = o.pos.x; out[ 1 ] = o.pos.y; out[ 2 ] = o.size.x; out[ 3 ] = o.size.y;
			} );
		for( size_t i = 0; i < shownBefore.size(); ++i )
			if( shownBefore[ i ]->winIsHidden() )
				s_purchaseHidden.insert( shownBefore[ i ] );      // hidden by this layout (and only those): unhidden for Original
		for( GameWindow *ch = gen->winGetChild(); ch; ch = ch->winGetNext() )
		{
			const UnsignedInt style = ch->winGetStyle();
			if( BitIsSet( style, GWS_STATIC_TEXT ) || ( style & GWS_USER_WINDOW ) )
			{
				if( s_purchaseArt.find( ch ) == s_purchaseArt.end() )
				{
					PopupArt art = { ch->winGetEnabledImage( 0 ), ch->winGetDisabledImage( 0 ) };
					s_purchaseArt[ ch ] = art;
				}
				ch->winSetEnabledImage( 0, nullptr );
				ch->winSetDisabledImage( 0, nullptr );
			}
			if( BitIsSet( style, GWS_PUSH_BUTTON ) && ch->winGetWindowId() != TheNameKeyGenerator->nameToKey( "GeneralsExpPoints.wnd:ButtonExit" ) )
				EnhancedHudImage::registerAspectFit( ch );
		}
	}
	s_powerMeter = barWindow( "PowerWindow" );

	applyEnhancedPowerShortcutLayout();
}

//-----------------------------------------------------------------------------
void ControlBar::restoreOriginalLayout()
{
	// A strict no-op unless the Enhanced layout was applied: the Original presentation is never touched.
	if( s_barMaster == nullptr && s_powersParent == nullptr )
		return;
	EnhancedHudImage::clear();
	releaseTexts();
	if( s_purchaseWindow && s_purchaseWindow == m_contextParent[ CP_PURCHASE_SCIENCE ] )
	{
		if( s_purchaseOriginalDraw )
			s_purchaseWindow->winSetDrawFunc( s_purchaseOriginalDraw );
		s_purchaseWindow->winSetEnabledImage( 0, s_purchaseOriginalImage );
		// geometry only (the popup's own shown / hidden state belongs to the game and is left alone)
		for( std::map<GameWindow *, Original>::const_iterator it = s_purchase.begin(); it != s_purchase.end(); ++it )
		{
			it->first->winSetPosition( it->second.pos.x, it->second.pos.y );
			it->first->winSetSize( it->second.size.x, it->second.size.y );
		}
		for( std::map<GameWindow *, PopupArt>::const_iterator it = s_purchaseArt.begin(); it != s_purchaseArt.end(); ++it )
		{
			it->first->winSetEnabledImage( 0, it->second.enabled );
			it->first->winSetDisabledImage( 0, it->second.disabled );
		}
		for( std::set<GameWindow *>::const_iterator it = s_purchaseHidden.begin(); it != s_purchaseHidden.end(); ++it )
			( *it )->winHide( FALSE );
	}
	s_purchaseHidden.clear();
	s_purchase.clear();
	s_purchaseArt.clear();
	s_powerMeter = nullptr;
	s_purchaseWindow = nullptr;
	s_purchaseOriginalDraw = nullptr;
	s_purchaseOriginalImage = nullptr;
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
		EnhancedHudWindows::applyPowerColumn<GameWindow>( parent, panel, factor, original, rememberWin,
			layout.powerEdgeRight, layout.powerPlateMargin );
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
	const Int surfaceW = (Int)TheDisplay->getWidth(), surfaceH = (Int)TheDisplay->getHeight();
	// While the General Powers popup is open the tooltip must not cover it (a covered button loses the mouse and the tooltip
	// would flicker): placement keeps clear of the popup's real, current rectangle.
	EnhancedHudLayout::Rect popup = { 0, 0, 0, 0 };
	if( s_purchaseWindow && s_purchaseWindow == m_contextParent[ CP_PURCHASE_SCIENCE ] )
	{
		Bool shown = TRUE;
		for( GameWindow *w = s_purchaseWindow; w; w = w->winGetParent() )
			if( w->winIsHidden() )
				shown = FALSE;
		if( shown )
		{
			Int sx = 0, sy = 0, sw = 0, sh = 0;
			s_purchaseWindow->winGetScreenPosition( &sx, &sy );
			s_purchaseWindow->winGetSize( &sw, &sh );
			popup = EnhancedHudLayout::Rect{ sx, sy, sw, sh };
		}
	}
	Int cursorX = -1, cursorY = -1;
	if( TheMouse )
	{
		cursorX = TheMouse->getMouseStatus()->pos.x;
		cursorY = TheMouse->getMouseStatus()->pos.y;
	}
	const EnhancedHudLayout::Rect spot = EnhancedHudPanels::tooltipPlacement( surfaceW, surfaceH, layout.island, layout.commandFrame, width, height, popup, cursorX, cursorY );
	*x = spot.x;
	*y = spot.y;
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
	if( c.theme->heavyBevel && r.w > 12 && r.h > 12 )
	{
		// structural plate: a second, inner bevel line (thicker edge)
		TheDisplay->drawLine( r.x + a + 2, r.y + 2, r.x + r.w - b - 2, r.y + 2, 1, lightEdge );
		TheDisplay->drawLine( r.x + 2, r.y + a + 2, r.x + 2, r.y + r.h - d - 2, 1, lightEdge );
		TheDisplay->drawLine( r.x + d + 2, r.y + r.h - 3, r.x + r.w - e - 2, r.y + r.h - 3, 1, darkEdge );
		TheDisplay->drawLine( r.x + r.w - 3, r.y + b + 2, r.x + r.w - 3, r.y + r.h - e - 2, 1, darkEdge );
	}
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
	if( c.theme->rivets )
	{
		// domed rivet head: a lit top/left, a dark bottom/right
		const Int rr = std::max<Int>( 1, P( 1.7 ) );
		TheDisplay->drawFillRect( cx - rr - 1, cy - rr - 1, 2 * rr + 3, 2 * rr + 3, c.shadow );
		TheDisplay->drawFillRect( cx - rr, cy - rr, 2 * rr + 1, 2 * rr + 1, themeColor( c.theme->control ) );
		TheDisplay->drawLine( cx - rr, cy - rr, cx + rr - 1, cy - rr, 1, c.highlight );
		TheDisplay->drawLine( cx - rr, cy - rr, cx - rr, cy + rr - 1, 1, c.highlight );
		return;
	}
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

Color themeLerp( const EnhancedHudTheme::Rgba &a, const EnhancedHudTheme::Rgba &b, int num, int den )
{
	if( den <= 0 )
		return themeColor( a );
	EnhancedHudTheme::Rgba m;
	m.r = (unsigned char)( a.r + ( b.r - a.r ) * num / den );
	m.g = (unsigned char)( a.g + ( b.g - a.g ) * num / den );
	m.b = (unsigned char)( a.b + ( b.b - a.b ) * num / den );
	m.a = 255;
	return themeColor( m );
}

// The chassis (level 0): a gunmetal shell whose top corners are machined (stepped chamfer) and whose colour fades from a lit
// upper band to a darker base; an optional fine grain gives it a material. The bottom is square (flush with the screen).
void fillChassis( const HRect &r, Int k, const HudPalette &c, int mask = CM_TOP )
{
	const EnhancedHudTheme::Theme &th = *c.theme;
	const bool top = ( mask & ( CM_TL | CM_TR ) ) != 0, bottom = ( mask & ( CM_BL | CM_BR ) ) != 0;
	const Int bottomZone = bottom ? r.h - k : r.h;
	for( Int y = 0; y < r.h; )
	{
		Int h = 1, inset = 0;
		if( top && y < k )
			inset = k - y;
		else if( bottom && y >= bottomZone )
			inset = y - bottomZone + 1;
		else
			h = std::min<Int>( 4, bottomZone - y );
		h = std::max<Int>( 1, std::min<Int>( h, r.h - y ) );
		TheDisplay->drawFillRect( r.x + inset, r.y + y, r.w - 2 * inset, h, themeLerp( th.plateBand, th.plate, y, r.h * 2 / 3 > 0 ? r.h * 2 / 3 : 1 ) );
		y += h;
	}
	if( th.brushedChassis )
	{
		const Int step = std::max<Int>( 4, P( 2.5 ) );
		for( Int y = k + 2; y < r.h - 2; y += step )
			TheDisplay->drawLine( r.x + 2, r.y + y, r.x + r.w - 3, r.y + y, 1, themeColor( th.chassisLine ) );
	}
}

// Group of small illuminated segments (technical status lights). `lit` of `count` are on.
void drawLedGroup( Int x, Int y, int count, int lit, const HudPalette &c )
{
	const Int side = std::max<Int>( 2, P( 2 ) ), gap = std::max<Int>( 1, P( 1.2 ) );
	for( int i = 0; i < count; ++i )
	{
		const Int lx = x + i * ( side + gap );
		TheDisplay->drawFillRect( lx, y, side, side, i < lit ? c.ready : c.accentDim );
		TheDisplay->drawLine( lx, y, lx + side - 1, y, 1, i < lit ? c.icon : c.wellEdge );
	}
}

// A raised machined frame (level 3 hardware) around a recessed content area: the frame is the visible bezel.
void drawRaisedFrame( const HRect &inner, Int thickness, const HudPalette &c, bool lit )
{
	const EnhancedHudTheme::Theme &th = *c.theme;
	const Int k = th.plateCorner == EnhancedHudTheme::Corner::Chamfer ? P( th.plateCornerRef ) : 0;
	const HRect outer = inflate( inner, thickness );
	drawBevelPlate( outer, k, CM_ALL, lit ? c.warningDark : themeColor( th.control ), lit ? c.warning : c.shadow, false, c );
}

// Raised square caps on the four corners of a pop-up window (popup / overview), for themes with reinforced shoulders.
void drawWindowCaps( const HRect &r, const HudPalette &c )
{
	const Int cs = std::max<Int>( 4, P( 7 ) );
	for( int i = 0; i < 4; ++i )
	{
		const HRect cap = { ( i & 1 ) ? r.x + r.w - cs : r.x, ( i & 2 ) ? r.y + r.h - cs : r.y, cs, cs };
		drawBevelPlate( cap, 0, CM_ALL, themeColor( c.theme->control ), c.shadow, false, c );
	}
}

// A module of the console (level 1): a recessed panel set into the chassis - inner shadow on the top/left, a faint lit lip on
// the bottom/right, machined corners, and a header band with a small group of status lights.
void drawModule( const HRect &r, const HudPalette &c, int litLeds, bool vents = false )
{
	const EnhancedHudTheme::Theme &th = *c.theme;
	if( r.w < 8 || r.h < 8 )
		return;
	const Int k = th.moduleCorner == EnhancedHudTheme::Corner::Chamfer ? P( th.moduleCornerRef ) : 0;
	fillChamfered( r, k, CM_ALL, c.panel );
	const Int depth = P( th.wellDepthRef );
	if( th.bevelModules )
	{
		TheDisplay->drawFillRect( r.x + k, r.y, r.w - 2 * k, depth, c.shadow );
		TheDisplay->drawFillRect( r.x, r.y + k, depth, r.h - 2 * k, c.shadow );
		TheDisplay->drawLine( r.x + k, r.y + r.h - 1, r.x + r.w - k, r.y + r.h - 1, 1, c.highlight );
		TheDisplay->drawLine( r.x + r.w - 1, r.y + k, r.x + r.w - 1, r.y + r.h - k, 1, c.highlight );
	}
	edgeChamfered( r, k, CM_ALL, c.shadow );
	if( th.moduleBrackets && r.w > P( 24 ) && r.h > P( 24 ) )
	{
		// reinforcing L brackets on the four corners of the recessed module
		const Int arm = P( 6 ), thick = std::max<Int>( 2, P( 2 ) );
		const Color bracket = themeColor( th.control );
		const Int xl = r.x, xr = r.x + r.w - arm, yt = r.y, yb = r.y + r.h - thick, xrv = r.x + r.w - thick, ybv = r.y + r.h - arm;
		TheDisplay->drawFillRect( xl, yt, arm, thick, bracket );
		TheDisplay->drawFillRect( xl, yt, thick, arm, bracket );
		TheDisplay->drawFillRect( xr, yt, arm, thick, bracket );
		TheDisplay->drawFillRect( xrv, yt, thick, arm, bracket );
		TheDisplay->drawFillRect( xl, yb, arm, thick, bracket );
		TheDisplay->drawFillRect( xl, ybv, thick, arm, bracket );
		TheDisplay->drawFillRect( xr, yb, arm, thick, bracket );
		TheDisplay->drawFillRect( xrv, ybv, thick, arm, bracket );
		TheDisplay->drawLine( xl, yt, xl + arm - 1, yt, 1, c.highlight );
		TheDisplay->drawLine( xr, yt, xr + arm - 1, yt, 1, c.highlight );
	}
	// header band (slightly darker than the panel) carrying the status lights
	const Int headH = P( 11 );
	TheDisplay->drawFillRect( r.x + depth, r.y + depth, r.w - 2 * depth - 1, headH, themeColor( th.moduleLow ) );
	if( th.vents && vents )
		drawVents( r.x + r.w - P( 30 ), r.y + depth + P( 2 ), P( 20 ), std::max<Int>( 4, headH - P( 4 ) ), 3, c );
	if( th.moduleLeds && litLeds >= 0 )
		drawLedGroup( r.x + P( 9 ), r.y + depth + ( headH - std::max<Int>( 2, P( 2 ) ) ) / 2, 3, litLeds, c );
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

// Filled polygon (even-odd scanline), used for the power symbol.
void fillPolygon( const Int *xs, const Int *ys, int n, Color color )
{
	Int top = ys[ 0 ], bottom = ys[ 0 ];
	for( int i = 1; i < n; ++i ) { top = std::min( top, ys[ i ] ); bottom = std::max( bottom, ys[ i ] ); }
	for( Int y = top; y <= bottom; ++y )
	{
		Int cross[ 16 ];
		int count = 0;
		for( int i = 0; i < n && count < 15; ++i )
		{
			const int j = ( i + 1 ) % n;
			if( ( ys[ i ] <= y && ys[ j ] > y ) || ( ys[ j ] <= y && ys[ i ] > y ) )
				cross[ count++ ] = xs[ i ] + ( y - ys[ i ] ) * ( xs[ j ] - xs[ i ] ) / ( ys[ j ] - ys[ i ] );
		}
		std::sort( cross, cross + count );
		for( int k = 0; k + 1 < count; k += 2 )
			TheDisplay->drawLine( cross[ k ], y, cross[ k + 1 ], y, 1, color );
	}
}

// Electrical bolt symbol inside `r`.
void drawBolt( const HRect &r, Color color )
{
	if( r.w < 4 || r.h < 4 )
		return;
	const Int px[ 6 ] = { r.x + r.w * 6 / 10, r.x + r.w * 1 / 10, r.x + r.w * 5 / 10, r.x + r.w * 3 / 10, r.x + r.w * 9 / 10, r.x + r.w * 5 / 10 };
	const Int py[ 6 ] = { r.y, r.y + r.h * 55 / 100, r.y + r.h * 55 / 100, r.y + r.h - 1, r.y + r.h * 38 / 100, r.y + r.h * 38 / 100 };
	fillPolygon( px, py, 6, color );
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

const int kHudWinCount = 8 + 12 + 1;
GameWindow *s_hudWin[ kHudWinCount ];     // 0 radar, 1-4 utility buttons, 5 XP trough, 6 attack lamp, 7 CommandWindow, 8.. command buttons
const char *const s_hudWinNames[ kHudWinCount ] = { "LeftHUD", "ButtonOptions", "ButtonIdleWorker", "ButtonPlaceBeacon",
                                         "PopupCommunicator", "ExpBarForeground", "WinUAttack", "CommandWindow",
                                         "ButtonCommand01", "ButtonCommand02", "ButtonCommand03", "ButtonCommand04",
                                         "ButtonCommand05", "ButtonCommand06", "ButtonCommand07", "ButtonCommand08",
                                         "ButtonCommand09", "ButtonCommand10", "ButtonCommand11", "ButtonCommand12", "PowerWindow" };

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

// Colour of a semantic outline role (EnhancedHudPanels::Outline). Blue (accent) is reserved for hover/pressed; the muted blue
// marks an available action at rest; gold is ownership.
Color outlineColor( EnhancedHudPanels::Outline o, const HudPalette &c )
{
	switch( o )
	{
		case EnhancedHudPanels::Outline::Active: return c.accent;
		case EnhancedHudPanels::Outline::Muted: return c.accentDim;
		case EnhancedHudPanels::Outline::Gold: return c.lit;
		default: return c.shadow;
	}
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
	drawRaisedFrame( r, P( 2 ), c, on );
	drawRecess( housing, c, P( 1.5 ) );
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
	const EnhancedHudTheme::Theme &th = *c.theme;
	const Color body = !enabled ? themeColor( th.controlLow ) : ( down ? themeColor( th.controlLow ) : themeColor( th.control ) );
	drawBevelPlate( r, k, CM_ALL, body, enabled ? ( hover ? c.accent : c.shadow ) : c.shadow, down && enabled, c );
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
		drawBevelPlate( plate, k, mask, ready ? themeColor( c.theme->control ) : themeColor( c.theme->controlLow ), ready ? ( hover ? c.accent : c.shadow ) : c.shadow, down && ready, c );
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
	const Int shellK = th.shellCorner == EnhancedHudTheme::Corner::Chamfer ? P( th.shellCornerRef ) : 0;

	// 1. LEVEL 0 - CHASSIS: one gunmetal shell (graded, finely grained) with stepped machined top corners and a lit top edge.
	//    The bottom is square and flush with the screen. Raised structural ribs in the gaps between the modules make the
	//    modules read as panels set into ONE chassis instead of four boxes that touch.
	fillChassis( isl, shellK, c );
	edgeChamfered( isl, shellK, CM_TOP, c.shadow );
	TheDisplay->drawLine( isl.x + shellK + 1, isl.y + 1, isl.x + isl.w - shellK - 2, isl.y + 1, 1, c.highlight );
	TheDisplay->drawLine( isl.x + shellK + 1, isl.y + 2, isl.x + isl.w - shellK - 2, isl.y + 2, 1, themeColor( th.plateBand ) );
	if( th.girder )
	{
		// a riveted structural girder across the top margin (modules are drawn over anything that reaches them)
		const HRect gd = { isl.x + P( 9 ), isl.y + P( 3 ), isl.w - 2 * P( 9 ), std::max<Int>( 3, P( 4 ) ) };
		TheDisplay->drawFillRect( gd.x, gd.y, gd.w, gd.h, themeColor( th.control ) );
		TheDisplay->drawLine( gd.x, gd.y, gd.x + gd.w - 1, gd.y, 1, c.highlight );
		TheDisplay->drawLine( gd.x, gd.y + gd.h - 1, gd.x + gd.w - 1, gd.y + gd.h - 1, 1, c.shadow );
		if( th.rivets )
			for( Int x = gd.x + P( 10 ); x < gd.x + gd.w - P( 6 ); x += P( 18 ) )
				TheDisplay->drawFillRect( x, gd.y + gd.h / 2 - 1, std::max<Int>( 2, P( 1.6 ) ), std::max<Int>( 2, P( 1.6 ) ), c.shadow );
	}
	if( th.junctionRibs )
	{
		for( int i = 0; i < 3; ++i )
		{
			const Int gx = mods[ i ].right(), gw = mods[ i + 1 ].x - mods[ i ].right();
			if( gw < 4 )
				continue;
			const HRect rib = { gx + 1, isl.y + P( 8 ), gw - 2, isl.h - P( 8 ) - P( 5 ) };
			TheDisplay->drawFillRect( rib.x, rib.y, rib.w, rib.h, themeColor( th.plateBand ) );
			TheDisplay->drawLine( rib.x, rib.y, rib.x, rib.y + rib.h, 1, c.highlight );
			TheDisplay->drawLine( rib.x + rib.w - 1, rib.y, rib.x + rib.w - 1, rib.y + rib.h, 1, c.shadow );
			if( th.seams )
			{
				const Int cx = rib.x + rib.w / 2, ym = rib.y + rib.h / 2;
				TheDisplay->drawLine( cx, rib.y + P( 5 ), cx, ym - P( 5 ), 1, c.shadow );
				TheDisplay->drawLine( cx, ym + P( 5 ), cx, rib.y + rib.h - P( 5 ), 1, c.shadow );
			}
			// one status light on each rib: the console is powered
			TheDisplay->drawFillRect( rib.x + rib.w / 2 - 1, rib.y + P( 2 ), 2, std::max<Int>( 2, P( 2 ) ), c.ready );
			if( th.rivets )
				for( Int y = rib.y + P( 14 ); y < rib.y + rib.h - P( 8 ); y += P( 22 ) )
					drawScrew( rib.x + rib.w / 2, y, c );
		}
	}
	if( th.vents )
	{
		// asymmetric end caps: a small ventilation grille at the left end, a pair of power lamps at the right end
		drawVents( isl.x + P( 2.5 ), isl.y + isl.h / 2 - P( 9 ), P( 5 ), P( 18 ), 5, c );
		drawLedGroup( isl.x + isl.w - P( 7 ), isl.y + P( 18 ), 1, 1, c );
		drawLedGroup( isl.x + isl.w - P( 7 ), isl.y + P( 22 ), 1, 0, c );
	}
	if( th.fasteners )
	{
		// two corner fasteners only, at the flush bottom corners
		drawScrew( isl.x + P( 4 ), isl.y + isl.h - P( 4 ), c );
		drawScrew( isl.x + isl.w - P( 4 ) - 1, isl.y + isl.h - P( 4 ), c );
	}

	// 2. LEVEL 1 - MODULES: recessed panels set into the chassis. The lit status lights show which subsystems are powered.
	drawModule( mods[ 0 ], c, 3 );
	drawModule( mods[ 1 ], c, 2, true );
	drawModule( mods[ 2 ], c, 3 );
	drawModule( mods[ 3 ], c, 2, true );
	if( th.cornerCaps )
	{
		// raised square caps on the top shoulders of the shell (the margin around the modules is 8 reference units)
		const Int cs = P( 8 );
		for( int side = 0; side < 2; ++side )
		{
			const HRect cap = { side == 0 ? isl.x : isl.x + isl.w - cs, isl.y, cs, cs };
			drawBevelPlate( cap, 0, CM_ALL, themeColor( th.control ), c.shadow, false, c );
		}
	}

	// 3. RADAR MODULE. The radar keeps the map's aspect ratio and (by retail design, W3DRadar::draw) paints the unused part
	//    of its window opaque black. In Enhanced the radar WINDOW is fitted to the map's aspect inside the module's radar
	//    slot (uniform, centred), so the radar fills its window, the mouse mapping stays exact and the map is never
	//    stretched. Around the fitted radar sits a military tactical display: a raised machined bezel around a deep recessed
	//    screen; the bezel turns red while the attack warning is lit. Original keeps the retail window and its black bars.
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
		drawRaisedFrame( fitted, bz, c, lampLit );
		if( lampLit )
			TheDisplay->drawOpenRect( fitted.x - bz - 1, fitted.y - bz - 1, fitted.w + 2 * bz + 2, fitted.h + 2 * bz + 2, 1, c.warning );
		drawRecess( inflate( fitted, 1 ), c, P( 1.5 ) );       // the screen sits deeper than the bezel
		if( th.fasteners )
		{
			const Int o = std::max<Int>( 2, bz / 2 );
			drawScrew( fitted.x - bz + o, fitted.y - bz + o, c );
			drawScrew( fitted.x + fitted.w + bz - o - 1, fitted.y + fitted.h + bz - o - 1, c );
			if( th.rivets )
			{
				drawScrew( fitted.x + fitted.w + bz - o - 1, fitted.y - bz + o, c );
				drawScrew( fitted.x - bz + o, fitted.y + fitted.h + bz - o - 1, c );
			}
		}
		if( th.moduleLeds )
			drawLedGroup( fitted.x - bz + std::max<Int>( 2, bz / 2 ) + P( 5 ), fitted.y + fitted.h + std::max<Int>( 1, bz / 2 ) - 1, 3, lampLit ? 0 : 2, c );
		if( th.tickMarks )
		{
			// graduation ticks along the top bezel
			const Int ty = fitted.y - bz + std::max<Int>( 1, bz / 3 );
			for( int i = 1; i < 10; ++i )
			{
				const Int tx = fitted.x + fitted.w * i / 10;
				TheDisplay->drawLine( tx, ty, tx, ty + ( i == 5 ? 2 : 1 ), 1, lampLit ? c.warning : c.accentDim );
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

	// 4. UNIT INFORMATION MODULE: the retail selection block is a tactical information display: a raised bezel around a
	//    recessed screen (the cameo, upgrades and queue are mounted inside).
	{
		EnhancedHudLayout::Rect dst;
		bool hide = false;
		if( EnhancedHudLayout::destinationFor( "WinUnitSelected", dst, hide ) )
		{
			const HRect screen = inflate( EnhancedHudLayout::toPhysical( l, dst ), 1 );
			drawRaisedFrame( screen, std::max<Int>( 2, g / 2 + 1 ), c, false );
			drawRecess( screen, c );
		}
	}

	// 5. COMMAND MODULE: resource readout and power gauge share a header strip; below it the command bay: a recessed keypad
	//    in a raised frame with twelve fixed cells (retail slot indices). Empty cells are subdued shallow wells; occupied
	//    cells are raised machined plates with a state rail; hover = blue rim, pressed = depressed, disabled = muted.
	{
		const Int cg = std::max<Int>( 2, g / 2 );
		const HRect strip = { l.cashFrame.x - cg, l.cashFrame.y - cg,
		                      l.commandFrame.right() - ( l.cashFrame.x - l.commandFrame.x ) - ( l.cashFrame.x - cg ) + cg,
		                      l.cashFrame.h + 2 * cg };
		drawRecess( strip, c );
		// POWER METER: permanent instrumentation. A raised housing around a recessed track of kPowerSegments segments is ALWAYS
		// drawn (empty capacity stays visible); the retail gauge - the game's real production / consumption in its own green /
		// yellow / red - fills it, and segment separators are drawn over fill and track by the gauge draw. The bolt symbol is
		// ready-blue while power is sufficient and warning-red when it is not (Energy::hasSufficientPower, read only).
		if( GameWindow *pw = s_hudWin[ 20 ] )
			if( !pw->winIsHidden() )
			{
				const HRect m = windowRect( pw );
				drawRaisedFrame( m, std::max<Int>( 2, P( 1.5 ) ), c, false );
				drawRecess( m, c, P( 1.2 ) );
				for( int i = 0; i < EnhancedHudPanels::kPowerSegments; ++i )
				{
					const Int x0 = m.x + EnhancedHudPanels::powerSegmentX( i, m.w ), x1 = m.x + EnhancedHudPanels::powerSegmentX( i + 1, m.w );
					TheDisplay->drawFillRect( x0 + 1, m.y + 2, std::max<Int>( 1, x1 - x0 - 1 ), std::max<Int>( 1, m.h - 4 ), c.panelLow );
				}
				bool sufficient = true;
				if( TheControlBar )
					if( Player *player = TheControlBar->getCurrentlyViewedPlayer() )
						if( Energy *energy = player->getEnergy() )
							sufficient = energy->hasSufficientPower();
				drawBolt( HRect{ m.x - P( 17 ), m.y - 1, P( 12 ), m.h + 2 }, sufficient ? c.ready : c.warning );
			}
		if( !l.cashFrame.empty() )
		{
			// compact military data readout: raised bezel, deep dark display, blue status edge, status segments
			drawRaisedFrame( l.cashFrame, std::max<Int>( 2, P( 1.5 ) ), c, false );
			drawRecess( l.cashFrame, c, P( 1.5 ) );
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
		drawRaisedFrame( bayRect, std::max<Int>( 2, P( 2 ) ), c, false );
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
					// subdued empty cell: a shallow well, no border
					TheDisplay->drawFillRect( cell.x, cell.y, cell.w, cell.h, c.panelLow );
					TheDisplay->drawLine( cell.x, cell.y, cell.x + cell.w - 1, cell.y, 1, c.shadow );
					continue;
				}
				const bool enabled = BitIsSet( btn->winGetStatus(), WIN_STATUS_ENABLED ) ? true : false;
				bool hover, down;
				pointerState( cell, &hover, &down );
				drawBevelPlate( cell, k, CM_ALL, enabled ? themeColor( th.control ) : themeColor( th.controlLow ), enabled ? ( hover ? c.accent : c.shadow ) : c.shadow, down && enabled, c );
				if( th.stateRails )
					TheDisplay->drawFillRect( cell.x + k + 1, cell.y + cell.h - 3, cell.w - 2 * k - 2, std::max<Int>( 1, P( 1 ) ), enabled ? ( hover ? c.ready : c.accentDim ) : c.shadow );
			}
		}
	}

	// 6. GENERAL MODULE: ONE rank/status unit. The XP gauge sits in its own raised housing with instrument ticks; the General
	//    Powers entry is mounted in a housing of its own; the rank grid is a recessed bay whose earned cells are raised plates
	//    in restrained gold and whose unearned cells recede.
	if( GameWindow *bar = s_hudWin[ 5 ] )
	{
		const HRect r = windowRect( bar );
		drawRaisedFrame( r, std::max<Int>( 2, P( 2 ) ), c, false );
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
			const HRect gr = inflate( EnhancedHudLayout::toPhysical( l, dst ), 1 );
			drawRaisedFrame( gr, std::max<Int>( 2, g / 2 + 1 ), c, false );
			drawRecess( gr, c );
		}

		Int rank = 1, levels = 1, points = 0, skill = 0;
		if( TheControlBar )
			if( Player *player = TheControlBar->getCurrentlyViewedPlayer() )
			{
				rank = player->getRankLevel();
				points = player->getSciencePurchasePoints();
				skill = player->getSkillPoints();
			}
		if( TheRankInfoStore )
			levels = TheRankInfoStore->getRankLevelCount();
		const int slots = EnhancedHudPanels::milestoneCount( levels );
		const Int k = th.plateCorner == EnhancedHudTheme::Corner::Chamfer ? P( th.plateCornerRef ) : 0;
		if( slots > 0 )
		{
			HRect rankBay = EnhancedHudLayout::toPhysical( l, EnhancedHudLayout::rankSlot( 0 ) );
			for( int i = 1; i < slots; ++i )
				rankBay = EnhancedHudWindows::unionOf( rankBay, EnhancedHudLayout::toPhysical( l, EnhancedHudLayout::rankSlot( i ) ) );
			drawRecess( inflate( rankBay, std::max<Int>( 2, g / 2 ) ), c );
		}
		// The four cells are the General's promotion milestones (EnhancedHudPanels): earned = raised plate, gold star and gold
		// rail; next = recessed cell that fills with the REAL xp progress toward it; later = recessed, dim star.
		for( int i = 0; i < slots; ++i )
		{
			const EnhancedHudPanels::Milestone state = EnhancedHudPanels::milestoneState( rank, levels, i, slots );
			const HRect slot = EnhancedHudLayout::toPhysical( l, EnhancedHudLayout::rankSlot( i ) );
			const HRect star = EnhancedHudLayout::toPhysical( l, EnhancedHudLayout::rankStar( i ) );
			if( state == EnhancedHudPanels::Milestone::Earned )
			{
				drawBevelPlate( slot, k, CM_ALL, themeColor( th.control ), c.shadow, false, c );
				if( th.stateRails )
					TheDisplay->drawFillRect( slot.x + k + 1, slot.y + slot.h - 3, slot.w - 2 * k - 2, std::max<Int>( 1, P( 1 ) ), c.lit );
			}
			else
			{
				TheDisplay->drawFillRect( slot.x, slot.y, slot.w, slot.h, c.panelLow );
				TheDisplay->drawLine( slot.x, slot.y, slot.x + slot.w - 1, slot.y, 1, c.shadow );
				if( state == EnhancedHudPanels::Milestone::Next && TheRankInfoStore )
				{
					const int permille = EnhancedHudPanels::milestoneProgressPermille( skill, levels, i, slots,
						[]( int level ) { const RankInfo *info = TheRankInfoStore->getRankInfo( level ); return info ? info->m_skillPointsNeeded : 0; } );
					const Int fill = ( slot.h - 4 ) * permille / 1000;
					if( fill > 0 )
						TheDisplay->drawFillRect( slot.x + 2, slot.y + slot.h - 2 - fill, slot.w - 4, fill, c.accentDim );
					TheDisplay->drawLine( slot.x + 2, slot.y + slot.h - 2 - fill, slot.x + slot.w - 3, slot.y + slot.h - 2 - fill, 1, c.ready );
				}
			}
			fillStar( star.x + star.w / 2, star.y + star.h / 2, std::max( 2, star.h / 2 ),
			          state == EnhancedHudPanels::Milestone::Earned ? c.lit : ( state == EnhancedHudPanels::Milestone::Next ? c.iconDim : c.litDim ) );
		}
		// unspent General Points: lit pips between the entry and the milestones (blue = available to spend; gold stays rank)
		if( EnhancedHudLayout::destinationFor( "ButtonGeneral", dst, hide ) )
		{
			const HRect gr = EnhancedHudLayout::toPhysical( l, dst );
			const int pips = EnhancedHudPanels::pointPips( points, 5 );
			const Int side = std::max<Int>( 2, P( 2 ) ), gap = std::max<Int>( 1, P( 1.2 ) );
			const Int total = 5 * side + 4 * gap;
			drawLedGroup( gr.x + ( gr.w - total ) / 2, gr.y + gr.h + std::max<Int>( 3, g ), 5, pips, c );
			// the entry button: no outline at rest (it never looks keyboard-focused); blue only under the pointer, pressed = sunk
			bool hover, down;
			pointerState( gr, &hover, &down );
			const EnhancedHudPanels::Interaction interaction = EnhancedHudPanels::interactionOf( hover, down );
			if( EnhancedHudPanels::generalButtonOutline( interaction ) == EnhancedHudPanels::Outline::Active )
				TheDisplay->drawOpenRect( gr.x - 1, gr.y - 1, gr.w + 2, gr.h + 2, 1, c.accent );
			if( interaction == EnhancedHudPanels::Interaction::Pressed )
			{
				TheDisplay->drawFillRect( gr.x, gr.y, gr.w, std::max<Int>( 2, P( 1.5 ) ), c.shadow );
				TheDisplay->drawFillRect( gr.x, gr.y, std::max<Int>( 2, P( 1.5 ) ), gr.h, c.shadow );
			}
			// unspent General Points: a small status lamp on the housing's upper corner (the pips below carry the count)
			if( EnhancedHudPanels::generalNotificationLamp( points ) )
			{
				const Int lamp = std::max<Int>( 3, P( 4 ) );
				const HRect lr = { gr.x + gr.w - lamp - std::max<Int>( 2, P( 2 ) ), gr.y + std::max<Int>( 2, P( 2 ) ), lamp, lamp };
				TheDisplay->drawFillRect( lr.x - 1, lr.y - 1, lr.w + 2, lr.h + 2, c.shadow );
				TheDisplay->drawFillRect( lr.x, lr.y, lr.w, lr.h, c.ready );
			}
		}
	}
	// (the Global Powers surface is not part of the island: see powersPanelDraw, drawn by the shortcut window itself)
}

//-----------------------------------------------------------------------------
// Text, the General Powers popup skin and the Battle Overview skin. All of it reuses the primitives above: the same Theme, the
// same chassis / recessed module / raised control construction; the retail windows (texts, buttons, lists) stay in charge of
// content and input, the skin only draws behind them. Nothing here changes what the popup or the overview does.
namespace
{

struct TextEntry { DisplayString *ds; Int size; Bool bold; };
std::map<std::string, TextEntry> s_texts;

void releaseTexts()
{
	for( std::map<std::string, TextEntry>::iterator it = s_texts.begin(); it != s_texts.end(); ++it )
		if( it->second.ds && TheDisplayStringManager )
			TheDisplayStringManager->freeDisplayString( it->second.ds );
	s_texts.clear();
}

// align: 0 = x is the left edge, 1 = centre, 2 = right edge. Uses the game's own font library and display strings.
void drawLabel( const char *key, const UnicodeString &text, Int x, Int y, Int size, Bool bold, Color color, int align, Int maxWidth = 0 )
{
	if( !TheDisplayStringManager || !TheFontLibrary )
		return;
	TextEntry &e = s_texts[ key ];
	if( !e.ds )
	{
		e.ds = TheDisplayStringManager->newDisplayString();
		e.size = 0;
		e.bold = FALSE;
	}
	if( !e.ds )
		return;
	if( e.size != size || e.bold != bold )
	{
		e.ds->setFont( TheFontLibrary->getFont( AsciiString( "Arial" ), size, bold ) );
		e.size = size;
		e.bold = bold;
	}
	if( e.ds->getText().compare( text ) != 0 )
		e.ds->setText( text );
	Int w = 0, h = 0;
	e.ds->getSize( &w, &h );
	if( maxWidth > 0 && w > maxWidth )
	{
		// a label never crosses its module: shrink the point size until it fits (measured, not guessed)
		const Int fitted = EnhancedHudPanels::fitPointSize( size, 7, w, maxWidth );
		if( fitted != size )
		{
			e.ds->setFont( TheFontLibrary->getFont( AsciiString( "Arial" ), fitted, bold ) );
			e.size = fitted;
			e.ds->getSize( &w, &h );
		}
	}
	if( align == 1 )
		x -= w / 2;
	else if( align == 2 )
		x -= w;
	e.ds->draw( x, y, color, themeColor( EnhancedHud::theme().bevelDark ) );
}

GameWindow *childByName( GameWindow *parent, const char *fullName )
{
	return TheWindowManager->winGetWindowFromId( parent, TheNameKeyGenerator->nameToKey( fullName ) );
}

// ---- General Powers popup (GeneralsExpPoints.wnd) --------------------------------------------------------------------------
// The retail popup keeps its windows, texts, progress bar and buttons; the skin is the parent's draw function: a chassis with a
// header module and one recessed module per rank section, a raised housing around the experience bar and a raised plate
// behind every visible power. Plate state comes from the flags the ControlBar already sets (available / purchased / locked).
void purchasePanelDraw( GameWindow *window, WinInstanceData * )
{
	if( !EnhancedHud::active() || !TheDisplay || !window )
		return;
	const HudPalette c = makePalette();
	const EnhancedHudTheme::Theme &th = *c.theme;
	const HRect r = windowRect( window );
	const double u = r.w / (double)EnhancedHudPanels::kPopupNewW;
	s_drawScale = std::max( 0.5, u );
	// real, read-only state of the local player (the same numbers the retail popup shows)
	Player *player = TheControlBar ? TheControlBar->getCurrentlyViewedPlayer() : nullptr;
	Int rank = 1, levels = 1, points = 0, skill = 0, skillUp = 0;
	if( player )
	{
		rank = player->getRankLevel();
		points = player->getSciencePurchasePoints();
		skill = player->getSkillPoints();
		skillUp = player->getSkillPointsLevelUp();
	}
	if( TheRankInfoStore )
		levels = TheRankInfoStore->getRankLevelCount();

	// the console: one chassis with machined corners all round; raised ribs join the stacked modules
	const Int k = th.windowCorner == EnhancedHudTheme::Corner::Chamfer ? P( th.windowCornerRef ) : 0;
	fillChassis( r, k, c, CM_ALL );
	edgeChamfered( r, k, CM_ALL, c.shadow );
	TheDisplay->drawLine( r.x + k + 1, r.y + 1, r.x + r.w - k - 2, r.y + 1, 1, c.highlight );
	auto place = [&]( const HRect &ref ) { const HRect s = EnhancedHudPanels::popupScaled( ref, u ); return HRect{ r.x + s.x, r.y + s.y, s.w, s.h }; };
	const HRect header = place( EnhancedHudPanels::kPopupNewHeader );
	const HRect tiers[ 3 ] = { place( EnhancedHudPanels::kPopupNewTier1 ), place( EnhancedHudPanels::kPopupNewTier3 ),
	                           place( EnhancedHudPanels::kPopupNewTier5 ) };
	const HRect stack[ 4 ] = { header, tiers[ 0 ], tiers[ 1 ], tiers[ 2 ] };
	if( th.junctionRibs )
		for( int i = 0; i < 3; ++i )
		{
			const Int gy = stack[ i ].bottom(), gh = stack[ i + 1 ].y - stack[ i ].bottom();
			if( gh < 4 )
				continue;
			const HRect rib = { r.x + P( 16 ), gy + 1, r.w - 2 * P( 16 ), gh - 2 };
			TheDisplay->drawFillRect( rib.x, rib.y, rib.w, rib.h, themeColor( th.plateBand ) );
			TheDisplay->drawLine( rib.x, rib.y, rib.x + rib.w, rib.y, 1, c.highlight );
			TheDisplay->drawLine( rib.x, rib.y + rib.h - 1, rib.x + rib.w, rib.y + rib.h - 1, 1, c.shadow );
			if( th.seams )
			{
				const Int cy = rib.y + rib.h / 2, cx = rib.x + rib.w / 2;
				TheDisplay->drawLine( rib.x + P( 8 ), cy, cx - P( 6 ), cy, 1, c.shadow );
				TheDisplay->drawLine( cx + P( 6 ), cy, rib.x + rib.w - P( 8 ), cy, 1, c.shadow );
			}
		}
	drawModule( header, c, 3 );
	for( int i = 0; i < 3; ++i )
		drawModule( tiers[ i ], c, -1 );
	if( th.cornerCaps )
		drawWindowCaps( r, c );

	// ---- header: rank, experience track with numbers and the next promotion, General Points -------------------------------------
	{
		UnicodeString rankText;
		rankText.format( L"%ls %d / %d", TheGameText->fetchOrSubstitute( "GUI:EnhancedRank", L"RANK" ).str(), rank, levels );
		drawLabel( "pop.rank", rankText, r.x + P( 216 ), r.y + P( 20 ), P( 9 ), FALSE, c.iconDim, 0, P( 100 ) );
	}
	if( GameWindow *bar = childByName( window, "GeneralsExpPoints.wnd:ProgressBarExperience" ) )
	{
		const HRect b = windowRect( bar );
		drawRaisedFrame( b, P( 2.5 ), c, false );
		drawRecess( b, c, P( 1.5 ) );                       // the track stays visible when the experience is zero
		for( int i = 1; i < 10; ++i )
		{
			const Int tx = b.x + b.w * i / 10;
			TheDisplay->drawLine( tx, b.y + b.h - 2, tx, b.y + b.h - 2 - ( i == 5 ? 3 : 1 ), 1, i == 5 ? c.accent : c.accentDim );
		}
		const Int ty = b.y - P( 2 );
		if( rank < levels )
		{
			UnicodeString numbers;
			numbers.format( L"%d / %d", skill, skillUp );
			drawLabel( "pop.xp", numbers, b.x + b.w + P( 10 ), b.y - P( 3 ), P( 10 ), TRUE, c.ready, 0, P( 66 ) );
			AsciiString key;
			key.format( "SCIENCE:Rank%d", rank + 1 );
			UnicodeString next;
			next.format( L"%ls  ", TheGameText->fetchOrSubstitute( "GUI:EnhancedNext", L"NEXT" ).str() );
			next.concat( TheGameText->fetch( key ) );
			drawLabel( "pop.next", next, b.x, b.y + b.h + P( 6 ), P( 9 ), FALSE, c.icon, 0, P( 236 ) );
		}
		else
			drawLabel( "pop.next", TheGameText->fetchOrSubstitute( "GUI:EnhancedMaxRank", L"MAXIMUM RANK" ), b.x, b.y + b.h + P( 6 ), P( 9 ), TRUE, c.lit, 0, P( 236 ) );
		(void)ty;
	}
	{
		// General Points: a dedicated readout - label, large number, rim and glow when points are unspent
		const HRect pr = place( HRect{ 330, 14, 94, 70 } );
		drawRaisedFrame( pr, P( 2 ), c, false );
		drawRecess( pr, c, P( 1.5 ) );
		if( points > 0 )
		{
			// an instrument, not a button: a status rail along the top and foot (no surrounding rectangle)
			TheDisplay->drawFillRect( pr.x + 2, pr.y + 2, pr.w - 4, std::max<Int>( 2, P( 1.5 ) ), c.ready );
			TheDisplay->drawFillRect( pr.x + 2, pr.y + pr.h - 4, pr.w - 4, std::max<Int>( 2, P( 1.5 ) ), c.ready );
		}
		drawLabel( "pop.gp", TheGameText->fetchOrSubstitute( "GUI:EnhancedGeneralPoints", L"GENERAL POINTS" ), pr.x + pr.w / 2, pr.y + P( 5 ), P( 8 ), TRUE,
		           points > 0 ? c.icon : c.iconDim, 1, pr.w - P( 6 ) );
		UnicodeString count;
		count.format( L"%d", points );
		drawLabel( "pop.gpn", count, pr.x + pr.w / 2, pr.y + P( 20 ), P( 30 ), TRUE, points > 0 ? c.ready : c.iconDim, 1, pr.w - P( 8 ) );
	}

	// ---- tiers: label with stars, rank lamp ----------------------------------------------------------------------------------------
	static const char *const tierKey[ 3 ] = { "GUI:EnhancedTier1", "GUI:EnhancedTier3", "GUI:EnhancedTier5" };
	static const wchar_t *const tierText[ 3 ] = { L"1 STAR POWERS", L"3 STAR POWERS", L"5 STAR POWERS" };
	static const char *const labelKey[ 3 ] = { "pop.t1", "pop.t3", "pop.t5" };
	static const char *const stateKey[ 3 ] = { "pop.s1", "pop.s3", "pop.s5" };
	for( int i = 0; i < 3; ++i )
	{
		const HRect t = tiers[ i ];
		const Int required = EnhancedHudPanels::popupRequiredRank( i, levels );
		const bool open = rank >= required;
		const Int starSide = P( 5 ), starGap = P( 2 );
		Int sx = t.x + P( 10 );
		for( int s = 0; s < EnhancedHudPanels::kPopupTierStars[ i ]; ++s )
		{
			fillStar( sx + starSide, t.y + P( 11 ), starSide, open ? c.lit : c.litDim );
			sx += 2 * starSide + starGap;
		}
		drawLabel( labelKey[ i ], TheGameText->fetchOrSubstitute( tierKey[ i ], tierText[ i ] ), sx + P( 4 ), t.y + P( 5 ), P( 10 ), TRUE, open ? c.icon : c.iconDim, 0, t.x + t.w * 60 / 100 - sx );
		UnicodeString state;
		if( open )
			state = TheGameText->fetchOrSubstitute( "GUI:EnhancedUnlocked", L"UNLOCKED" );
		else
			state.format( L"%ls %d", TheGameText->fetchOrSubstitute( "GUI:EnhancedRankNeeded", L"REQUIRES RANK" ).str(), required );
		drawLabel( stateKey[ i ], state, t.x + t.w - P( 10 ), t.y + P( 6 ), P( 8 ), TRUE, open ? c.ready : c.iconDim, 2, t.w * 34 / 100 );
	}

	// ---- powers -------------------------------------------------------------------------------------------------------------------
	struct Plate { GameWindow *w; HRect rect; ScienceType st; EnhancedHudPanels::PowerState state; };
	std::vector<Plate> plates;
	GameWindow *exitButton = childByName( window, "GeneralsExpPoints.wnd:ButtonExit" );
	for( GameWindow *ch = window->winGetChild(); ch; ch = ch->winGetNext() )
	{
		if( ch->winIsHidden() || !BitIsSet( ch->winGetStyle(), GWS_PUSH_BUTTON ) || ch == exitButton )
			continue;
		ScienceType st = SCIENCE_INVALID;
		if( const CommandButton *cb = (const CommandButton *)GadgetButtonGetData( ch ) )
			if( !cb->getScienceVec().empty() )
				st = cb->getScienceVec()[ 0 ];
		const bool enabled = BitIsSet( ch->winGetStatus(), WIN_STATUS_ENABLED ) ? true : false;
		const bool owned = player && st != SCIENCE_INVALID && player->hasScience( st );
		const bool prereqs = player && st != SCIENCE_INVALID && TheScienceStore && TheScienceStore->playerHasPrereqsForScience( player, st );
		Plate p = { ch, windowRect( ch ), st, EnhancedHudPanels::powerStateDetailed( enabled, owned || BitIsSet( ch->winGetStatus(), WIN_STATUS_ALWAYS_COLOR ), prereqs ) };
		plates.push_back( p );
	}
	// the real prerequisite relation between the shown powers (ScienceStore, read only)
	const int count = (int)plates.size();
	std::vector<HRect> rects( plates.size() );
	for( int i = 0; i < count; ++i )
		rects[ i ] = plates[ i ].rect;
	auto isPrerequisite = [&]( int a, int b )
	{
		if( !TheScienceStore || plates[ a ].st == SCIENCE_INVALID || plates[ b ].st == SCIENCE_INVALID )
			return false;
		ScienceVec pre;
		TheScienceStore->getPrerequisiteSciences( plates[ b ].st, pre );
		return std::find( pre.begin(), pre.end(), plates[ a ].st ) != pre.end();
	};
	EnhancedHudPanels::Connector links[ 24 ];
	const int linkCount = count > 0 ? EnhancedHudPanels::chainConnectors( &rects[ 0 ], count, isPrerequisite, links, 24 ) : 0;
	for( int i = 0; i < linkCount; ++i )
	{
		const Plate &a = plates[ links[ i ].from ], &b = plates[ links[ i ].to ];
		const Int cx = a.rect.x + a.rect.w / 2, y0 = a.rect.bottom() + P( 3 ), y1 = b.rect.y - P( 3 );
		const bool ownedB = b.state == EnhancedHudPanels::PowerState::Purchased;
		const bool nextStep = a.state == EnhancedHudPanels::PowerState::Purchased && !ownedB;
		const Color rail = ownedB ? c.lit : ( nextStep ? c.ready : c.accentDim );
		const Int w = std::max<Int>( 2, P( 1.6 ) );
		if( y1 > y0 )
		{
			TheDisplay->drawFillRect( cx - w / 2, y0, w, std::max<Int>( 1, y1 - y0 - P( 3 ) ), rail );
			const Int ax[ 3 ] = { cx - P( 3 ), cx + P( 3 ), cx }, ay[ 3 ] = { y1 - P( 3 ), y1 - P( 3 ), y1 };
			fillPolygon( ax, ay, 3, rail );
		}
	}
	// plates (with chain step numerals) and the hovered power for the detail strip
	const Int pk = th.plateCorner == EnhancedHudTheme::Corner::Chamfer ? P( th.plateCornerRef ) : 0;
	int hovered = -1;
	for( int i = 0; i < count; ++i )
	{
		const HRect b = plates[ i ].rect;
		bool hover, down;
		pointerState( inflate( b, P( 3 ) ), &hover, &down );
		if( hover )
			hovered = i;
		const EnhancedHudPanels::PowerState state = plates[ i ].state;
		const HRect plate = inflate( b, P( 3 ) );
		switch( state )
		{
			case EnhancedHudPanels::PowerState::Available:
				drawBevelPlate( plate, pk, CM_ALL, themeColor( th.control ), outlineColor( EnhancedHudPanels::powerOutline( state, EnhancedHudPanels::interactionOf( hover, down ) ), c ), down, c );
				if( th.stateRails )
					TheDisplay->drawFillRect( plate.x + pk + 1, plate.y + plate.h - 3, plate.w - 2 * pk - 2, std::max<Int>( 1, P( 1 ) ), c.ready );
				break;
			case EnhancedHudPanels::PowerState::Purchased:
				drawBevelPlate( plate, pk, CM_ALL, themeColor( th.control ), outlineColor( EnhancedHudPanels::powerOutline( state, EnhancedHudPanels::interactionOf( hover, down ) ), c ), false, c );
				if( th.stateRails )
					TheDisplay->drawFillRect( plate.x + pk + 1, plate.y + plate.h - 3, plate.w - 2 * pk - 2, std::max<Int>( 1, P( 1 ) ), c.lit );
				break;
			case EnhancedHudPanels::PowerState::NeedsPoints:
				// prerequisites met but no General Point: engaged outline, flat plate, a dim pending lamp
				drawBevelPlate( plate, pk, CM_ALL, themeColor( th.controlLow ), outlineColor( EnhancedHudPanels::powerOutline( state, EnhancedHudPanels::interactionOf( hover, down ) ), c ), false, c );
				drawLedGroup( plate.x + plate.w - P( 6 ), plate.y + P( 2 ), 1, 0, c );
				break;
			default:
				drawRecess( plate, c, P( 1.2 ) );        // locked by rank or by a missing upgrade step
				break;
		}
		if( EnhancedHudPanels::inChain( i, count, isPrerequisite ) )
		{
			static const wchar_t *const roman[ 5 ] = { L"I", L"II", L"III", L"IV", L"V" };
			const int depth = std::min( 5, EnhancedHudPanels::chainDepth( i, count, isPrerequisite ) );
			char key[ 16 ];
			std::snprintf( key, sizeof( key ), "pop.n%d", i );
			UnicodeString numeral;
			numeral.format( L"%ls", roman[ depth - 1 ] );
			drawLabel( key, numeral, plate.x + P( 5 ), plate.y + P( 2 ), P( 8 ), TRUE,
			           state == EnhancedHudPanels::PowerState::Purchased ? c.lit : ( state == EnhancedHudPanels::PowerState::Available ? c.icon : c.iconDim ), 0, P( 22 ) );
		}
	}
	// detail strip (5 star foot, between the power and Done): name, cost and requirements of the hovered power - real data only
	{
		const HRect strip = place( HRect{ 100, 442, 224, 38 } );
		drawRecess( strip, c, P( 1.2 ) );
		if( hovered >= 0 && TheScienceStore )
		{
			UnicodeString name, description;
			if( TheScienceStore->getNameAndDescription( plates[ hovered ].st, name, description ) )
				drawLabel( "pop.dn", name, strip.x + P( 6 ), strip.y + P( 3 ), P( 10 ), TRUE, c.icon, 0, strip.w - P( 12 ) );
			UnicodeString line;
			line.format( L"%ls %d", TheGameText->fetchOrSubstitute( "GUI:EnhancedCost", L"COST" ).str(), TheScienceStore->getSciencePurchaseCost( plates[ hovered ].st ) );
			for( int a = 0; a < count; ++a )
				if( a != hovered && isPrerequisite( a, hovered ) )
				{
					UnicodeString pre, preDesc;
					if( TheScienceStore->getNameAndDescription( plates[ a ].st, pre, preDesc ) )
					{
						line.concat( L"   " );
						line.concat( TheGameText->fetchOrSubstitute( "GUI:EnhancedRequires", L"REQUIRES" ) );
						line.concat( L" " );
						line.concat( pre );
					}
				}
			int tier = 1;
			if( plates[ hovered ].rect.y < tiers[ 1 ].y ) tier = 0;
			else if( plates[ hovered ].rect.y >= tiers[ 2 ].y ) tier = 2;
			UnicodeString rankPart;
			rankPart.format( L"   %ls %d", TheGameText->fetchOrSubstitute( "GUI:EnhancedRank", L"RANK" ).str(), EnhancedHudPanels::popupRequiredRank( tier, levels ) );
			line.concat( rankPart );
			drawLabel( "pop.dl", line, strip.x + P( 6 ), strip.y + P( 20 ), P( 8 ), FALSE, c.iconDim, 0, strip.w - P( 12 ) );
		}
	}
	// Done sits in the console's foot
	if( exitButton && !exitButton->winIsHidden() )
	{
		const HRect b = windowRect( exitButton );
		bool hover, down;
		pointerState( b, &hover, &down );
		drawBevelPlate( inflate( b, P( 2 ) ), pk, CM_ALL, themeColor( th.control ), hover ? c.accent : c.shadow, down, c );
	}
}

// ---- Battle Overview (Diplomacy.wnd) ---------------------------------------------------------------------------------------
GameWindow *s_overviewRoot = nullptr;
GameWinDrawFunc s_overviewDraw = nullptr;
struct WindowArt { const Image *enabled; const Image *disabled; };
std::map<GameWindow *, WindowArt> s_overviewArt;

void overviewDraw( GameWindow *window, WinInstanceData * )
{
	if( !EnhancedHud::active() || !TheDisplay || !window )
		return;
	const HudPalette c = makePalette();
	const EnhancedHudTheme::Theme &th = *c.theme;
	const HRect r = windowRect( window );
	const double u = r.w / (double)EnhancedHudPanels::kOverviewW;
	s_drawScale = std::max( 0.5, u );
	const Int k = th.windowCorner == EnhancedHudTheme::Corner::Chamfer ? P( th.windowCornerRef ) : 0;
	fillChassis( r, k, c );
	edgeChamfered( r, k, CM_TOP, c.shadow );
	TheDisplay->drawLine( r.x + k + 1, r.y + 1, r.x + r.w - k - 2, r.y + 1, 1, c.highlight );
	if( th.cornerCaps )
		drawWindowCaps( r, c );

	// header band: title, map, elapsed game time (both shared by every player)
	const HRect hs = EnhancedHudPanels::popupScaled( EnhancedHudPanels::kOverviewHeader, u );
	const HRect header = { r.x + P( 8 ), r.y + P( 6 ), r.w - P( 16 ), std::max<Int>( P( 22 ), hs.h - P( 8 ) ) };
	drawRecess( header, c, P( 1.5 ) );
	TheDisplay->drawFillRect( header.x + 1, header.y + 1, std::max<Int>( 2, P( 1.5 ) ), header.h - 2, c.accent );
	const Int ty = header.y + ( header.h - P( 12 ) ) / 2 - 1;
	drawLabel( "ov.title", TheGameText->fetchOrSubstitute( "GUI:BattleOverview", L"BATTLE OVERVIEW" ), header.x + P( 10 ), ty, P( 12 ), TRUE, c.icon, 0 );
	wchar_t timeText[ 24 ], mapText[ 96 ];
	EnhancedHudPanels::formatGameTime( TheGameLogic ? TheGameLogic->getFrame() : 0, LOGICFRAMES_PER_SECOND, timeText, 24 );
	UnicodeString time;
	time.format( L"%ls", timeText );
	drawLabel( "ov.time", time, header.x + header.w - P( 8 ), ty, P( 12 ), TRUE, c.ready, 2 );
	mapText[ 0 ] = 0;
	if( TheGameInfo )
		EnhancedHudPanels::mapDisplayName( TheGameInfo->getMap().str(), mapText, 96 );
	if( mapText[ 0 ] )
	{
		UnicodeString map;
		map.format( L"%ls", mapText );
		drawLabel( "ov.map", map, header.x + header.w / 2, ty + 1, P( 10 ), FALSE, c.iconDim, 1 );
	}

	// tab strip: only tabs whose data exists are drawn (today: PLAYERS)
	{
		int n = 0;
		Int tx = r.x + P( 12 );
		const Int tabY = r.y + hs.h + P( 2 );
		for( const EnhancedHudPanels::OverviewTabInfo *tab = EnhancedHudPanels::overviewTabs( n ); n-- > 0; ++tab )
		{
			if( !tab->available )
				continue;
			UnicodeString label;
			label.translate( AsciiString( tab->label ) );
			const HRect tabRect = { tx, tabY, P( 72 ), P( 17 ) };
			drawBevelPlate( tabRect, P( 3 ), CM_TOP, themeColor( th.control ), c.shadow, false, c );
			TheDisplay->drawFillRect( tabRect.x + P( 4 ), tabRect.y + tabRect.h - 3, tabRect.w - P( 8 ), std::max<Int>( 1, P( 1 ) ), c.ready );
			drawLabel( "ov.tab", label, tabRect.x + tabRect.w / 2, tabRect.y + P( 3 ), P( 9 ), TRUE, c.icon, 1 );
			tx += tabRect.w + P( 4 );
		}
	}

	// player table module and a plate behind every visible row
	const HRect ts = EnhancedHudPanels::popupScaled( EnhancedHudPanels::kOverviewTable, u );
	drawModule( HRect{ r.x + ts.x + P( 4 ), r.y + ts.y + P( 12 ), ts.w - P( 8 ), ts.h - P( 8 ) }, c, -1 );
	const Int rowPitch = (Int)( EnhancedHudPanels::kOverviewRowPitch * u + 0.5 );
	for( int row = 0; row < EnhancedHudPanels::kOverviewRows; ++row )
	{
		AsciiString name;
		name.format( "Diplomacy.wnd:StaticTextPlayer%d", row );
		GameWindow *player = childByName( window, name.str() );
		if( !player || player->winIsHidden() )
			continue;
		const HRect pr = windowRect( player );
		const HRect strip = { r.x + ts.x + P( 10 ), pr.y - P( 3 ), ts.w - P( 20 ), rowPitch - P( 4 ) };
		drawRecess( strip, c, P( 1 ) );
		if( th.stateRails )
			TheDisplay->drawFillRect( strip.x + 1, strip.y + 2, std::max<Int>( 2, P( 1.5 ) ), strip.h - 4, row % 2 == 0 ? c.accentDim : c.outline );
	}
	// footer: the retail Done / hide button gets a raised housing
	if( GameWindow *hide = childByName( window, "Diplomacy.wnd:ButtonHide" ) )
		if( !hide->winIsHidden() )
		{
			const HRect b = windowRect( hide );
			bool hover, down;
			pointerState( b, &hover, &down );
			drawBevelPlate( inflate( b, P( 2 ) ), P( 3 ), CM_ALL, themeColor( th.control ), hover ? c.accent : c.shadow, down, c );
		}
}

}  // namespace

//-----------------------------------------------------------------------------
void EnhancedHudOverview::attach( GameWindow *root )
{
	if( !root )
		return;
	if( !EnhancedHud::active() )
	{
		forget( root, TRUE );
		return;
	}
	if( s_overviewRoot == root )
		return;
	forget( s_overviewRoot, FALSE );
	s_overviewRoot = root;
	s_overviewDraw = root->winGetDrawFunc();
	s_overviewArt.clear();
	// the retail background and the column-title bars are bitmaps; the skin replaces them (restored for Original)
	std::vector<GameWindow *> stack( 1, root );
	while( !stack.empty() )
	{
		GameWindow *w = stack.back();
		stack.pop_back();
		const UnsignedInt style = w->winGetStyle();
		if( w == root || BitIsSet( style, GWS_STATIC_TEXT ) || ( style & GWS_USER_WINDOW ) )
		{
			WindowArt art = { w->winGetEnabledImage( 0 ), w->winGetDisabledImage( 0 ) };
			s_overviewArt[ w ] = art;
			w->winSetEnabledImage( 0, nullptr );
			w->winSetDisabledImage( 0, nullptr );
		}
		for( GameWindow *ch = w->winGetChild(); ch; ch = ch->winGetNext() )
			stack.push_back( ch );
	}
	root->winSetDrawFunc( overviewDraw );
}

void EnhancedHudOverview::forget( GameWindow *root, Bool restore )
{
	if( !root || root != s_overviewRoot )
		return;
	if( restore )
	{
		for( std::map<GameWindow *, WindowArt>::const_iterator it = s_overviewArt.begin(); it != s_overviewArt.end(); ++it )
		{
			it->first->winSetEnabledImage( 0, it->second.enabled );
			it->first->winSetDisabledImage( 0, it->second.disabled );
		}
		if( s_overviewDraw )
			root->winSetDrawFunc( s_overviewDraw );
	}
	s_overviewArt.clear();
	s_overviewRoot = nullptr;
	s_overviewDraw = nullptr;
}

Bool EnhancedHudOverview::showPlayerTable()
{
	// Enhanced shows the player table in skirmish too (retail shows the briefing list there): the data is the slot list every
	// player already saw in the setup screen plus the alive/defeated status the retail list shows in multiplayer
	return EnhancedHud::active() && TheGameLogic && TheGameLogic->getGameMode() == GAME_SKIRMISH && TheGameInfo ? TRUE : FALSE;
}
