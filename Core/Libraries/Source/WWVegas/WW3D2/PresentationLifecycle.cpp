// SPDX-License-Identifier: GPL-3.0-or-later
#include "PresentationLifecycle.h"
#include "IRenderBackend.h"
#include <stdexcept>
#include <utility>
PresentationLifecycle::PresentationLifecycle(std::unique_ptr<IRenderBackend> value, PresentationSize dimensions, StatusReader reader)
    :backend(std::move(value)),size(dimensions),readStatus(reader) {
    if(!backend || !readStatus || !size.width || !size.height || size.width>16384 || size.height>16384)
        throw std::runtime_error("Invalid presentation lifecycle configuration");
}
PresentationLifecycle::~PresentationLifecycle()=default;
IRenderBackend *PresentationLifecycle::Backend() const {return backend.get();}
PresentationSize PresentationLifecycle::Size() const {return size;}
PresentationStatus PresentationLifecycle::Status() const {return readStatus(*backend);}
unsigned PresentationLifecycle::Frames() const {return frames;}
void PresentationLifecycle::Resize(PresentationSize dimensions) {
    if(active || !dimensions.width || !dimensions.height || dimensions.width>16384 || dimensions.height>16384)
        throw std::runtime_error("Invalid presentation resize");
    size=dimensions;
}
void PresentationLifecycle::Begin(bool clear,bool depth,const Vector3 &color,float alpha) {
    if(active || depth) throw std::runtime_error("Presentation lifecycle requires paired color-only frames");
    backend->Set_Viewport({0,0,size.width,size.height,0,1});
    backend->Clear(clear,false,color,alpha);
    backend->Begin_Scene();
    active=true;
}
void PresentationLifecycle::End(bool present) {
    if(!active) throw std::runtime_error("Presentation End without Begin");
    // End_Scene relinquishes the backend scene even if Present fails. This
    // scope can always destroy its device; it never retries a failed frame.
    active=false;
    backend->End_Scene(present);
    backend->Invalidate_Cached_Render_States();
    ++frames;
}
