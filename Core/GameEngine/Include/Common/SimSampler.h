// SPDX-License-Identifier: GPL-3.0-or-later
// Opt-in statistical profiler of the headless replay benchmark (GENERALS_SIM_SAMPLE=<output file>). See SimSampler.cpp. Inert unless started.
#pragma once

namespace SimSampler
{
bool start(const char *outputFile);   ///< samples the calling thread until stopAndReport(); false when no file was given
void stopAndReport();                 ///< stops, symbolises (dbghelp + PDB) and writes the table of functions by sample share
}
