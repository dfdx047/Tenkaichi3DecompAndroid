// Host-only TLS signaling/audio fallback. No game state or pad queues here.
#include <winhttp.h>
#include <deque>
#include <vector>
#include <string>
#include <atomic>
#include <mutex>
#include <memory>
#include <array>
struct VoiceWsMessage {std::vector<unsigned char> bytes;bool binary;Uint64 at;};
class VoiceWs {
    std::mutex lock;std::deque<VoiceWsMessage> in,out;
    SDL_Thread *rx=nullptr,*tx=nullptr;std::atomic<bool> stop{false};
    std::string url,cap,name;int seat=0;
    bool orderedFrames=false;void (*notify)(void*)=nullptr;void *notifyContext=nullptr;
    HANDLE sendWake=nullptr;
    std::atomic<HINTERNET> session{nullptr},connection{nullptr},request{nullptr},ws{nullptr};
    bool adopt(std::atomic<HINTERNET> &slot,HINTERNET handle){if(!handle)return false;slot=handle;if(stop){if(auto h=slot.exchange(nullptr))WinHttpCloseHandle(h);return false;}return true;}
    static void closeHandle(std::atomic<HINTERNET> &slot){if(auto h=slot.exchange(nullptr))WinHttpCloseHandle(h);}
    static int receiveThread(void *p){return static_cast<VoiceWs*>(p)->receive();}
    static int sendThread(void *p){return static_cast<VoiceWs*>(p)->sendLoop();}
    int receive(){
        wchar_t wide[1400],host[256],path[1000],extra[1000];URL_COMPONENTS u={};u.dwStructSize=sizeof(u);u.lpszHostName=host;u.dwHostNameLength=256;u.lpszUrlPath=path;u.dwUrlPathLength=1000;u.lpszExtraInfo=extra;u.dwExtraInfoLength=1000;
        MultiByteToWideChar(CP_UTF8,0,url.c_str(),-1,wide,1400);
        if(!WinHttpCrackUrl(wide,0,0,&u))return fail();
        if(!adopt(session,WinHttpOpen(L"BT4-Voice/1",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0)))return fail();
        // This socket is long lived. The receive deadline must exceed the
        // Realtime heartbeat interval (20 s), or an otherwise idle channel
        // can be torn down before its next heartbeat reply.
        if(!WinHttpSetTimeouts(session,10000,10000,15000,30000)){
            fprintf(stderr,"bt4: realtime: cannot set WebSocket timeouts (%lu)\n",GetLastError());return fail();}
        if(!adopt(connection,WinHttpConnect(session,host,u.nPort,0)))return fail();
        std::wstring fullPath(path,u.dwUrlPathLength);fullPath.append(extra,u.dwExtraInfoLength);
        if(!adopt(request,WinHttpOpenRequest(connection,L"GET",fullPath.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,u.nScheme==INTERNET_SCHEME_HTTPS?WINHTTP_FLAG_SECURE:0)))return fail();
        std::string headers=cap.empty()?std::string():"Authorization: Bearer "+cap+"\r\n";
        headers+="X-BT4-Seat: "+std::to_string(seat)+"\r\nX-BT4-Name: ";
        for(unsigned char c:name){if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9'))headers+=c;else {char h[4];snprintf(h,4,"%%%02X",c);headers+=h;}}
        headers+="\r\n";wchar_t wh[1024];MultiByteToWideChar(CP_UTF8,0,headers.c_str(),-1,wh,1024);
        if(!WinHttpSetOption(request,WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET,nullptr,0)||!WinHttpSendRequest(request,wh,(DWORD)-1,WINHTTP_NO_REQUEST_DATA,0,0,0)||!WinHttpReceiveResponse(request,nullptr)){
            if(!stop)fprintf(stderr,"bt4: realtime: WebSocket handshake failed (%lu)\n",GetLastError());return fail();}
        DWORD code=0,size=sizeof(code);WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&code,&size,WINHTTP_NO_HEADER_INDEX);
        if(code!=101){fprintf(stderr,"bt4: realtime: WebSocket handshake returned HTTP %lu\n",code);return fail();}
        if(!adopt(ws,WinHttpWebSocketCompleteUpgrade(request,0)))return fail();status=1;if(sendWake)SetEvent(sendWake);
        while(!stop){
            unsigned char data[16385];DWORD got=0;unsigned used=0;WINHTTP_WEB_SOCKET_BUFFER_TYPE kind;
            do{
                DWORD received=WinHttpWebSocketReceive(ws,data+used,sizeof(data)-used,&got,&kind);
                if(received!=NO_ERROR){if(!stop)fprintf(stderr,"bt4: realtime: WebSocket receive failed (%lu)\n",received);return fail();}used+=got;
                if(kind==WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE||used>16384)return fail();
            }while(kind==WINHTTP_WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE||kind==WINHTTP_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE);
            if(kind!=WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE&&kind!=WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE)return fail();
            {std::lock_guard<std::mutex> g(lock);
                if(in.size()>=(orderedFrames?2048u:64u)){if(orderedFrames)return fail();in.pop_front();}
                in.push_back({std::vector<unsigned char>(data,data+used),kind==WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE,SDL_GetTicks()});}
            if(notify)notify(notifyContext);
        }return 0;
    }
    int fail(){if(!stop)status=-1;return 0;}
    int sendLoop(){
        while(!stop){
            if(status<0)break;VoiceWsMessage msg;bool have=false;
            if(status==1){std::lock_guard<std::mutex> g(lock);if(!out.empty()){msg=std::move(out.front());out.pop_front();have=true;}}
            if(have){if(!orderedFrames&&msg.binary&&SDL_GetTicks()-msg.at>160)continue;
                if(WinHttpWebSocketSend(ws,msg.binary?WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE:WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE,msg.bytes.data(),(DWORD)msg.bytes.size())!=NO_ERROR)return fail();
            }else if(sendWake)WaitForSingleObject(sendWake,1000);else SDL_Delay(2);
        }return 0;
    }
public:
    std::atomic<int> status{0};
    VoiceWs(const std::string &u,const std::string &c,int s,const std::string &n,bool game=false,void (*wake)(void*)=nullptr,void *context=nullptr):url(u),cap(c),name(n),seat(s),orderedFrames(game),notify(wake),notifyContext(context){if(game){sendWake=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!sendWake){status=-1;return;}}rx=SDL_CreateThread(receiveThread,"BT4 TLS receive",this);tx=SDL_CreateThread(sendThread,"BT4 TLS send",this);if(!rx||!tx)status=-1;}
    ~VoiceWs(){stop=true;if(sendWake)SetEvent(sendWake);closeHandle(ws);closeHandle(request);if(tx)SDL_WaitThread(tx,nullptr);if(rx)SDL_WaitThread(rx,nullptr);closeHandle(ws);closeHandle(request);closeHandle(connection);closeHandle(session);if(sendWake)CloseHandle(sendWake);}
    bool read(VoiceWsMessage &m){std::lock_guard<std::mutex>g(lock);if(in.empty())return false;m=std::move(in.front());in.pop_front();return true;}
    void send(const void *data,size_t size,bool binary){
        if(status<0||size>16384)return;std::lock_guard<std::mutex>g(lock);
        if(binary&&!orderedFrames){size_t audio=0;for(const auto &m:out)if(m.binary)++audio;if(audio>=8){for(auto it=out.begin();it!=out.end();++it)if(it->binary){out.erase(it);break;}}}
        if(out.size()>=(orderedFrames?2048u:64u)){if(orderedFrames)status=-1;return;}const auto *p=static_cast<const unsigned char*>(data);out.push_back({std::vector<unsigned char>(p,p+size),binary,SDL_GetTicks()});if(sendWake)SetEvent(sendWake);
    }
    void text(const std::string &s){send(s.data(),s.size(),false);}
};
