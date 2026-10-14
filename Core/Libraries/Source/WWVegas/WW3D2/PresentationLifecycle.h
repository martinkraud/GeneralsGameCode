// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <memory>
class IRenderBackend;
class Vector3;
struct PresentationStatus { unsigned result=0, removed_reason=0; bool occluded=false; };
struct PresentationSize { unsigned width, height; };
struct PresentationConfiguration {
    void *window=nullptr;
    unsigned width=800,height=600;
    bool vsync=false; // X64.33: present with sync interval 1 (Options > VSync)
    void (*diagnostic)(void *,const char *,const char *,const char *,unsigned)=nullptr;
    void *context=nullptr;
};
// Owns one configured, fixed-size presentation backend. No engine/world updates,
// legacy device reset/sorting/texture-loader work, or implicit backend switching.
class PresentationLifecycle {
public:
    using StatusReader = PresentationStatus (*)(IRenderBackend &);
    PresentationLifecycle(std::unique_ptr<IRenderBackend>, PresentationSize, StatusReader);
    ~PresentationLifecycle();
    PresentationLifecycle(const PresentationLifecycle &)=delete;
    PresentationLifecycle &operator=(const PresentationLifecycle &)=delete;
    IRenderBackend *Backend() const;
    PresentationSize Size() const;
    PresentationStatus Status() const;
    void Begin(bool clear, bool depth, const Vector3 &, float alpha);
    void End(bool present);
    unsigned Frames() const;
    void Resize(PresentationSize dimensions); // X64.33: after the backend swap chain was resized
private:
    std::unique_ptr<IRenderBackend> backend;
    PresentationSize size;
    StatusReader readStatus;
    bool active=false;
    unsigned frames=0;
};
