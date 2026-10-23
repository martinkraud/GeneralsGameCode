// SPDX-License-Identifier: GPL-3.0-or-later
// X64.52: pure, deterministic building blocks of the Enhanced AI "Dynamic Base & Defence" (Original never calls any of this).
//
// Everything here is integer / hash based, free of engine state, clocks and the simulation RNG: the Enhanced decisions are a pure function of
//   (the synchronised match seed, the player index, the observed game state),
// so a replay or a network peer with the same seed and inputs reproduces them exactly. No random draws are consumed from the game logic RNG.
#pragma once

#include <cmath>
#include <cstdint>

namespace AIEnhancedBase
{
// ------------------------------------------------------------------------------------------------
// private decision stream: a stateless hash (SplitMix32 finalizer). pick(seed, salt, n) is uniform enough for n << 2^16.
inline uint32_t mix32(uint32_t x)
{
	x += 0x9E3779B9u;
	x = (x ^ (x >> 16)) * 0x85EBCA6Bu;
	x = (x ^ (x >> 13)) * 0xC2B2AE35u;
	return x ^ (x >> 16);
}
inline uint32_t hash2(uint32_t a, uint32_t b) { return mix32(a ^ mix32(b + 0x7F4A7C15u)); }
inline uint32_t playerSeed(uint32_t matchSeed, int playerIndex) { return hash2(matchSeed, 0xA15EEDu + (uint32_t)playerIndex * 7919u); }
inline int pick(uint32_t seed, uint32_t salt, int n) { return n <= 1 ? 0 : (int)(hash2(seed, salt) % (uint32_t)n); }
// [0, 1) from the hash, 16 bit resolution
inline float unit(uint32_t seed, uint32_t salt) { return (float)(hash2(seed, salt) & 0xFFFFu) / 65536.0f; }

// ------------------------------------------------------------------------------------------------
// Layout variants: the retail build list is rotated about the Command Center by an angle that depends on the start position; Enhanced adds one of a few
// bounded extra rotations, chosen among the variants that keep (nearly) as many build slots legal as the best one.
constexpr int kLayoutVariants = 5;
constexpr float kPi = 3.14159265358979f;
inline float layoutVariantOffset(int variant)
{
	static const float offsets[kLayoutVariants] = { 0.0f, -kPi / 16.0f, kPi / 16.0f, -kPi / 8.0f, kPi / 8.0f };   // 0, -11.25, +11.25, -22.5, +22.5 degrees
	return (variant >= 0 && variant < kLayoutVariants) ? offsets[variant] : 0.0f;
}
// legalCount[v] = number of build slots that are legal under variant v (-1 = variant not evaluated). Returns the chosen variant.
inline int chooseLayoutVariant(uint32_t seed, const int legalCount[kLayoutVariants], int tolerance)
{
	int best = -1;
	for (int v = 0; v < kLayoutVariants; ++v) if (legalCount[v] > best) best = legalCount[v];
	if (best < 0) return 0;
	int eligible[kLayoutVariants], n = 0;
	for (int v = 0; v < kLayoutVariants; ++v) if (legalCount[v] >= 0 && legalCount[v] >= best - tolerance) eligible[n++] = v;
	return eligible[pick(seed, 0x1A70u, n)];
}

// Slot jitter: how far a non-critical slot may move without touching its neighbours. slack = distance to the nearest other slot minus both footprint radii.
inline float jitterAllowance(float slack, float maxAllowance)
{
	const float quarter = slack * 0.25f;   // two neighbours that both move towards each other use at most half of the original gap
	return quarter <= 0.0f ? 0.0f : (quarter < maxAllowance ? quarter : maxAllowance);
}
inline void jitterOffset(uint32_t seed, uint32_t slotKey, float allowance, float *dx, float *dy)
{
	if (allowance <= 0.0f) { *dx = 0.0f; *dy = 0.0f; return; }
	const float angle = 2.0f * kPi * unit(seed, slotKey * 2u + 1u);
	const float length = allowance * (0.5f + 0.5f * unit(seed, slotKey * 2u + 2u));
	*dx = std::cos(angle) * length;
	*dy = std::sin(angle) * length;
}

// ------------------------------------------------------------------------------------------------
// Threat memory: where damage to this player's base structures came from. 16 sectors around the base center; integer weights, integer decay.
struct ThreatMap
{
	static constexpr int kSectors = 16;
	int32_t weight[kSectors];                // fixed point: one damage observation = kEvent
	static constexpr int kEvent = 256;
	static constexpr int kDecayShift = 8;    // per decay step the weight loses weight / 256 (+1): half-life of about 177 steps (steps are one second)
	ThreatMap() { clear(); }
	void clear() { for (int i = 0; i < kSectors; ++i) weight[i] = 0; }
	static int sectorOf(float dx, float dy)
	{
		float a = std::atan2(dy, dx);                       // (-pi, pi]
		if (a < 0.0f) a += 2.0f * kPi;
		int s = (int)(a * (kSectors / (2.0f * kPi)));
		return s >= kSectors ? kSectors - 1 : (s < 0 ? 0 : s);
	}
	static float sectorCenterAngle(int s) { return (s + 0.5f) * (2.0f * kPi / kSectors); }
	void add(int sector, int n = 1) { if (sector >= 0 && sector < kSectors) { weight[sector] += kEvent * n; if (weight[sector] > 60 * kEvent) weight[sector] = 60 * kEvent; } }
	void decay() { for (int i = 0; i < kSectors; ++i) if (weight[i] > 0) { weight[i] -= (weight[i] >> kDecayShift) + 1; if (weight[i] < 0) weight[i] = 0; } }
	// dominant sector (smoothed with its two neighbours), or -1 when below `minEvents` observations
	int dominant(int minEvents) const
	{
		int bestSector = -1, bestScore = 0;
		for (int s = 0; s < kSectors; ++s)
		{
			const int score = weight[s] * 2 + weight[(s + 1) % kSectors] + weight[(s + kSectors - 1) % kSectors];
			if (score > bestScore) { bestScore = score; bestSector = s; }
		}
		return bestScore >= minEvents * kEvent * 2 ? bestSector : -1;   // smoothed score: the sector counts twice, its neighbours once (n events in one sector score 2n, spread evenly over three 1.33n)
	}
	int total() const { int t = 0; for (int i = 0; i < kSectors; ++i) t += weight[i]; return t; }
};

// ------------------------------------------------------------------------------------------------
// Persistent per-player state of the Enhanced base policy (xfer'd by AIPlayer, version 3).
struct State
{
	uint32_t seed = 0;                // playerSeed(match seed, player index)
	int32_t  layoutVariant = 0;
	int32_t  recoveryGrants = 0;      // extra rebuilds granted to critical structures
	int32_t  threatDefenses = 0;      // autonomous threat-sector defences ordered
	int32_t  expansions = 0;          // supply expansions ordered
	int32_t  expansionFailures = 0;
	int32_t  arcSector = -1;          // threat sector the front defence arc is currently centred on
	uint32_t lastThreatScan = 0;      // frame of the last damage scan
	uint32_t lastThreatDefense = 0;
	uint32_t lastExpansion = 0;
	uint32_t lastRecovery = 0;
	// Placement failures: a build slot whose whole fallback search found nothing is not searched again for a while (the retail search is the most expensive thing
	// the base AI does: hundreds of legality checks, measured at 100-350 ms when it comes up empty).
	static constexpr int kFailSlots = 8;
	uint32_t failKey[kFailSlots] = {};
	uint32_t failFrame[kFailSlots] = {};
	int32_t  failNext = 0;
	uint32_t stuckKey = 0;            // the priority order that could not be started at its last attempt
	uint32_t stuckSince = 0;          // ... and since when
	ThreatMap threat;
};

constexpr unsigned kPlacementFailureWindow = 30 * 60 * 3;   // 3 simulated minutes
constexpr unsigned kStuckOrderFrames = 30 * 45;              // a priority order that cannot be started for 45 s steps aside (the retail loop only ever looks at the FIRST priority order)
inline uint32_t fnv1a(const char *text) { uint32_t h = 2166136261u; while (text && *text) { h ^= (uint8_t)*text++; h *= 16777619u; } return h; }
inline uint32_t placementKey(const char *templateName, float x, float y)
{
	return hash2(fnv1a(templateName), hash2((uint32_t)(int)(x * 0.5f) * 73856093u, (uint32_t)(int)(y * 0.5f) * 19349663u));
}
inline bool placementRecentlyFailed(const State &s, uint32_t key, unsigned frame)
{
	for (int i = 0; i < State::kFailSlots; ++i)
		if (s.failKey[i] == key && s.failFrame[i] != 0 && frame < s.failFrame[i] + kPlacementFailureWindow) return true;
	return false;
}
inline void notePlacementFailure(State &s, uint32_t key, unsigned frame)
{
	for (int i = 0; i < State::kFailSlots; ++i) if (s.failKey[i] == key) { s.failFrame[i] = frame ? frame : 1u; return; }
	s.failKey[s.failNext] = key;
	s.failFrame[s.failNext] = frame ? frame : 1u;
	s.failNext = (s.failNext + 1) % State::kFailSlots;
}

// Caps / cadence (logic frames at 30 per second); constants live here so tests and the engine agree.
constexpr int kRecoveryGrantCap = 24;
constexpr int kThreatDefenseCap = 6;
constexpr int kExpansionCap = 3;
constexpr int kExpansionFailureCap = 3;
constexpr unsigned kScanInterval = 30;                 // damage scan: once per second
constexpr unsigned kThreatDefenseCooldown = 30 * 75;   // at most one autonomous defence per 75 s
constexpr unsigned kExpansionCooldown = 30 * 90;
constexpr unsigned kExpansionStart = 30 * 240;         // not before minute 4: the scripted opening builds the first economy
constexpr unsigned kRecoveryCooldown = 30 * 20;
constexpr unsigned kFillInStart = 30 * 60 * 5;           // the scripted opening (and its start-of-match cash) is left alone for the first five minutes
constexpr int kThreatMinEvents = 6;                    // observations (smoothed) before the base reacts at all
constexpr int kThreatDefenseMinEvents = 14;            // observations before an autonomous defence is ordered
constexpr float kArcRecenterRadians = 0.7854f;         // re-centre the front arc when the threat moved by more than 45 degrees

inline float angleDifference(float a, float b)
{
	float d = a - b;
	while (d > kPi) d -= 2.0f * kPi;
	while (d < -kPi) d += 2.0f * kPi;
	return d < 0.0f ? -d : d;
}

// Rebuild policy: a critical structure whose retail rebuild allowance is spent may be granted one more rebuild while the AI can afford it comfortably.
inline bool recoveryAllowed(bool critical, bool slotHasRebuilds, int money, int cost, int wealthyThreshold, int grantsUsed, unsigned frame, unsigned lastRecovery)
{
	if (!critical || slotHasRebuilds) return false;
	if (grantsUsed >= kRecoveryGrantCap) return false;
	if (frame < lastRecovery + kRecoveryCooldown) return false;
	return money >= cost + (wealthyThreshold >> 1);   // the structure itself plus half the "wealthy" reserve
}

// Supply expansion: how many supply centers the AI should aim for by `frame`
inline int desiredSupplyCenters(unsigned frame) { return frame < 30 * 60 * 10 ? 2 : 3; }
}
