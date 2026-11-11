#define main SetupWindowEntry
#include "../../setup/setup.cpp"
#undef main
#include "../../third_party/imgui/imgui_internal.h"
#include <cassert>

int main(){
    SDL_Init(0);ImGui::CreateContext();
    auto &io=ImGui::GetIO();io.IniFilename=NULL;io.DisplaySize=ImVec2(820,720);
    io.DeltaTime=1.0f/60;io.BackendFlags|=ImGuiBackendFlags_RendererHasTextures;
    io.ConfigInputTrickleEventQueue=false;
    style();sRelease=true;sRoot="./";
    auto frame=[&](bool down,bool card){
        io.AddMousePosEvent(70,70);io.AddMouseButtonEvent(0,down);
        ImGui::NewFrame();
        if(card){
            ImGui::SetNextWindowPos(ImVec2(0,0));ImGui::SetNextWindowSize(ImVec2(820,720));
            ImGui::Begin("probe");ImGui::SetCursorPos(ImVec2(30,40));
            if(language_choice("ptbr","Português (Brasil)","Textos em português do Brasil.",sInstallPtbrPack,360))
                {sInstallPtbrPack=!sInstallPtbrPack;sInstallMask=2;}
            ImGui::End();
        }else build_ui();
        ImGui::Render();assert(ImGui::GetCurrentContext()->ColorStack.Size==0);
    };
    // Click the previously unselected Portuguese option and deselect it again.
    frame(false,true);frame(false,true);frame(false,true);frame(true,true);frame(false,true);assert(sInstallPtbrPack);
    frame(true,true);frame(false,true);assert(!sInstallPtbrPack);
    for(int lang=0;lang<3;++lang)for(int mask=1;mask<=3;++mask)for(int pt=0;pt<2;++pt){
        sUiLanguage=lang;sInstallMask=mask;sInstallPtbrPack=pt;
        frame(false,false);frame(false,false);
        assert(!strcmp(L("Cerrar","Close","Fechar"),lang==0?"Cerrar":lang==1?"Close":"Fechar"));
    }
    io.DisplaySize=ImVec2(720,620);sUiLanguage=2;sInstallPtbrPack=true;
    frame(false,false);frame(false,false);
    printf("PASS: Portuguese option click/release, balanced ImGui styles, 18 language/selection combinations and small window\n");
    ImGui::DestroyContext();SDL_Quit();return 0;
}
