// SPDX-License-Identifier: GPL-3.0-or-later
// Developer Preview distribution: the ONE place the release designation lives in the engine. scripts/distribution/identity.json carries the same
// string for the package builder; a source contract test keeps the two equal. Shown nowhere in the Modern UI (the footer text stays as is); it goes
// to the log (checkpoint "version") so a log or a bug report says which preview produced it. Not an official-release claim.
#pragma once

namespace ReleaseIdentity
{
constexpr const char *kDesignation = "Developer Preview 1";
}
