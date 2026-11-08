// The player's programs (docs/plans/launcher/LAUNCHER_PLAN.md). Built twice from this file:
//   bt2-game      starts the game with the stored settings; while the game is not set up (no
//                 settings file, or no disc image of the data folder chosen) it shows the launcher first.
//   bt2-launcher  always shows the launcher (BT2_LAUNCHER_ALWAYS).
// Next to the programs: `data` (the disc image) and `saves` (the memory cards), made on the first start
// and never emptied (host/game_folders.h), and `assets` (the launcher's logo, from the build).
// Both then run the runtime program (bt2-entry-probe, next to this one) as a player's run: window,
// sound, live input, the GPU renderer, the settings file. The runtime still starts from the local
// startup inputs in work/bt2-generation, so these programs run from a prepared repository only.
// When the player chooses "Quit to the launcher" in the game, the launcher comes back.
#include "host/game_folders.h"
#include "host/launcher.h"
#include "host/player_session.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {
namespace fs=std::filesystem;

// The runtime's startup inputs, in its argument order (tools/ps2recomp/run_startup_probe.py holds
// the same list; tools/ps2recomp/test_game_inputs.py compares them).
constexpr const char* startup_inputs[]={"guest-memory.bin","kernel-snapshot.bin","timer3-mode.bin","timer2-mode.bin","intc-mask.bin","timer2-readback.bin",
    "osd-config.bin","osd-readback.bin","dma-status.bin","dma5-control.bin","sif-command.bin","sif-dma.bin","sif-rpc-state.bin","sif-bind.bin","cd-init.bin",
    "scmd-bind.bin","heap-bind.bin","heap-allocation.bin","texture-startup.bin","ee-thread-start.bin","disc-bind.bin","ncmd-bind.bin"};

fs::path programFolder() {
    char path[MAX_PATH]{};GetModuleFileNameA(nullptr,path,MAX_PATH);
    return fs::path(path).parent_path();
}
// The repository root: the first folder above the program that holds the startup inputs.
fs::path findRoot(fs::path from) {
    for(unsigned up=0;up<8;++up,from=from.parent_path()) {
        std::error_code error;
        if(fs::is_regular_file(from/"work"/"bt2-generation"/startup_inputs[0],error))return from;
        if(!from.has_parent_path() || from.parent_path()==from)break;
    }
    throw std::runtime_error("The game's local startup inputs (work/bt2-generation) were not found above the program's folder.");
}
std::string inQuotes(const fs::path& path) {return "\""+path.string()+"\"";}

