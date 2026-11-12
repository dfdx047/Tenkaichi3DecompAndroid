/* Public release assets are downloaded directly over HTTPS. No GitHub sign-in
 * or account token is needed to install public language and texture packs. */
#include "language_packages.h"
static int sInstallMask=1;
static std::string url_escape_asset(const std::string &name){
    static const char hex[]="0123456789ABCDEF";
    std::string out;out.reserve(name.size()*3);
    for(unsigned char c:name){
        if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~')out.push_back((char)c);
        else{out.push_back('%');out.push_back(hex[c>>4]);out.push_back(hex[c&15]);}
    }
    return out;
}
static void download_public_asset(const std::string &root,const std::string &tag,const std::string &name,
                                 const std::string &path,Uint64 expected,int progressStart=0,int progressSpan=55){
    const std::string temp=path+".part";
    SDL_RemovePath(temp.c_str());
    SDL_RemovePath(path.c_str());
    const std::string url="https://github.com/"+std::string(kLanguageRepository)+"/releases/download/"+tag+"/"+url_escape_asset(name);
    std::vector<std::string> args;
#ifdef _WIN32
    args.emplace_back("curl.exe");
#elif defined(__APPLE__)
    args.emplace_back("/usr/bin/curl");
#else
    args.emplace_back("curl");
#endif
    args.insert(args.end(),{"--location","--fail","--silent","--show-error","--proto-redir","=https","--connect-timeout","25","--speed-limit","1024","--speed-time","60","--max-redirs","8","--output",temp,"--url",url});
    std::vector<const char*> argv;for(const auto &arg:args)argv.push_back(arg.c_str());argv.push_back(NULL);
    SDL_PropertiesID props=SDL_CreateProperties();
    SDL_SetPointerProperty(props,SDL_PROP_PROCESS_CREATE_ARGS_POINTER,(void*)argv.data());
    SDL_SetNumberProperty(props,SDL_PROP_PROCESS_CREATE_STDIN_NUMBER,SDL_PROCESS_STDIO_NULL);
    SDL_SetNumberProperty(props,SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER,SDL_PROCESS_STDIO_APP);
    SDL_SetBooleanProperty(props,SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN,true);
#ifdef _WIN32
    SDL_SetBooleanProperty(props,SDL_PROP_PROCESS_CREATE_BACKGROUND_BOOLEAN,true);
#endif
    SDL_SetStringProperty(props,SDL_PROP_PROCESS_CREATE_WORKING_DIRECTORY_STRING,root.c_str());
    SDL_Process *process=SDL_CreateProcessWithProperties(props);SDL_DestroyProperties(props);
    if(!process)throw Stop{"PUBLIC_DOWNLOAD_TOOL_MISSING"};
    std::string output;int code=0,last=-1;
    try{
        for(;;){
            char buf[1024];size_t n=SDL_ReadIO(SDL_GetProcessOutput(process),buf,sizeof(buf));
            if(n&&output.size()<8192)output.append(buf,n);
            SDL_PathInfo info;
            if(expected&&SDL_GetPathInfo(temp.c_str(),&info)){
                Uint64 got=std::min<Uint64>((Uint64)info.size,expected);
                int pct=progressStart+(int)(got*(Uint64)progressSpan/expected);
                if(pct!=last){last=pct;say("@package "+std::to_string(pct));}
            }
            if(SDL_WaitProcess(process,false,&code))break;
            check_cancel();SDL_Delay(100);
        }
        for(;;){char buf[1024];size_t n=SDL_ReadIO(SDL_GetProcessOutput(process),buf,sizeof(buf));if(!n)break;if(output.size()<8192)output.append(buf,n);}
        SDL_DestroyProcess(process);
    }catch(...){SDL_KillProcess(process,true);SDL_WaitProcess(process,true,&code);SDL_DestroyProcess(process);SDL_RemovePath(temp.c_str());throw;}
    SDL_PathInfo info;
    if(code||!SDL_GetPathInfo(temp.c_str(),&info)||info.type!=SDL_PATHTYPE_FILE||info.size!=expected){
        SDL_RemovePath(temp.c_str());
        throw Stop{"PUBLIC_DOWNLOAD_FAILED"+(output.empty()?std::string():std::string(": ")+output)};
    }
    if(!SDL_RenamePath(temp.c_str(),path.c_str())){SDL_RemovePath(temp.c_str());throw Stop{"PUBLIC_DOWNLOAD_FAILED: could not finalize the downloaded file."};}
}
static std::string sha256_file(const std::string &path){
    SDL_IOStream *f=SDL_IOFromFile(path.c_str(),"rb");
    if(!f)throw Stop{"LANGUAGE_DOWNLOAD_INCOMPLETE"};
    struct Cleanup {SDL_IOStream *file;~Cleanup(){SDL_CloseIO(file);}} cleanup{f};
    UpdSha256 hash;UpdSha256_Init(&hash);
    std::vector<Uint8> buf(1024*1024);size_t n;
    Uint64 read=0;Sint64 expected=SDL_GetIOSize(f);int last=-1;
    say("@activity verify-package");
    while((n=SDL_ReadIO(f,buf.data(),buf.size()))){
        check_cancel();UpdSha256_Add(&hash,buf.data(),n);read+=n;
        int pct=expected>0?55+(int)(read*10/(Uint64)expected):55;
        if(pct!=last){last=pct;say("@package "+std::to_string(pct));}
    }
    if(expected<0||read!=(Uint64)expected)throw Stop{"LANGUAGE_VERIFY_FAILED"};
    Uint8 digest[32];UpdSha256_End(&hash,digest);
    char out[65];for(int i=0;i<32;++i)snprintf(out+i*2,3,"%02x",digest[i]);return out;
}
static std::string download_language(const std::string &root,bool english){
    const LanguagePackage &package=kLanguagePackages[english?1:0];
    const std::string folder=root+"tbo/setup/cache/",path=folder+package.name;
    step("Language package");say("@package 0");say("@activity download");
    SDL_CreateDirectory(folder.c_str());
    SDL_PathInfo info;bool good=SDL_GetPathInfo(path.c_str(),&info)&&info.size==package.bytes;
    if(good)good=sha256_file(path)==package.sha256;
    if(!good){
        SDL_RemovePath(path.c_str());say("@activity download");say("@package 0");
        try{download_public_asset(root,kLanguageRelease,package.name,path,package.bytes);}
        catch(...){SDL_RemovePath(path.c_str());throw;}
        if(!SDL_GetPathInfo(path.c_str(),&info)||info.size!=package.bytes||sha256_file(path)!=package.sha256){SDL_RemovePath(path.c_str());throw Stop{"LANGUAGE_VERIFY_FAILED"};}
    }
    say("@package 65");return path;
}
