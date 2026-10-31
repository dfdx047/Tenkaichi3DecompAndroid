// A memory card as a host folder (docs/plans/memory-card/MEMORY_CARD_PLAN.md): one sub-folder per
// save with the save's files, and `entries.txt` in it for what the host file system cannot keep
// (each entry's mode and its two time stamps in the card's own 8-byte form; "." is the folder).
// This is the file layer the native card service answers from: open, close, seek, read, write,
// listing, making folders, deleting and setting an entry's data, with the card library's result
// codes. Paths are the card's ("/SAVE/file", or relative to the current directory); nothing outside
// the folder can be named. An open file is held in memory; closing or flushing a changed one writes
// a temporary file and renames it over the old one, so a file on the host is never half written.
#pragma once
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <span>
#include <sstream>
#include <string>
#include <vector>

class CardFolder {
public:
    static constexpr int full=-3,no_entry=-4,denied=-5,not_empty=-6,too_many_open=-7,max_open=3;
    static constexpr std::uint16_t file_mode=0x8497,folder_mode=0x8427;
    // Open flags of the card library: reading, writing, create the file, empty it, make a folder.
    static constexpr unsigned read_flag=1,write_flag=2,create_flag=0x200,truncate_flag=0x400,folder_flag=0x40;
    static constexpr unsigned known_flags=read_flag|write_flag|create_flag|truncate_flag|folder_flag;
    // An 8 MB card's clusters of 1 KB that files can use.
    static constexpr std::uint32_t clusters=8000;
    using Stamp=std::array<std::uint8_t,8>;
    // The time written to entries: the card's form (0, second, minute, hour, day, month, year in two
    // bytes) in Japanese time, as the console's clock keeps it. Replaceable for tests.
    std::function<Stamp()> now=[] {
        const auto t=std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now())+std::chrono::hours(9);
        const auto day=std::chrono::floor<std::chrono::days>(t);
        const std::chrono::year_month_day date(day);const std::chrono::hh_mm_ss time(t-day);
        const auto year=unsigned(int(date.year()));
        return Stamp{0,std::uint8_t(time.seconds().count()),std::uint8_t(time.minutes().count()),std::uint8_t(time.hours().count()),
                     std::uint8_t(unsigned(date.day())),std::uint8_t(unsigned(date.month())),std::uint8_t(year),std::uint8_t(year>>8)};
    };
    struct Entry {
        std::string name;std::uint16_t mode=0;std::uint32_t size=0;
        std::array<std::uint8_t,8> created{},modified{};
    };

    explicit CardFolder(std::filesystem::path root):root_(std::move(root)) {}

    // The file's descriptor (0 to 2), 0 for a folder that was made, or a negative result code.
    int open(const std::string& name,unsigned flags) {
        if(flags&~known_flags || !(flags&(read_flag|write_flag|folder_flag)))return denied;
        const auto path=resolve(name);
        std::error_code error;
        if(path.empty() || hidden(path) || path==root_)return no_entry;
        if(flags&folder_flag) {
            // A new folder in an existing one; an existing name is reported as the card does.
            if(flags!=folder_flag)return denied;
            if(std::filesystem::exists(path,error) || !std::filesystem::is_directory(path.parent_path(),error))return no_entry;
            if(freeClusters()<2)return full;
            if(!std::filesystem::create_directory(path,error))return denied;
            const auto stamp=now();
            setLine(path/"entries.txt",".",folder_mode,&stamp,&stamp);
            return 0;
        }
        const bool exists=std::filesystem::is_regular_file(path,error);
        if(!exists && (!(flags&create_flag) || !(flags&write_flag) || std::filesystem::exists(path,error) || !inSave(path)))return no_entry;
        for(int fd=0;fd<max_open;++fd)if(!open_[fd].used) {
            auto& f=open_[fd];f={};
            if(!exists) {
                // A new file is on the card from now on, empty.
                if(freeClusters()<2)return full;
                const auto stamp=now();
                f.changed=true;f.path=path;
                if(!store(f))return denied;
                setLine(path.parent_path()/"entries.txt",path.filename().string(),file_mode,&stamp,&stamp);
            } else if(!(flags&truncate_flag)) {
                std::ifstream in(path,std::ios::binary);
                f.data.assign(std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>());
            } else f.changed=true;
            f.used=true;f.path=path;f.writable=(flags&write_flag)!=0;f.readable=(flags&read_flag)!=0;
            return fd;
        }
        return too_many_open;
    }
    // Closing writes a changed file to the host; so does flushing, which keeps it open.
    int close(int fd) {
        if(!valid(fd))return no_entry;
        const bool stored=store(open_[fd]);
        open_[fd]={};return stored?0:denied;
    }
    int flush(int fd) {
        if(!valid(fd))return no_entry;
        return store(open_[fd])?0:denied;
    }
    // Bytes written at the position (the file grows as needed), or a negative result code.
    int write(int fd,std::span<const std::uint8_t> in) {
        if(!valid(fd))return no_entry;
        auto& f=open_[fd];
        if(!f.writable)return denied;
        const std::size_t end=f.position+in.size();
        if(end>f.data.size()) {
            // What the file takes beyond its size on the host, which the free space already counts.
            std::error_code error;
            const auto stored=std::filesystem::file_size(f.path,error);
            const std::size_t have=error?0:(std::size_t(stored)+1023)/1024;
            if((end+1023)/1024>have+freeClusters())return full;
            f.data.resize(end);
        }
        std::copy(in.begin(),in.end(),f.data.begin()+std::ptrdiff_t(f.position));
        f.position=end;f.changed=true;return int(in.size());
    }
    // Deletes a file, or a folder with nothing in it; 0, or a negative result code.
    int remove(const std::string& name) {
        const auto path=resolve(name);
        std::error_code error;
        if(path.empty() || hidden(path) || path==root_ || !std::filesystem::exists(path,error))return no_entry;
        for(const auto& f:open_)if(f.used && f.path==path)return denied;
        if(std::filesystem::is_directory(path,error)) {
            for(std::filesystem::directory_iterator it(path,error),end;!error && it!=end;it.increment(error))if(!hidden(it->path()))return not_empty;
            if(std::filesystem::path(root_/current_)==path)current_.clear();
            std::filesystem::remove(path/"entries.txt",error);
            return std::filesystem::remove(path,error)?0:denied;
        }
        if(!std::filesystem::remove(path,error))return denied;
        setLine(path.parent_path()/"entries.txt",path.filename().string(),0,nullptr,nullptr);
        return 0;
    }
    // Sets the parts of an entry's data that `valid` names (1: created, 2: modified, 4: mode); 0, or
    // a negative result code.
    int setInfo(const std::string& name,const Entry& info,unsigned valid) {
        const auto path=resolve(name);
        std::error_code error;
        if(path.empty() || hidden(path) || path==root_ || !std::filesystem::exists(path,error))return no_entry;
        const bool folder=std::filesystem::is_directory(path,error);
        Entry e=entry(path);
        if(valid&1)e.created=info.created;
        if(valid&2)e.modified=info.modified;
        if(valid&4)e.mode=info.mode;
        setLine((folder?path:path.parent_path())/"entries.txt",folder?".":e.name,e.mode,&e.created,&e.modified);
        return 0;
    }
    // Free space in 1 KB clusters: the card's clusters less what is on it, each file rounded up to
    // whole clusters plus two for its directory entry, two for each folder (an estimate of the card's
    // own bookkeeping; the game only compares it with what a save needs).
    std::uint32_t freeClusters() const {
        std::uint64_t used=0;std::error_code error;
        for(std::filesystem::recursive_directory_iterator it(root_,error),end;!error && it!=end;it.increment(error)) {
            if(hidden(it->path()))continue;
            used+=it->is_regular_file(error)?(it->file_size(error)+1023)/1024+2:2;
        }
        return used>=clusters?0u:std::uint32_t(clusters-used);
    }
    // The new position, or a negative result code. Origin: 0 from the start, 1 from here, 2 from the end.
    int seek(int fd,std::int32_t offset,unsigned origin) {
        if(!valid(fd) || origin>2)return no_entry;
        auto& f=open_[fd];
        const std::int64_t target=(origin==0?0:origin==1?std::int64_t(f.position):std::int64_t(f.data.size()))+offset;
        if(target<0 || target>std::int64_t(f.data.size()))return denied;
        f.position=std::size_t(target);return int(target);
    }
    // Bytes read into `out` (fewer at the end of the file), or a negative result code.
    int read(int fd,std::span<std::uint8_t> out) {
        if(!valid(fd))return no_entry;
        auto& f=open_[fd];
        if(!f.readable)return denied;
        const std::size_t count=std::min(out.size(),f.data.size()-f.position);
        std::copy_n(f.data.begin()+std::ptrdiff_t(f.position),count,out.begin());
        f.position+=count;return int(count);
    }
    // The entries a path names: one file or folder, or with "*" as its last part everything in that
    // folder ("." and ".." first below the top, as a card lists them).
    std::vector<Entry> list(const std::string& pattern) {
        std::vector<Entry> found;
        const bool all=pattern.ends_with("/*") || pattern=="*";
        const auto path=resolve(all?pattern.substr(0,pattern.size()-1):pattern);
        std::error_code error;
        if(path.empty())return found;
        if(!all) {
            if(std::filesystem::exists(path,error) && !hidden(path))found.push_back(entry(path));
            return found;
        }
        if(!std::filesystem::is_directory(path,error))return found;
        if(!std::filesystem::equivalent(path,root_,error)) {
            Entry self=entry(path);self.name=".";found.push_back(self);
            Entry parent;parent.name="..";parent.mode=folder_mode;found.push_back(parent);
        }
        std::vector<std::filesystem::path> children;
        for(std::filesystem::directory_iterator it(path,error),end;!error && it!=end;it.increment(error))if(!hidden(it->path()))children.push_back(it->path());
        std::sort(children.begin(),children.end());
        for(const auto& child:children)found.push_back(entry(child));
        return found;
    }
    // Makes `name` the current directory; 0, or a negative result code.
    int changeDirectory(const std::string& name) {
        const auto path=resolve(name);
        std::error_code error;
        if(path.empty() || !std::filesystem::is_directory(path,error))return no_entry;
        current_=std::filesystem::relative(path,root_,error).generic_string();
        if(current_==".")current_.clear();
        return 0;
    }
    std::string currentDirectory() const {return "/"+current_;}

