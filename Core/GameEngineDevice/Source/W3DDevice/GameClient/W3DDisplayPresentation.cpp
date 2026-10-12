// SPDX-License-Identifier: GPL-3.0-or-later
#if defined(_WIN64)
#include "W3DDevice/GameClient/W3DDisplay.h"
#include "Common/GlobalData.h"
#include "Common/Errors.h"
#include "Common/StartupDiagnostics.h"
#include "WW3D2/ww3d.h"
#include "WW3D2/Backend/NativeGraphicsSmoke.h"
#include "GameClient/Image.h"
#include "WW3D2/render2d.h"
#include "WW3D2/render2dsentence.h"
#include "GameClient/GameText.h"
#include "GameClient/GameWindowManager.h"
#include "WW3D2/texture.h"
#include "WW3D2/Texture2DCPUContent.h"
#include <stdexcept>
#include "W3DDevice/GameClient/NativeMenuComposition.h"
#endif
#if defined(_WIN64)
// The retained display borrows the checked platform host HWND. WW3D owns the
// attached device. Headless subsystem selection is unchanged; no ordinary draw,
// engine update, shell or world rendering is reachable through this gate.
int W3DDisplay::runNativePresentationSmoke()
{
    if (!m_initialized || !TheGlobalData->m_headless || m_2DRender) {
        StartupDiagnostics::checkpoint("display_attach", "fail",
            "Native display presentation requires initialized headless W3DDisplay", 1);
        return 1; // Keep normal GameMain engine cleanup on a rejected attachment.
    }
    struct Driver final : NativeGraphicsSmoke::DisplayDriver {
        W3DDisplay &display;
        Int width, height, bits;
        Bool windowed;
        bool attached = false, firstBegin = true, firstEnd = true, firstImage = true;
        const bool imageMode = StartupDiagnostics::uiImageSmoke();
        const bool sentenceMode = StartupDiagnostics::uiSentenceSmoke();
        const bool previousBias = WW3D::Is_Screen_UV_Biased();
        FontCharsClass *font = nullptr;
        Render2DSentenceClass *sentence = nullptr;
        bool firstSentence = true, firstPanel = true;
        const bool panelMode = StartupDiagnostics::uiPanelSmoke();
        NativeMenuComposition *menu = nullptr;
        const bool menuMode = StartupDiagnostics::uiMenuSmoke();
        TextureClass *texture = nullptr;
        Image *image = nullptr;
        RectClass previousScreen = Render2DClass::Get_Screen_Resolution();
        explicit Driver(W3DDisplay &value) : display(value), width(value.getWidth()),
            height(value.getHeight()), bits(value.getBitDepth()), windowed(value.getWindowed()) {}
        static void Log(void *, const char *stage, const char *status, const char *detail, unsigned code) {
            StartupDiagnostics::checkpoint(stage, status, detail, code);
        }
        void Initialize(void *window) override {
            Log(nullptr,"display_attach","begin","",0);
            PresentationConfiguration config; config.window = window; config.diagnostic = Log;
            WW3D::Init_Native_Presentation(config); attached = true;
            // The graphical setter dereferences the headless-null Render2D helper.
            display.Display::setWidth(800); display.Display::setHeight(600);
            display.setBitDepth(32); display.setWindowed(true);
            int w,h,b; bool windowed;
            WW3D::Get_Device_Resolution(w,h,b,windowed);
            if(w!=800 || h!=600 || b!=32 || !windowed)
                throw std::runtime_error("Native display/device dimensions disagree");
            WW3D::Get_Render_Target_Resolution(w,h,b,windowed);
            if(w!=800 || h!=600 || b!=32)
                throw std::runtime_error("Native display/target dimensions disagree");
            Log(nullptr,"display_size","complete","device=800x600 target=800x600 bits=32 windowed=1",0);
            Log(nullptr,"display_attach","complete","W3DDisplay -> WW3D active backend; presentation-only",0);
            if(sentenceMode || menuMode) {
                // Match the original graphical UI producer's pixel-center policy.
                WW3D::Set_Screen_UV_Bias(true);
                Log(nullptr,"ui_pixel_policy","complete","legacy UV bias enabled; D3D11 half-pixel correction",0);
            }
            if(imageMode) {
                Log(nullptr,"ui_image_load","begin","GeneralsLogo",0);
                try {
                    const Image *mapped = TheMappedImageCollection ?
                        TheMappedImageCollection->findImageByName("GeneralsLogo") : nullptr;
                    if(!mapped || mapped->getStatus()!=IMAGE_STATUS_NONE ||
                        mapped->getFilename().isEmpty())
                        throw std::runtime_error("Expected ordinary GeneralsLogo mapped image unavailable");
                    Region2D uv = *mapped->getUV();
                    char detail[512];
                    sprintf_s(detail,"name=GeneralsLogo file=%s atlas=%dx%d uv=%.9g,%.9g,%.9g,%.9g rect=504,16,791,110",
                        mapped->getFilename().str(),mapped->getTextureSize()->x,mapped->getTextureSize()->y,
                        uv.lo.x,uv.lo.y,uv.hi.x,uv.hi.y);
                    Log(nullptr,"ui_image_metadata","complete",detail,0);
                    // Private ownership avoids leaving a device residency in the global
                    // asset cache after this short attachment. Original metadata is read-only.
                    Log(nullptr,"ui_texture_create","begin","TextureClass",0);
                    texture = new TextureClass(mapped->getFilename().str(), nullptr, MIP_LEVELS_1);
                    Log(nullptr,"ui_texture_create","complete","TextureClass",0);
                    Log(nullptr,"ui_texture_content","begin","ordinary single-mip file decode",0);
                    texture->Prepare_Render2D_Texture();
                    const auto *content = texture->Get_Render2D_CPU_Content();
                    if(!content || content->Describe().width != unsigned(mapped->getTextureSize()->x) ||
                        content->Describe().height != unsigned(mapped->getTextureSize()->y))
                        throw std::runtime_error("Menu atlas dimensions disagree with mapped metadata");
                    Log(nullptr,"ui_texture_content","complete","CPU content dimensions verified",0);
                    image = newInstance(Image);
                    image->setUV(&uv); image->setRawTextureData(texture);
                    image->setStatus(IMAGE_STATUS_RAW_TEXTURE);
                    Log(nullptr,"ui_render2d_create","begin","Render2DClass",0);
                    display.m_2DRender = new Render2DClass;
                    Render2DClass::Set_Screen_Resolution(RectClass(0,0,800,600));
                    display.m_2DRender->Set_Coordinate_Range(RectClass(0,0,800,600));
                    Log(nullptr,"ui_render2d_create","complete","800x600 coordinates/screen",0);
                    Log(nullptr,"ui_image_load","complete",detail,0);
                } catch(ErrorCode error) {
                    Log(nullptr,"ui_image_load","fail","engine ErrorCode",static_cast<unsigned>(error));
                    throw;
                }
            }
            if(menuMode) {
                display.m_2DRender = new Render2DClass;
                Render2DClass::Set_Screen_Resolution(RectClass(0,0,800,600));
                display.m_2DRender->Set_Coordinate_Range(RectClass(0,0,800,600));
                menu = new NativeMenuComposition;
                menu->Load();
            }
            if(sentenceMode) {
                try { PrepareSentence(window); }
                catch(ErrorCode error) {
                    Log(nullptr,"ui_sentence_load","fail","engine ErrorCode",static_cast<unsigned>(error));
                    throw;
                }
            }
        }
        void PrepareSentence(void *window) {
            Log(nullptr,"ui_sentence_load","begin","GUI:SelectDifficulty",0);
            if(!TheGameText) throw std::runtime_error("Original game text unavailable");
            Bool exists = FALSE;
            const UnicodeString text = TheGameText->fetch("GUI:SelectDifficulty", &exists);
            if(!exists || text.isEmpty()) throw std::runtime_error("Original menu label unavailable");
            // Original MainMenu.wnd StaticTextSelectDifficulty: Arial14, not a
            // replacement font or synthetic caption. Private producer ownership.
            Log(nullptr,"ui_font_create","begin","Arial size=14 bold=0",0);
            font = new FontCharsClass;
            if(!font->Initialize_GDI_Font("Arial",14,false,window))
                throw std::runtime_error("Original menu font GDI initialization failed");
            Log(nullptr,"ui_font_create","complete","Arial size=14 bold=0",0);
            sentence = new Render2DSentenceClass;
            sentence->Set_Font(font);
            sentence->Set_Clipping_Rect(RectClass(540,116,756,152));
            Log(nullptr,"ui_sentence_build","begin","Render2DSentenceClass::Build_Sentence",0);
            sentence->Build_Sentence(text.str(),nullptr,nullptr);
            const Vector2 extent = sentence->Get_Text_Extents(text.str());
            if(extent.X<=0 || extent.Y<=0 || extent.X>216 || extent.Y>36)
                throw std::runtime_error("Original label does not fit diagnostic menu rectangle");
            // Probe the sentence producer inside its original rectangle. This
            // does not construct the hidden widget or claim its layout state.
            sentence->Set_Location(Vector2(540,116));
            sentence->Draw_Sentence(0xffffbe01);
            const RectClass drawn = sentence->Get_Draw_Extents();
            if(drawn.Width()<=0 || drawn.Height()<=0)
                throw std::runtime_error("Original sentence generated no geometry");
            char detail[256];
            sprintf_s(detail,"label=GUI:SelectDifficulty font=Arial size=14 bold=0 rect=540,116,756,152 extent=%.9g,%.9g chars=%u",
                extent.X,extent.Y,static_cast<unsigned>(text.getLength()));
            Log(nullptr,"ui_sentence_build","complete",detail,0);
            Log(nullptr,"ui_sentence_load","complete",detail,0);
        }
        void BeginClear() override {
            if(WW3D::Begin_Render(true,false,Vector3(0.08f,0.32f,0.55f),1)!=WW3D_ERROR_OK)
                throw std::runtime_error("Native display Begin_Render failed");
            if(firstBegin) { firstBegin=false; Log(nullptr,"display_frame_begin","complete","WW3D::Begin_Render color-only",0); }
        }
        void EndPresent() override {
            if(menuMode) menu->Render();
            if(panelMode) {
                if(firstPanel) Log(nullptr,"ui_panel_draw","begin","original MapBorder2 primitive route",0);
                // Qualified base primitive delegation is real even on the
                // headless manager. Do not create a second manager: its destructor
                // owns global transition/static-string cleanup. No widget/parser
                // or layout callback is invoked here.
                if(!TheWindowManager) throw std::runtime_error("Missing UI primitive manager");
                TheWindowManager->GameWindowManager::winOpenRect(0xffffbb00,1,532,108,756,360);
                TheWindowManager->GameWindowManager::winFillRect(0x7e000000,1,533,109,755,359);
                if(firstPanel) {
                    firstPanel=false;
                    Log(nullptr,"ui_panel_draw","complete",
                        "MapBorder2 rect=532,108,756,360 border=FFFFBB00 fill=7E000000; GameWindowManager -> W3DDisplay -> Render2D -> D3D11 diffuse-alpha",0);
                }
            }
            if(imageMode) {
                if(firstImage) Log(nullptr,"ui_image_draw","begin","W3DDisplay::drawImage",0);
                display.drawImage(image,504,16,791,110,0xffffffff,Display::DRAW_IMAGE_ALPHA);
                if(firstImage) {
                    firstImage=false;
                    Log(nullptr,"ui_image_draw","complete",
                        "W3DDisplay::drawImage -> Render2D -> D3D11; original GeneralsLogo; alpha",0);
                }
            }
            if(sentenceMode) {
                if(firstSentence) Log(nullptr,"ui_sentence_draw","begin","actual sentence -> TextureClass -> Render2D -> D3D11",0);
                sentence->Render(); // snapshots pending glyph atlases, uploads and submits actual producer
                if(firstSentence) {
                    firstSentence=false;
                    Log(nullptr,"ui_sentence_draw","complete","CPU glyph atlas / TextureClass / GPU residency / Render2D submitted",0);
                }
            }
            if(WW3D::End_Render(true)!=WW3D_ERROR_OK)
                throw std::runtime_error("Native display End_Render failed");
            if(firstEnd) { firstEnd=false; Log(nullptr,"display_frame_end","complete","WW3D::End_Render Present",0); }
        }
        PresentationStatus Status() const noexcept override {
            return attached ? WW3D::Native_Presentation_Status() : PresentationStatus{};
        }
        void Shutdown() noexcept override {
            Log(nullptr,"display_detach","begin","",0);
            if(menuMode) {
                delete menu; menu=nullptr;
                delete display.m_2DRender;display.m_2DRender=nullptr;
                Render2DClass::Set_Screen_Resolution(previousScreen);
                WW3D::Set_Screen_UV_Bias(previousBias);
            }
            if(sentenceMode) {
                delete sentence; sentence=nullptr;
                if(font) { font->Release_Ref(); font=nullptr; }
                WW3D::Set_Screen_UV_Bias(previousBias);
                Log(nullptr,"ui_sentence_release","complete","sentence/font/CPU/GPU released before image/device detach; UV bias restored",0);
            }
            if(imageMode) {
                // Render2D retains its texture reference. Drop it before our owner,
                // then detach the device; no resource survives this attachment.
                delete display.m_2DRender; display.m_2DRender=nullptr;
                if(texture) { texture->Release_Ref(); texture=nullptr; }
                if(image) { deleteInstance(image); image=nullptr; }
                Render2DClass::Set_Screen_Resolution(previousScreen);
                Log(nullptr,"ui_image_release","complete","Render2D/texture released before device detach",0);
            }
            if(attached) { WW3D::Shutdown_Native_Presentation(); attached=false; }
            display.Display::setWidth(width); display.Display::setHeight(height);
            display.setBitDepth(bits); display.setWindowed(windowed);
            Log(nullptr,"display_detach","complete","device released; display dimensions restored",0);
        }
    } driver(*this);
    NativeGraphicsSmoke::Configuration config; config.display=&driver; config.sink=Driver::Log;
    return NativeGraphicsSmoke::Run(config);
}
#endif
