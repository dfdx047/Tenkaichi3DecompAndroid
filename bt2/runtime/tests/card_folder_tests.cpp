// The memory card folder's file layer on a synthetic card: opening, reading, seeking, listing, saving,
// the current directory, result codes, and that nothing outside the folder can be named.
#include "services/card_folder.h"
#include "services/memory_card_info.h"
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace {
void check(bool value,const char* what) {if(!value){std::cerr<<"Card folder check failed: "<<what<<'\n';std::exit(1);}}
void put(const std::filesystem::path& path,const std::string& content) {std::ofstream(path,std::ios::binary)<<content;}
}

int main() {
    {
        // GetInfo by port: the card folder answers in port 0 (a new card first, then 0); port 1 and a
        // port 0 without a folder are empty (-11, type 0); a multitap slot or a third port is refused.
        const auto first=cardInfo(0,0,true,false,1000),later=cardInfo(0,0,true,true,1000);
        check(first && first->reply==0xFFFFFFFFu && first->type==2 && first->free_clusters==1000 && first->formatted==1,"a card folder in port 0");
        check(later && later->reply==0,"the card folder reported before");
        const auto empty=cardInfo(1,0,true,false,1000),none=cardInfo(0,0,false,false,0);
        check(empty && empty->reply==0xFFFFFFF5u && !empty->type && !empty->free_clusters && !empty->formatted,"port 1 is empty");
        check(none && none->reply==0xFFFFFFF5u,"port 0 without a folder is empty");
        check(!cardInfo(0,1,true,false,0) && !cardInfo(2,0,true,false,0),"a multitap slot or a third port");
    }
    const auto root=std::filesystem::temp_directory_path()/"bt2-card-folder-tests";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root/"card"/"SAVE-ONE");
    std::filesystem::create_directories(root/"card"/"SAVE-TWO");
    put(root/"outside.txt","not on the card");
    put(root/"card"/"SAVE-ONE"/"data","0123456789");
    put(root/"card"/"SAVE-ONE"/"icon.sys","icon");
    put(root/"card"/"SAVE-ONE"/"entries.txt",". 8427 0102030405060708 1112131415161718\ndata 8417 2122232425262728 3132333435363738\nbroken line\n");
    CardFolder card(root/"card");

    // Reading a file by its full path, in pieces, with seeks.
    const int fd=card.open("/SAVE-ONE/data",1);
    check(fd==0,"the first file gets descriptor 0");
    std::array<std::uint8_t,4> part{};
    check(card.read(fd,part)==4 && part[0]=='0' && part[3]=='3',"the first bytes");
    check(card.seek(fd,2,1)==6 && card.read(fd,part)==4 && part[0]=='6' && part[3]=='9',"a seek from the position");
    check(card.read(fd,part)==0,"nothing at the end");
    check(card.seek(fd,-3,2)==7 && card.read(fd,part)==3 && part[0]=='7',"a seek from the end; a short read");
    check(card.seek(fd,0,0)==0 && card.seek(fd,11,0)==CardFolder::denied && card.seek(fd,-1,0)==CardFolder::denied,"seeks stay inside the file");
    check(card.close(fd)==0 && card.close(fd)==CardFolder::no_entry && card.read(fd,part)==CardFolder::no_entry,"a closed descriptor is gone");

    // Result codes: missing files, folders, writing, too many files, the metadata file, leaving the card.
    check(card.open("/SAVE-ONE/none",1)==CardFolder::no_entry && card.open("/SAVE-ONE",1)==CardFolder::no_entry,"no such file; a folder is not a file");
    check(card.open("/SAVE-ONE/data",0)==CardFolder::denied && card.open("/SAVE-ONE/data",0x801)==CardFolder::denied,"flags that are not the library's are refused");
    check(card.open("/SAVE-ONE/entries.txt",1)==CardFolder::no_entry,"the metadata file is not on the card");
    check(card.open("/../outside.txt",1)==CardFolder::no_entry && card.open("/SAVE-ONE/../../outside.txt",1)==CardFolder::no_entry,"nothing outside the card");
    check(card.open("/SAVE-ONE/da:ta",1)==CardFolder::no_entry,"characters a card name cannot have");
    check(card.open("/SAVE-ONE/data",1)==0 && card.open("/SAVE-ONE/data",1)==1 && card.open("/SAVE-ONE/icon.sys",1)==2,"three files at once");
    check(card.open("/SAVE-ONE/data",1)==CardFolder::too_many_open,"not a fourth");
    for(int n=0;n<3;++n)card.close(n);

    // Listing: the top, a save's folder with "." and ".." first, a single entry, modes and time stamps.
    const auto top=card.list("/*");
    check(top.size()==2 && top[0].name=="SAVE-ONE" && top[1].name=="SAVE-TWO" && top[0].mode==0x8427 && top[0].created[0]==1 && top[0].modified[7]==0x18,"the saves on the card, with the folder's own entry data");
    check(top[1].mode==CardFolder::folder_mode && top[1].created[0]==0,"defaults without metadata");
    const auto inside=card.list("/SAVE-ONE/*");
    check(inside.size()==4 && inside[0].name=="." && inside[1].name==".." && inside[2].name=="data" && inside[3].name=="icon.sys","a save's files after . and ..");
    check(inside[2].size==10 && inside[2].mode==0x8417 && inside[2].created[0]==0x21 && inside[2].modified[7]==0x38,"a file's size, mode and time stamps");
    check(inside[3].size==4 && inside[3].mode==CardFolder::file_mode,"a file without metadata");
    const auto one=card.list("/SAVE-ONE/data");
    check(one.size()==1 && one[0].name=="data" && card.list("/SAVE-ONE/none").empty() && card.list("/NONE/*").empty(),"a single entry; nothing for what is not there");

    // The current directory: relative names follow it.
    check(card.currentDirectory()=="/" && card.changeDirectory("/SAVE-ONE")==0 && card.currentDirectory()=="/SAVE-ONE","into a save's folder");
    const int relative=card.open("data",1);
    check(relative==0 && card.list("*").size()==4,"relative names");
    card.close(relative);
    check(card.changeDirectory("..")==0 && card.currentDirectory()=="/" && card.changeDirectory("..")==CardFolder::no_entry,"up to the top and no further");
    check(card.changeDirectory("/SAVE-ONE/data")==CardFolder::no_entry && card.changeDirectory("/NONE")==CardFolder::no_entry,"only folders");

    // Saving: a new save folder, a new file written in pieces, an existing file changed in place and
    // emptied, the metadata kept current, free space, deleting.
    const auto text=[&](const char* name) {std::ifstream in(root/"card"/name,std::ios::binary);return std::string(std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>());};
    const auto bytes=[](const char* s) {return std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(s),std::strlen(s));};
    card.now=[]{return CardFolder::Stamp{0,1,2,3,4,5,0xEA,7};};
    const auto free_before=card.freeClusters();
    check(free_before==CardFolder::clusters-2-2-3-3,"free space: two folders and two small files");
    check(card.open("/SAVE-NEW",0x40)==0 && std::filesystem::is_directory(root/"card"/"SAVE-NEW"),"a save folder is made");
    check(card.open("/SAVE-NEW",0x40)==CardFolder::no_entry && card.open("/NONE/DEEP",0x40)==CardFolder::no_entry,"not twice, and only in a folder that exists");
    check(card.list("/SAVE-NEW/*").size()==2 && card.list("/SAVE-NEW/*")[0].modified[6]==0xEA,"it lists as empty, with the time it was made");
    check(card.open("/SAVE-NEW/file",1)==CardFolder::no_entry && card.open("/SAVE-NEW/file",2)==CardFolder::no_entry && card.open("/SAVE-NEW/file",0x201)==CardFolder::no_entry,"a missing file is made only when writing with the create flag");
    check(card.open("/top-file",0x202)==CardFolder::no_entry && card.open("/SAVE-NEW/entries.txt",0x202)==CardFolder::no_entry,"no files at the top of the card; the metadata file is not the game's");
    const int made=card.open("/SAVE-NEW/file",0x202);
    check(made==0 && text("SAVE-NEW/file").empty(),"a new file is on the card at once, empty");
    check(card.write(made,bytes("hello "))==6 && card.write(made,bytes("card"))==4 && text("SAVE-NEW/file").empty(),"written data is held until a flush or close");
    check(card.read(made,part)==CardFolder::denied,"a file opened for writing only cannot be read");
    check(card.flush(made)==0 && text("SAVE-NEW/file")=="hello card","a flush writes it");
    check(card.remove("/SAVE-NEW/file")==CardFolder::denied,"an open file is not deleted");
    check(card.seek(made,0,0)==0 && card.write(made,bytes("J"))==1 && card.close(made)==0 && text("SAVE-NEW/file")=="Jello card","a write at a position; close writes it");
    check(!std::filesystem::exists(root/"card"/"SAVE-NEW"/"file.bt2-new") && card.list("/SAVE-NEW/*").size()==3,"no temporary file stays");
    const auto written=card.list("/SAVE-NEW/file");
    check(written.size()==1 && written[0].size==10 && written[0].mode==CardFolder::file_mode && written[0].created[6]==0xEA && written[0].modified[7]==7,"the new file's entry");
    check(card.freeClusters()==free_before-2-3,"the folder and the file take space");
    const int both=card.open("/SAVE-ONE/data",3);
    check(both==0 && card.seek(both,8,0)==8 && card.write(both,bytes("abcd"))==4 && card.seek(both,0,0)==0 && card.read(both,part)==4 && part[0]=='0',"reading and writing one file; it grows");
    check(card.close(both)==0 && text("SAVE-ONE/data")=="01234567abcd","the changed file");
    const auto changed=card.list("/SAVE-ONE/data");
    check(changed[0].mode==0x8417 && changed[0].created[0]==0x21 && changed[0].modified[6]==0xEA,"its mode and creation time stay, the modification time is new");
    check(card.list("/SAVE-ONE/*")[0].created[0]==1,"the folder's own line stays");
    const int emptied=card.open("/SAVE-ONE/icon.sys",0x602);
    check(emptied==0 && card.write(emptied,bytes("new"))==3 && card.close(emptied)==0 && text("SAVE-ONE/icon.sys")=="new","an existing file is emptied by the truncate flag");
    const int read_only=card.open("/SAVE-ONE/data",1);
    check(card.write(read_only,bytes("x"))==CardFolder::denied && card.close(read_only)==0 && text("SAVE-ONE/data")=="01234567abcd","a file opened for reading is not written");
    // An entry's data: only the named parts change.
    CardFolder::Entry info;info.mode=0x8497;info.created.fill(0x44);info.modified.fill(0x55);
    check(card.setInfo("/SAVE-NEW/file",info,2)==0,"setting an entry's data");
    const auto set=card.list("/SAVE-NEW/file");
    check(set[0].modified[0]==0x55 && set[0].created[6]==0xEA && set[0].mode==CardFolder::file_mode,"only the modification time");
    check(card.setInfo("/SAVE-NEW",info,5)==0 && card.list("/SAVE-NEW/*")[0].created[0]==0x44 && card.list("/SAVE-NEW/*")[0].modified[6]==0xEA && card.list("/SAVE-NEW/*")[0].mode==0x8497,"a folder's creation time and mode");
    check(card.setInfo("/SAVE-NEW/none",info,7)==CardFolder::no_entry,"nothing to set");
    // A card has room for its clusters only.
    const int big=card.open("/SAVE-NEW/big",0x202);
    const std::vector<std::uint8_t> megabyte(1u<<20);
    int stored=0;while(card.write(big,megabyte)==int(megabyte.size()) && stored<20) {card.flush(big);++stored;}
    check(stored==7,"seven megabytes fit on the card, the eighth does not");
    check(card.write(big,megabyte)==CardFolder::full && card.close(big)==0,"a full card is reported");
    // Deleting: files, then the empty folder.
    check(card.remove("/SAVE-NEW")==CardFolder::not_empty && card.remove("/SAVE-NEW/none")==CardFolder::no_entry && card.remove("/")==CardFolder::no_entry,"a folder with files stays");
    check(card.remove("/SAVE-NEW/big")==0 && card.remove("/SAVE-NEW/file")==0 && card.list("/SAVE-NEW/*").size()==2,"files are deleted");
    check(card.remove("/SAVE-NEW")==0 && !std::filesystem::exists(root/"card"/"SAVE-NEW") && card.freeClusters()==free_before,"then the folder, and the space is back");

    std::filesystem::remove_all(root);
    std::cout<<"Card folder checks passed.\n";
}
