// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Texture2D.h"
#include "WWLib/refcount.h"

// Owned single-mip CPU storage. The original sentence constructor remains
// A4R4G4B4; the explicit-format overload also supports bounded UI content.
class Texture2DCPUContent : public RefCountClass
{
    W3DMPO_CODE(Texture2DCPUContent)
public:
    struct SurfaceDescription { WW3DFormat Format; unsigned Width, Height; };
    Texture2DCPUContent(unsigned width, unsigned height, int row_pitch = 0);
    Texture2DCPUContent(unsigned width, unsigned height, WW3DFormat format, int row_pitch);
    ~Texture2DCPUContent() override;
    Texture2DCPUContent(const Texture2DCPUContent &) = delete;
    Texture2DCPUContent &operator=(const Texture2DCPUContent &) = delete;

    void Get_Description(SurfaceDescription &out) const;
    Texture2DDescription Describe() const;
    void *Lock(int *row_pitch);
    void Unlock();
    Texture2DContent *Map(const Texture2DRegion *region = nullptr);
    Texture2DCPUContent *Snapshot() const;
    void Copy_Rows(void *destination, int destination_pitch) const;
    const unsigned char *Pixels() const;
    int Row_Pitch() const { return pitch; }
    unsigned Storage_Rows() const { return rows; }
    unsigned Row_Bytes() const { return row_bytes; }

private:
    unsigned width, height;
    WW3DFormat format;
    unsigned rows, row_bytes;
    int pitch;
    unsigned char *pixels;
    bool locked = false;
};
