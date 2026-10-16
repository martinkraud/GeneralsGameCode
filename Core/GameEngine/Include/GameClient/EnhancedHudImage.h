// SPDX-License-Identifier: GPL-3.0-or-later
// Enhanced gameplay HUD: draw-time image fitting for reused retail bitmaps (see Common/EnhancedHudImageFit.h).
// Every entry point is a no-op unless the Enhanced HUD is active AND the window was registered by the Enhanced layout,
// so the Original control bar draws exactly as before.
#pragma once

#include "Lib/BaseType.h"

class GameWindow;
class Image;

namespace EnhancedHudImage
{
void clear();
void registerAspectFit( GameWindow *window );
void registerGauge( GameWindow *window );

// Push button / default image draw: narrows [start,end] to the aspect-fitted, centred rectangle of `image`.
void fitImage( GameWindow *window, const Image *image, ICoord2D *start, ICoord2D *end );

// Gauge draw: uniform tile scale (window cross extent / native tile cross extent); 1.0 for Original or unregistered windows.
Real gaugeScale( GameWindow *window, Int windowCross, Int imageCross );

// Power meter: segment separators drawn over the gauge (no-op for Original / other windows).
void drawGaugeSegments( GameWindow *window, Int x, Int y, Int w, Int h );
}
