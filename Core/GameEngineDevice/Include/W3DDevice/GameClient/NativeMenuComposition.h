// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#if defined(_WIN64)
#include "W3DDevice/GameClient/W3DGameWindowManager.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/GadgetStaticText.h"
#include "GameClient/GameWindowTransitions.h"
#include "GameClient/GameFont.h"
#include "GameClient/DisplayString.h"
#include "GameClient/GameText.h"
#include "Common/NameKeyGenerator.h"
#include "Common/FunctionLexicon.h"
#include "Common/StartupDiagnostics.h"
#include "WW3D2/texture.h"
#include "WW3D2/Texture2DCPUContent.h"
#include <map>
#include <memory>
#include <stdexcept>

// Diagnostic-only static composition. The original parser, constructors and
// widget draws remain unchanged. Never run layout callbacks, input, transitions
// or ordinary updates. Only the original gadget label/lifetime messages pass.
class NativeStaticMenuManager : public W3DGameWindowManager {
    struct DrawEntry { GameWinDrawFunc original;AsciiString name;bool complete=false; };
    std::map<GameWindow *,DrawEntry> tracked;
    static void CheckedDraw(GameWindow *w,WinInstanceData *data) {
        auto *self=static_cast<NativeStaticMenuManager*>(TheWindowManager);
        auto entry=self->tracked.find(w);
        if(entry==self->tracked.end()) throw std::runtime_error("Untracked static menu draw");
        auto &r=entry->second;
        if(!r.complete) StartupDiagnostics::checkpoint("ui_menu_widget_draw","begin",r.name.str());
        r.original(w,data);
        if(!r.complete) StartupDiagnostics::checkpoint("ui_menu_widget_draw","complete",r.name.str());
        r.complete=true;
    }
public:
    void TrackDraw(GameWindow *w,const char *name) {
        tracked.emplace(w,DrawEntry{w->winGetDrawFunc(),AsciiString(name)});
        w->winSetDrawFunc(CheckedDraw);
    }
    unsigned blockedSystem = 0, blockedInput = 0;
    WindowMsgHandledType winSendSystemMsg(GameWindow *w, UnsignedInt msg,
        WindowMsgData a, WindowMsgData b) override {
        const auto f = w ? w->winGetSystemFunc() : nullptr;
        if(f == GadgetPushButtonSystem || f == GadgetStaticTextSystem) {
            if(msg != GWM_CREATE && msg != GWM_DESTROY && msg != GGM_SET_LABEL && msg != GGM_GET_LABEL)
                throw std::runtime_error("Unexpected static menu gadget message");
            return GameWindowManager::winSendSystemMsg(w,msg,a,b);
        }
        // Explicit isolation policy, including MainMenuSystem GWM_CREATE and
        // PassSelectedButtonsToParentSystem. No ordinary application event runs.
        if(f) ++blockedSystem;
        return MSG_IGNORED;
    }
    WindowMsgHandledType winSendInputMsg(GameWindow *, UnsignedInt,
        WindowMsgData, WindowMsgData) override { ++blockedInput; return MSG_IGNORED; }
    void RepaintStaticRoot(GameWindow *root) { drawWindow(root); }
};