// Runs the game; returns the runtime's exit code. Its messages go to `log`.
DWORD runGame(const fs::path& root,const fs::path& settings_path,const Settings& settings,const GameFolders& folders,const fs::path& log) {
    const fs::path runtime=programFolder()/"bt2-entry-probe.exe",inputs=root/"work"/"bt2-generation";
    std::error_code error;
    if(!fs::is_regular_file(runtime,error))throw std::runtime_error("The runtime program is missing: "+runtime.string());
    // MEMORY_IMAGE STOP_REPORT inputs... DISC last-input, as the runtime's command line is defined.
    const std::size_t count=std::size(startup_inputs);
    std::string command=inQuotes(runtime)+" "+inQuotes(inputs/startup_inputs[0])+" "+inQuotes(root/"user"/"last-run.json");
    for(std::size_t n=1;n<count;++n) {
        if(!fs::is_regular_file(inputs/startup_inputs[n],error))throw std::runtime_error(std::string("A startup input is missing: ")+startup_inputs[n]);
        if(n==count-1)command+=" "+inQuotes(folders.disc(settings));
        command+=" "+inQuotes(inputs/startup_inputs[n]);
    }
    for(const char* name:{"BT2_VIEWER","BT2_AUDIO_DEVICE","BT2_HOST_INPUT"})SetEnvironmentVariableA(name,"1");
    SetEnvironmentVariableA("BT2_GPU_GS","on");
    SetEnvironmentVariableA("BT2_PLAYER_SETTINGS",settings_path.string().c_str());
    // The picture settings the renderer takes at its start: HD textures (when a pack is there) and super-sampling.
    const fs::path pack=settings.hd_textures?folders.texturePack():fs::path{};
    SetEnvironmentVariableW(L"BT2_HD_TEXTURES",pack.empty()?nullptr:pack.wstring().c_str());
    SetEnvironmentVariableA("BT2_SUPER_SAMPLING",std::to_string(settings.super_sampling).c_str());
    SetEnvironmentVariableA("BT2_GAME_PATCHES",settings.widescreen?"widescreen":nullptr); // ee/game_patches.h
    SetEnvironmentVariableA("BT2_SIXTY_FPS",settings.sixty_fps?"1":nullptr); // gs/gpu/in_between_frames.h
    // The memory card folder in slot 1, when the settings name one of the saves folder.
    const fs::path card=folders.card(settings);
    SetEnvironmentVariableW(L"BT2_CARD_FOLDER",card.empty()?nullptr:card.wstring().c_str());
    for(const char* name:{"BT2_FRAME_LIMIT","BT2_PAD_SCRIPT","BT2_PAD_RECORD","BT2_VIEWER_SCALE","BT2_GPU_GS_SNAPSHOT"})SetEnvironmentVariableA(name,nullptr);

    SECURITY_ATTRIBUTES inherit{sizeof inherit,nullptr,TRUE};
    const HANDLE out=CreateFileA(log.string().c_str(),GENERIC_WRITE,FILE_SHARE_READ,&inherit,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(out==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot write "+log.string());
    STARTUPINFOA startup{};startup.cb=sizeof startup;startup.dwFlags=STARTF_USESTDHANDLES;startup.hStdOutput=startup.hStdError=out;
    PROCESS_INFORMATION process{};
    std::string mutable_command=command;
    if(!CreateProcessA(nullptr,mutable_command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,root.string().c_str(),&startup,&process)) {
        CloseHandle(out);throw std::runtime_error("The runtime program could not be started.");
    }
    WaitForSingleObject(process.hProcess,INFINITE);
    DWORD code=1;GetExitCodeProcess(process.hProcess,&code);
    CloseHandle(process.hThread);CloseHandle(process.hProcess);CloseHandle(out);
    return code;
}
// The end of the runtime's messages, for the player when a run stops by itself.
std::string lastLines(const fs::path& log,unsigned count) {
    std::ifstream in(log);std::vector<std::string> lines;std::string line;
    while(std::getline(in,line))if(!line.empty() && line.rfind("[INFO]",0)!=0)lines.push_back(line.substr(0,300));
    std::string text;
    for(std::size_t n=lines.size()>count?lines.size()-count:0;n<lines.size();++n)text+=lines[n]+"\n";
    return text;
}
// How many places of untranslated code the session ran through the interpreter (its log lines).
unsigned interpretedPlaces(const fs::path& log) {
    std::ifstream in(log);std::string line;unsigned count=0;
    while(std::getline(in,line))if(line.find("runs interpreted")!=std::string::npos)++count;
    return count;
}
}

int WINAPI WinMain(HINSTANCE,HINSTANCE,LPSTR arguments,int) {
    try {
        if(std::string(arguments)=="--preview-paused") {
            // The in-game menu over a stand-in picture, without starting the game: for looking at it and trying its screens.
            Settings preview;DisplayFrame picture;picture.width=640;picture.height=448;picture.bgra.resize(640*448);
            for(unsigned y=0;y<448;++y)for(unsigned x=0;x<640;++x)picture.bgra[y*640+x]=((x*255/640)<<16)|((y*255/448)<<8)|0x60;
            ViewerWindow window;HostInput input;
            const auto file=(fs::temp_directory_path()/"bt2-preview-settings").string();
            runOverlay(preview,file,window,input,picture,[&]{window.setScaling(preview.integer_scaling?ViewerWindow::Scaling::Integer:ViewerWindow::Scaling::Stretch);window.setLayout(preview.window_scale,preview.fullscreen);});
            return 0;
        }
        const fs::path root=findRoot(programFolder()),user=root/"user",settings_path=user/"settings",log=user/"last-run.log";
        fs::create_directories(user);
        const GameFolders folders(programFolder());
        {
            std::error_code error;
            const bool first_start=!fs::exists(folders.saves,error);
            folders.create();
            if(first_start)folders.copyCardsFrom(user/"memcards");
        }
        Settings settings;
        if(auto stored=Settings::load(settings_path.string()))settings=*stored;
#ifdef BT2_LAUNCHER_ALWAYS
        bool launcher=true;
#else
        std::error_code error;
        bool launcher=!fs::is_regular_file(settings_path,error) || !folders.configured(settings);
#endif
        for(;;) {
            if(launcher) {
                ViewerWindow window;HostInput input;
                settings.save(settings_path.string());
                if(runLauncher(settings,settings_path.string(),folders,window,input)!=LauncherResult::Start)return 0;
            }
            if(!folders.configured(settings)) {launcher=true;continue;} // No disc image of the data folder: choose one.
            if(folders.card(settings).empty()) {
                // The game always has a card to save to: the first card of the saves folder, or a new
                // empty one when there is none. Existing cards are never replaced.
                const auto cards=folders.cards();
                settings.memory_card=cards.empty()?folders.newCard():cards.front();settings.save(settings_path.string());
            }
            const DWORD code=runGame(root,settings_path,settings,folders,log);
            if(auto stored=Settings::load(settings_path.string()))settings=*stored; // The in-game menu may have changed them.
            if(code==DWORD(PlayerSession::launcher_exit_code)) {launcher=true;continue;}
            // Code that was never translated ran interpreted (slower), or stopped the game: the discovery
            // loop builds it in.
            const std::string discover="double-click\n"+(root/"work"/"scripts"/"discover.cmd").string()+"\nand play again.\n";
            const unsigned interpreted=interpretedPlaces(log);
            if(code!=0) {
                const std::string last=lastLines(log,6);
                const std::string hint=last.find("Branch target")!=std::string::npos||interpreted?"\nThe game reached code that is not translated yet. To build it in, "+discover:"";
                MessageBoxA(nullptr,("The game stopped.\n\n"+last+hint+"\nThe full messages are in "+log.string()).c_str(),"BT2 Recomp",MB_OK|MB_ICONINFORMATION);
            } else if(interpreted) {
                MessageBoxA(nullptr,("The game ran "+std::to_string(interpreted)+" place(s) of code that is not translated yet, more slowly than the rest.\nTo build it in, "+discover).c_str(),"BT2 Recomp",MB_OK|MB_ICONINFORMATION);
            }
            return int(code);
        }
    } catch(const std::exception& error) {
        MessageBoxA(nullptr,error.what(),"BT2 Recomp",MB_OK|MB_ICONERROR);
        return 1;
    }
}
