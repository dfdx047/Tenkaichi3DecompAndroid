// The game's folders next to its programs (docs/plans/launcher/LAUNCHER_PLAN.md): `data` holds the
// disc image, `saves` the memory cards. They are made when missing (the first start) and never
// replaced or emptied: what the player put there stays. The launcher lists what it finds in them each
// time a screen is opened, so a disc image or a card moved in while it runs shows up.
#pragma once
#include "host/settings.h"
#include "host/utf8.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

struct GameFolders {
    std::filesystem::path data,saves,assets; // assets: the launcher's pictures, copied there by the build.
    std::filesystem::path textures;          // HD texture packs, in data (docs/plans/graphics/HD_TEXTURES_PLAN.md).
    explicit GameFolders(const std::filesystem::path& program_folder)
        :data(program_folder/"data"),saves(program_folder/"saves"),assets(program_folder/"assets"),textures(data/"textures") {}
    std::filesystem::path logo() const {return assets/"logo.png";}

    // Makes the folders that are missing; returns whether any was made. Existing folders and their
    // files are left as they are.
    bool create() const {
        bool made=false;
        for(const auto& folder:{data,saves,textures}) {
            std::error_code error;
            if(std::filesystem::is_directory(folder,error))continue;
            if(std::filesystem::exists(folder,error))throw std::runtime_error("Not a folder: "+utf8(folder)+" (the game keeps its files there)");
            std::filesystem::create_directory(folder,error);
            if(error)throw std::runtime_error("Cannot make the folder "+utf8(folder));
            made=true;
        }
        return made;
    }

    // The disc images in `data` (file names of .iso files), sorted.
    std::vector<std::string> discImages() const {return names(data,false,{".iso"});}
    // The memory cards in `saves`: each folder is a card (one folder per save in it), sorted.
    std::vector<std::string> cards() const {return names(saves,true,{});}
    // Card image files in `saves` that the game cannot use yet (8 MB raw images of other programs).
    std::vector<std::string> cardImages() const {return names(saves,false,{".ps2",".mcd",".mcr",".mc2",".bin",".vmc"});}

    // The disc image the settings name, when it is a file in `data`; empty otherwise.
    std::filesystem::path disc(const Settings& settings) const {
        if(settings.disc.empty() || !plainName(settings.disc))return {};
        const auto path=data/pathFromUtf8(settings.disc);
        std::error_code error;
        return std::filesystem::is_regular_file(path,error)?path:std::filesystem::path{};
    }
    // The card the settings name, when it is a folder in `saves`; empty otherwise.
    std::filesystem::path card(const Settings& settings) const {
        if(settings.memory_card.empty() || !plainName(settings.memory_card))return {};
        const auto path=saves/pathFromUtf8(settings.memory_card);
        std::error_code error;
        return std::filesystem::is_directory(path,error)?path:std::filesystem::path{};
    }
    // The HD texture pack: the first folder in `textures` (in name order) that holds a `replacements`
    // folder, as PCSX2's packs do, or `textures` itself when it holds one; empty when there is none.
    std::filesystem::path texturePack() const {
        std::error_code error;
        if(std::filesystem::is_directory(textures/"replacements",error))return textures;
        for(const auto& name:names(textures,true,{}))
            if(std::filesystem::is_directory(textures/pathFromUtf8(name)/"replacements",error))return textures/pathFromUtf8(name);
        return {};
    }
    // Ready to play without the launcher: the settings name a disc image that is in `data`.
    bool configured(const Settings& settings) const {return !disc(settings).empty();}

    // Makes a new, empty card ("Card N" with the lowest number not taken) and returns its name.
    std::string newCard() const {
        std::error_code error;
        for(unsigned n=1;;++n) {
            const std::string name="Card "+std::to_string(n);
            if(std::filesystem::exists(saves/name,error))continue;
            std::filesystem::create_directories(saves/name,error);
            if(error)throw std::runtime_error("Cannot make the card folder "+utf8(saves/name));
            return name;
        }
    }

    // On the first start (the saves folder was just made): the cards of the older card folder
    // (user/memcards) are copied in, so the card in use stays; nothing there is moved or changed, and
    // nothing in saves is replaced. Returns how many cards were copied.
    unsigned copyCardsFrom(const std::filesystem::path& older) const {
        unsigned copied=0;std::error_code error;
        for(std::filesystem::directory_iterator it(older,error),end;!error && it!=end;it.increment(error)) {
            std::error_code kind;
            if(!it->is_directory(kind) || std::filesystem::exists(saves/it->path().filename(),kind))continue;
            std::error_code failed;
            std::filesystem::copy(it->path(),saves/it->path().filename(),std::filesystem::copy_options::recursive,failed);
            if(!failed)++copied;
        }
        return copied;
    }

    // A file or folder name without a folder part.
    static bool plainName(const std::string& name) {
        return name!="." && name!=".." && name.find_first_of("/\\:")==std::string::npos;
    }

private:
    static std::vector<std::string> names(const std::filesystem::path& folder,bool folders,std::initializer_list<const char*> extensions) {
        std::vector<std::string> found;std::error_code error;
        for(std::filesystem::directory_iterator it(folder,error),end;!error && it!=end;it.increment(error)) {
            std::error_code kind;
            if(folders) {if(it->is_directory(kind))found.push_back(utf8(it->path().filename()));continue;}
            if(!it->is_regular_file(kind))continue;
            auto extension=utf8(it->path().extension());
            for(auto& c:extension)c=char(std::tolower(static_cast<unsigned char>(c)));
            if(std::any_of(extensions.begin(),extensions.end(),[&](const char* e){return extension==e;}))found.push_back(utf8(it->path().filename()));
        }
        std::sort(found.begin(),found.end());
        return found;
    }
};
