// SPDX-License-Identifier: GPL-3.0-or-later
// X64.38: determinism fingerprint of the authoritative simulation (pure, engine-free).
//
// A fingerprint is an ORDERED list of checkpoints taken at fixed logic frames (never at wall-clock moments and never in worker
// completion order). Each checkpoint carries the logic frame, the engine's own state CRC (GameLogic::getCRC: every object snapshot,
// the logic RNG seed, the partition manager, the player list and the AI) and the live object count. Two runs of the same replay are
// deterministic exactly when their fingerprints are equal; the first differing checkpoint localises a divergence to an interval of
// frames. The digest is a 64-bit FNV-1a over the fixed-width little-endian encoding of the checkpoints, so it is identical on every
// platform and independent of container or locale behaviour.
#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace DeterminismFingerprint
{
struct Checkpoint
{
	std::uint32_t frame = 0;
	std::uint32_t crc = 0;          // GameLogic::getCRC(CRC_RECALC) at that frame
	std::uint32_t objects = 0;      // live object count at that frame
	bool operator==(const Checkpoint &o) const { return frame == o.frame && crc == o.crc && objects == o.objects; }
	bool operator!=(const Checkpoint &o) const { return !(*this == o); }
};

constexpr std::uint64_t kFnvOffset = 14695981039346656037ull;
constexpr std::uint64_t kFnvPrime = 1099511628211ull;

inline std::uint64_t mix(std::uint64_t hash, std::uint32_t value)
{
	for (int i = 0; i < 4; ++i)
	{
		hash ^= static_cast<std::uint64_t>((value >> (8 * i)) & 0xFFu);
		hash *= kFnvPrime;
	}
	return hash;
}

inline std::uint64_t digest(const std::vector<Checkpoint> &trace)
{
	std::uint64_t h = kFnvOffset;
	for (const Checkpoint &c : trace)
	{
		h = mix(h, c.frame);
		h = mix(h, c.crc);
		h = mix(h, c.objects);
	}
	return h;
}

// Index of the first checkpoint that differs (or the shorter length when one trace is a prefix of the other); -1 when equal.
inline long long firstDivergence(const std::vector<Checkpoint> &a, const std::vector<Checkpoint> &b)
{
	const size_t common = a.size() < b.size() ? a.size() : b.size();
	for (size_t i = 0; i < common; ++i)
		if (a[i] != b[i]) return static_cast<long long>(i);
	return a.size() == b.size() ? -1 : static_cast<long long>(common);
}

inline std::string format(const Checkpoint &c)
{
	char buffer[96];
	std::snprintf(buffer, sizeof(buffer), "frame=%u crc=%08X objects=%u", c.frame, c.crc, c.objects);
	return buffer;
}

inline std::string formatDigest(std::uint64_t digestValue)
{
	char buffer[32];
	std::snprintf(buffer, sizeof(buffer), "%016llX", static_cast<unsigned long long>(digestValue));
	return buffer;
}
}  // namespace DeterminismFingerprint