class NativeMenuComposition final {
    GameWindowManager *previousManager;
    GameWindowTransitionsHandler *previousTransitions;
    NativeStaticMenuManager *manager;
    GameWindow *root = nullptr;
    struct ImageRelease { void operator()(Image *p) const { if(p) deleteInstance(p); } };
    struct TextureRelease { void operator()(TextureClass *p) const { if(p) p->Release_Ref(); } };
    std::map<AsciiString,std::unique_ptr<TextureClass,TextureRelease>> textures;
    std::map<const Image *,std::unique_ptr<Image,ImageRelease>> images;
    bool firstDraw = true;
    unsigned parsed = 0;
    void HideTree(GameWindow *w) {
        if(!w) return;
        if(++parsed > 128) throw std::runtime_error("Unexpected MainMenu window count");
        w->winHide(TRUE);
        w->winSetInputFunc(nullptr); w->winSetTooltipFunc(nullptr);
        // Keep only built-in gadget destruction/label ownership after parsing.
        const auto f=w->winGetSystemFunc();
        if(f!=GadgetPushButtonSystem && f!=GadgetStaticTextSystem) w->winSetSystemFunc(nullptr);
        for(auto *c=w->winGetChild();c;c=c->winGetNext()) HideTree(c);
    }
    GameWindow *Select(const char *name, GameWinDrawFunc expected) {
        AsciiString full("MainMenu.wnd:"); full.concat(name);
        GameWindow *w=manager->winGetWindowFromId(root,TheNameKeyGenerator->nameToKey(full));
        if(!w || w->winGetDrawFunc()!=expected || w->winGetInstanceData()->m_videoBuffer)
            throw std::runtime_error("Missing/unexpected original static menu widget");
        w->winHide(FALSE);
        auto *data=w->winGetInstanceData();
        if(data->getState()!=0) throw std::runtime_error("Unexpected static widget state");
        if(w->winGetStyle() & GWS_PUSH_BUTTON) {
            if(!data->getTextLength() || !data->getFont() ||
                data->getFont()->nameString!=AsciiString("Generals") ||
                data->getFont()->pointSize!=15 || data->getFont()->bold)
                throw std::runtime_error("Original MainButton font/text contract mismatch");
            for(int i : {0,5,6}) if(!w->winGetEnabledImage(i))
                throw std::runtime_error("Original three-piece button artwork unavailable");
        }
        if(expected==W3DGadgetPushButtonImageDraw) {
            for(int i : {0,5,6}) w->winSetEnabledImage(i,PrivateImage(w->winGetEnabledImage(i)));
        } else if(expected==W3DGameWinDefaultDraw && (w->winGetStatus() & WIN_STATUS_IMAGE)) {
            const auto *src=w->winGetEnabledImage(0);
            if(!src) throw std::runtime_error("Original USER image unavailable");
            w->winSetEnabledImage(0,PrivateImage(src));
        } // W3DNoDraw root deliberately has no image submission.
        int x,y,width,height; w->winGetScreenPosition(&x,&y);w->winGetSize(&width,&height);
        struct Spec {const char *name;int x,y,ex,ey;const char *label;};
        static const Spec specs[]={
            {"MainMenuParent",0,0,800,600,""},{"MainMenuRuler",0,0,800,600,""},
            {"MapBorder2",532,108,756,360,""},{"EarthMap2",532,108,756,352,""},
            {"ButtonSinglePlayer",540,116,748,152,"GUI:SinglePlayer"},
            {"ButtonMultiplayer",540,156,748,192,"GUI:Multiplayer"},
            {"ButtonLoadReplay",540,196,748,231,"GUI:ReplayMenu"},
            {"ButtonOptions",540,236,748,272,"GUI:Options"},
            {"ButtonCredits",540,276,748,312,"GUI:Credits"},
            {"ButtonExit",540,316,748,352,"GUI:Exit"},{"Logo",504,16,791,110,""}};
        bool matched=false;
        for(const auto &spec:specs) if(!strcmp(name,spec.name)) {
            matched=true;
            if(x!=spec.x || y!=spec.y || x+width!=spec.ex || y+height!=spec.ey ||
                data->m_textLabelString!=AsciiString(spec.label))
                throw std::runtime_error("Original MainMenu rectangle/label contract mismatch");
            if(*spec.label) {
                Bool exists=FALSE;
                const auto text=TheGameText->fetch(spec.label,&exists);
                if(!exists || text.isEmpty() || !data->getTextDisplayString())
                    throw std::runtime_error("Original menu localized string unavailable");
                int tw,th;data->getTextDisplayString()->getSize(&tw,&th);
                if(tw<=0 || th<=0 || tw>width || th>height)
                    throw std::runtime_error("Original button label does not fit WND rectangle");
            }
        }
        if(!matched) throw std::runtime_error("Unspecified static menu widget");
        char detail[512];sprintf_s(detail,"name=%s rect=%d,%d,%d,%d label=%s font=%s size=%d bold=%d",
            name,x,y,x+width,y+height,data->m_textLabelString.str(),
            data->getFont()?data->getFont()->nameString.str():"",data->getFont()?data->getFont()->pointSize:0,
            data->getFont()?data->getFont()->bold:0);
        StartupDiagnostics::checkpoint("ui_menu_widget","complete",detail);
        manager->TrackDraw(w,name);
        return w;
    }
    Image *PrivateImage(const Image *src) {
        auto found=images.find(src); if(found!=images.end()) return found->second.get();
        if(src->getStatus()!=IMAGE_STATUS_NONE || src->getFilename().isEmpty() ||
            src->getImageWidth()<=0 || src->getImageHeight()<=0)
            throw std::runtime_error("Unsupported/missing original menu image metadata");
        StartupDiagnostics::checkpoint("ui_menu_image","begin",src->getName().str());
        auto &texture=textures[src->getFilename()];
        if(!texture) {
            StartupDiagnostics::checkpoint("ui_menu_atlas","begin",src->getFilename().str());
            texture.reset(new TextureClass(src->getFilename().str(),nullptr,MIP_LEVELS_1));
            texture->Prepare_Render2D_Texture();
            const auto *content=texture->Get_Render2D_CPU_Content();
            if(!content || content->Describe().width!=unsigned(src->getTextureSize()->x) ||
                content->Describe().height!=unsigned(src->getTextureSize()->y))
                throw std::runtime_error("Original menu atlas dimensions disagree");
            StartupDiagnostics::checkpoint("ui_menu_atlas","complete",src->getFilename().str());
        }
        std::unique_ptr<Image,ImageRelease> copy(newInstance(Image));
        Region2D uv=*src->getUV(); ICoord2D size=*src->getImageSize();
        copy->setName(src->getName()); copy->setFilename(src->getFilename());copy->setUV(&uv);
        copy->setImageSize(&size);copy->setTextureWidth(src->getTextureSize()->x);
        copy->setTextureHeight(src->getTextureSize()->y);copy->setRawTextureData(texture.get());
        copy->setStatus(IMAGE_STATUS_RAW_TEXTURE);
        auto *result=copy.get();images.emplace(src,std::move(copy));
        char detail[512];sprintf_s(detail,"name=%s file=%s size=%dx%d uv=%.9g,%.9g,%.9g,%.9g",
            src->getName().str(),src->getFilename().str(),size.x,size.y,uv.lo.x,uv.lo.y,uv.hi.x,uv.hi.y);
        StartupDiagnostics::checkpoint("ui_menu_image","complete",detail);return result;
    }
public:
    NativeMenuComposition() : previousManager(TheWindowManager),previousTransitions(TheTransitionHandler),
        manager(new NativeStaticMenuManager) {
        // Base destruction owns TheTransitionHandler. Isolate it for the whole
        // private manager lifetime; preserve the initialized engine singleton.
        TheTransitionHandler=nullptr; TheWindowManager=manager;
    }
    ~NativeMenuComposition() {
        // Window/DisplayString sentence resources precede atlas owners and device.
        manager->winDestroyAll(); delete manager;
        images.clear();textures.clear();
        TheWindowManager=previousManager;TheTransitionHandler=previousTransitions;
        StartupDiagnostics::checkpoint("ui_menu_release","complete",
            "widgets/display strings/private images/atlases released; engine manager/transitions restored");
    }
    void Load() {
        StartupDiagnostics::checkpoint("ui_menu_load","begin","original Window/Menus/MainMenu.wnd; static callbacks isolated");
        // Do not wrap in WindowLayout: load/init/shutdown callbacks must not run.
        // A bare name is prefixed only with Window\ by the legacy parser.
        // Use the original archive path, including its Menus subdirectory.
        root=manager->winCreateFromScript("Window\\Menus\\MainMenu.wnd");
        if(!root) throw std::runtime_error("Original MainMenu.wnd parse failed");
        HideTree(root);
        if(parsed!=63) throw std::runtime_error("Original Generals MainMenu window count mismatch");
        // Original initial main dropdown subset, without MainMenuInit side effects.
        const auto noDraw=TheFunctionLexicon->gameWinDrawFunc(TheNameKeyGenerator->nameToKey("W3DNoDraw"));
        if(!noDraw) throw std::runtime_error("Missing original W3DNoDraw callback");
        Select("MainMenuParent",noDraw);
        Select("MainMenuRuler",W3DGameWinDefaultDraw);
        Select("MapBorder2",W3DGameWinDefaultDraw);
        Select("EarthMap2",W3DGameWinDefaultDraw);
        for(const char *name : {"ButtonSinglePlayer","ButtonMultiplayer","ButtonLoadReplay",
            "ButtonOptions","ButtonCredits","ButtonExit"}) Select(name,W3DGadgetPushButtonImageDraw);
        Select("Logo",W3DGameWinDefaultDraw);
        char detail[256];sprintf_s(detail,"parsed=%u visible=11 buttons=6 images=%u atlases=%u blocked_system=%u blocked_input=%u; no layout callbacks/input/update",
            parsed,unsigned(images.size()),unsigned(textures.size()),manager->blockedSystem,manager->blockedInput);
        StartupDiagnostics::checkpoint("ui_menu_load","complete",detail);
    }
    void Render() {
        if(firstDraw) StartupDiagnostics::checkpoint("ui_menu_draw","begin","original manager tree/default USER/three-piece buttons/DisplayString");
        manager->RepaintStaticRoot(root);
        if(firstDraw) StartupDiagnostics::checkpoint("ui_menu_draw","complete","11 original windows; 6 original labels; static main dropdown");
        firstDraw=false;
    }
};
#endif