private:
    struct Open {bool used=false,readable=false,writable=false,changed=false;std::filesystem::path path;std::vector<std::uint8_t> data;std::size_t position=0;};
    std::filesystem::path root_;std::string current_;std::array<Open,max_open> open_{};

    bool valid(int fd) const {return fd>=0 && fd<max_open && open_[fd].used;}
    static bool hidden(const std::filesystem::path& path) {return path.filename()=="entries.txt" || path.extension()==".bt2-new";}
    // Files live in a save's folder, never at the top of the card.
    bool inSave(const std::filesystem::path& path) const {
        std::error_code error;
        return path.parent_path()!=root_ && std::filesystem::is_directory(path.parent_path(),error);
    }
    // Writes a changed file to the host under a temporary name and renames it over the old one.
    bool store(Open& f) {
        if(!f.changed)return true;
        auto fresh=f.path;fresh+=".bt2-new";
        {
            std::ofstream out(fresh,std::ios::binary|std::ios::trunc);
            out.write(reinterpret_cast<const char*>(f.data.data()),std::streamsize(f.data.size()));
            if(!out)return false;
        }
        std::error_code error;
        std::filesystem::rename(fresh,f.path,error);
        if(error)return false;
        f.changed=false;
        const auto stamp=now();
        Entry e=entry(f.path);
        setLine(f.path.parent_path()/"entries.txt",e.name,e.mode,&e.created,&stamp);
        return true;
    }
    static std::string hex(const Stamp& stamp) {
        static const char digits[]="0123456789abcdef";std::string text;
        for(const auto b:stamp) {text.push_back(digits[b>>4]);text.push_back(digits[b&15]);}
        return text;
    }
    // Replaces, adds or (without time stamps) removes the line of `name` in a metadata file.
    static void setLine(const std::filesystem::path& file,const std::string& name,std::uint16_t mode,const Stamp* created,const Stamp* modified) {
        std::vector<std::string> lines;
        {
            std::ifstream in(file);
            for(std::string line;std::getline(in,line);) {
                if(!line.empty() && line.back()=='\r')line.pop_back();
                std::istringstream fields(line);std::string first;
                if(fields>>first && first!=name)lines.push_back(line);
            }
        }
        if(created && modified) {
            char mode_text[5];std::snprintf(mode_text,sizeof mode_text,"%04x",unsigned(mode));
            lines.push_back(name+" "+mode_text+" "+hex(*created)+" "+hex(*modified));
        }
        std::ofstream out(file,std::ios::binary|std::ios::trunc);
        for(const auto& line:lines)out<<line<<'\n';
    }
    // The host path of a card path, or empty when it would leave the card or uses characters a card name cannot have.
    std::filesystem::path resolve(const std::string& name) const {
        std::vector<std::string> parts;
        if(!name.starts_with("/")) {std::istringstream current(current_);for(std::string part;std::getline(current,part,'/');)if(!part.empty())parts.push_back(part);}
        std::istringstream in(name);
        for(std::string part;std::getline(in,part,'/');) {
            if(part.empty() || part==".")continue;
            if(part=="..") {if(parts.empty())return {};parts.pop_back();continue;}
            if(part.find_first_of("\\:*?\"<>|")!=std::string::npos)return {};
            parts.push_back(part);
        }
        auto path=root_;
        for(const auto& part:parts)path/=part;
        return path;
    }
    Entry entry(const std::filesystem::path& path) const {
        std::error_code error;
        Entry e;e.name=path.filename().string();
        const bool folder=std::filesystem::is_directory(path,error);
        e.mode=folder?folder_mode:file_mode;
        e.size=folder?0u:std::uint32_t(std::filesystem::file_size(path,error));
        // Mode and time stamps from the save folder's entries.txt, where it has the entry.
        std::ifstream meta((folder?path:path.parent_path())/"entries.txt");
        const std::string wanted=folder?".":e.name;
        for(std::string line;std::getline(meta,line);) {
            std::istringstream fields(line);std::string name,mode,created,modified;
            if(!(fields>>name>>mode>>created>>modified) || name!=wanted || mode.size()!=4 || created.size()!=16 || modified.size()!=16)continue;
            try {
                const auto value=std::uint16_t(std::stoul(mode,nullptr,16));
                if(value)e.mode=value;
                for(unsigned n=0;n<8;++n) {
                    e.created[n]=std::uint8_t(std::stoul(created.substr(n*2,2),nullptr,16));
                    e.modified[n]=std::uint8_t(std::stoul(modified.substr(n*2,2),nullptr,16));
                }
            } catch(const std::exception&) {} // A damaged line: the defaults stay.
            break;
        }
        return e;
    }
};
