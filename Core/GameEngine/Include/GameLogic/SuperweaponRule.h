// SPDX-License-Identifier: GPL-3.0-or-later
// X64.52A: engine side of the Superweapons match rule (pure rules: Common/SuperweaponLimit.h). Shared by Generals and Zero Hour.
#pragma once

class Player;
class ThingTemplate;

namespace SuperweaponRule
{
/// The number of superweapon structures the player owns right now: completed, under construction, captured, and standing GLA rebuild holes of one (the hole
/// holds the capacity of the structure it will rebuild). Dead objects are not counted, so a destroyed or sold structure frees its capacity at once.
unsigned countOwned(const Player *player);

/// True when the active match rule forbids `player` to START `what` right now. Always false when the rule is Unlimited (the default), when `what` is not a superweapon
/// structure, in Campaign and in General's Challenge. This is the authoritative check: human construction, the build command buttons and the AI all go through it.
bool blocksStart(const Player *player, const ThingTemplate *what);
}
