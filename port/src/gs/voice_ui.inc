static bool voice_device_combo(const char *label,bool recording,int &choice){
    char selected[256];PortVoice_DeviceName(recording,selected,sizeof(selected));bool changed=false;
    const char *preview=f1_text(recording&&strcmp(selected,"@none")==0?"No microphone (listen only)":selected[0]?selected:"System default");
    if(ImGui::BeginCombo(label,preview)){
        if(ImGui::Selectable(f1_text("System default"),!selected[0])){choice=-2;changed=true;}
        if(recording&&ImGui::Selectable(f1_text("No microphone (listen only)"),strcmp(selected,"@none")==0)){choice=-1;changed=true;}
        int count=0;auto *ids=recording?SDL_GetAudioRecordingDevices(&count):SDL_GetAudioPlaybackDevices(&count);
        for(int i=0;i<count;++i){const char *name=SDL_GetAudioDeviceName(ids[i]);if(!name)continue;ImGui::PushID(i);if(ImGui::Selectable(name,strcmp(name,selected)==0)){choice=(int)ids[i];changed=true;}ImGui::PopID();}
        SDL_free(ids);ImGui::EndCombo();
    }return changed;
}
static void voice_settings_draw(){
    PortVoiceOptions o;PortVoiceState s;PortVoice_Options(&o);PortVoice_State(&s);bool enabled=o.enabled!=0,dirty=false;
    ImGui::Spacing();ImGui::SeparatorText(f1_text("Voice Chat"));
    if(ImGui::Checkbox(f1_text("Enable voice chat"),&enabled)){o.enabled=enabled;dirty=true;}
    ImGui::SetItemTooltip(f1_text("Optional voice during online battles. The microphone is opened only while voice is enabled and connected to a room."));
    ImGui::TextWrapped("%s",f1_text(s.status));
    if(enabled){
        if(!SDL_WasInit(SDL_INIT_AUDIO))SDL_InitSubSystem(SDL_INIT_AUDIO);
        bool muted=o.muted!=0;if(ImGui::Checkbox(f1_text("Mute microphone"),&muted)){o.muted=muted;dirty=true;}
        dirty|=voice_device_combo(f1_text("Microphone"),true,o.input);dirty|=voice_device_combo(f1_text("Voice output"),false,o.output);
        const char *modeName=f1_text(o.mode?"Voice Activation":"Push to Talk");
        if(ImGui::BeginCombo(f1_text("Transmit mode"),modeName)){
            if(ImGui::Selectable(f1_text("Push to Talk"),o.mode==0)){o.mode=0;dirty=true;}
            if(ImGui::Selectable(f1_text("Voice Activation"),o.mode!=0)){o.mode=1;dirty=true;}
            ImGui::EndCombo();
        }
        if(o.mode){dirty|=ImGui::SliderInt(f1_text("Activation threshold"),&o.threshold,1,100);ImGui::SetItemTooltip(f1_text("Higher values ignore quieter sounds. Use a headset to avoid sending game audio back to other players."));}
        else{
            static bool bindKey=false,bindPad=false;
            ImGui::TextUnformatted(f1_text("Push-to-Talk key"));ImGui::SameLine();
            const char *key=SDL_GetScancodeName((SDL_Scancode)o.key);
            if(ImGui::Button(bindKey?f1_text("Press a key..."):key&&key[0]?key:"F8")){bindKey=true;bindPad=false;}
            ImGui::SetItemTooltip(f1_text("Hold this key to transmit. Escape cancels binding."));
            ImGui::TextUnformatted(f1_text("Push-to-Talk controller button"));ImGui::SameLine();
            const char *button=o.button<0?f1_text("Not assigned"):SDL_GetGamepadStringForButton((SDL_GamepadButton)o.button);
            if(ImGui::Button(bindPad?f1_text("Press a controller button..."):button?button:f1_text("Not assigned"))){bindPad=true;bindKey=false;}
            ImGui::SetItemTooltip(f1_text("Choose a button that does not conflict with your combat controls."));
            if(bindKey||bindPad){
                ImGuiIO &io=ImGui::GetIO();
                if(ImGui::IsKeyPressed(ImGuiKey_Escape,false)){bindKey=bindPad=false;}
                else if(bindKey){const bool *keys=SDL_GetKeyboardState(nullptr);for(int k=4;k<SDL_SCANCODE_COUNT;++k)if(keys[k]&&k!=SDL_SCANCODE_ESCAPE){o.key=k;dirty=true;bindKey=false;break;}}
                else{int count=0;auto *ids=SDL_GetGamepads(&count);for(int i=0;i<count&&bindPad;++i){auto *pad=SDL_GetGamepadFromID(ids[i]);if(!pad)continue;for(int b=0;b<SDL_GAMEPAD_BUTTON_COUNT;++b)if(SDL_GetGamepadButton(pad,(SDL_GamepadButton)b)){o.button=b;dirty=true;bindPad=false;break;}}SDL_free(ids);}
                (void)io;
            }
        }
        dirty|=ImGui::SliderInt(f1_text("Microphone level"),&o.gain,0,200,"%d%%");
        ImGui::ProgressBar(fminf(s.level*5.f,1.f),ImVec2(-1,0),f1_text(s.transmitting?"Transmitting":"Microphone level"));
        dirty|=ImGui::SliderInt(f1_text("Voice volume"),&o.volume,0,150,"%d%%");
        bool indicators=o.indicators!=0;if(ImGui::Checkbox(f1_text("Show microphone icon"),&indicators)){o.indicators=indicators;dirty=true;}
        ImGui::TextWrapped("%s",f1_text("Proximity voice reaches both teams. Volume fades smoothly with distance; players outside its reach are silent."));
        if(s.connected){
            ImGui::TextUnformatted(f1_text("Proximity voice: all players, both teams"));
            for(int i=0;i<s.count;++i){const auto &p=s.peers[i];ImGui::PushID((int)p.id);bool mute=p.muted!=0;if(ImGui::Checkbox(f1_text("Mute"),&mute))PortVoice_Mute(p.id,mute);ImGui::SameLine();ImGui::TextUnformatted(p.name);ImGui::PopID();}
        }
    }
    if(dirty)PortVoice_OptionsSet(&o);
}
static void voice_indicator_draw(){
    PortVoiceOptions o;PortVoiceState s;PortVoice_Options(&o);PortVoice_State(&s);
    const char *fixture=getenv("BT4_RENDER_TEST")?getenv("BT4_VOICE_ICON_TEST"):nullptr;
    if(fixture){o.indicators=1;o.enabled=1;o.gain=100;o.muted=strcmp(fixture,"muted")==0;s.sessionMode=2;s.connected=1;s.capture=strcmp(fixture,"missing")!=0;s.speaking=strcmp(fixture,"speaking")==0;}
    if(!o.indicators||s.sessionMode==0||gUiPresentW<=0||gUiPresentH<=0)return;
    const int state=PortVoice_MicStatus(&o,&s);
    float scale=fminf(gUiPresentW/1280.f,gUiPresentH/720.f);
    float x=gUiPresentX+30*scale,y=gUiPresentY+gUiPresentH-54*scale;
    ImDrawList *dl=ImGui::GetForegroundDrawList();
    ImU32 color=state==1?IM_COL32(65,225,125,255):state==2?IM_COL32(245,76,85,255):IM_COL32(158,168,178,255);
    dl->AddCircleFilled(ImVec2(x,y),21*scale,IM_COL32(7,18,24,155),32);
    dl->AddCircle(ImVec2(x,y),21*scale,IM_COL32(160,190,200,65),32,scale);
    dl->AddRectFilled(ImVec2(x-4*scale,y-12*scale),ImVec2(x+4*scale,y+2*scale),color,4*scale);
    dl->PathArcTo(ImVec2(x,y+2*scale),8*scale,0,3.14159265f,16);dl->PathStroke(color,0,2*scale);
    dl->AddLine(ImVec2(x-8*scale,y-2*scale),ImVec2(x-8*scale,y+2*scale),color,2*scale);
    dl->AddLine(ImVec2(x+8*scale,y-2*scale),ImVec2(x+8*scale,y+2*scale),color,2*scale);
    dl->AddLine(ImVec2(x,y+10*scale),ImVec2(x,y+15*scale),color,2*scale);
    dl->AddLine(ImVec2(x-5*scale,y+15*scale),ImVec2(x+5*scale,y+15*scale),color,2*scale);
    if(state==2){dl->AddLine(ImVec2(x-12*scale,y-14*scale),ImVec2(x+12*scale,y+14*scale),IM_COL32(7,18,24,230),5*scale);dl->AddLine(ImVec2(x-12*scale,y-14*scale),ImVec2(x+12*scale,y+14*scale),color,2.5f*scale);}
    if(fixture){static bool logged;if(!logged){fprintf(stderr,"bt4 mic icon: state=%d x=%.1f y=%.1f local-only=1\n",state,x,y);logged=true;}}
}
