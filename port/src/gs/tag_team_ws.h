/* Native TLS WebSocket transport. OS handles and queues stay outside the PS2 heap. */
#ifdef _WIN32
#include <winhttp.h>
#include <SDL3/SDL.h>
#define TT_WS_RING 1024
static struct {
    CRITICAL_SECTION lock;int lockReady;HANDLE rx,tx,sendWake,receiveWake;
    HINTERNET session,connection,request,websocket;
    volatile LONG stop,status;
    unsigned inRead,inWrite,outRead,outWrite;
    unsigned char incoming[TT_WS_RING][512],outgoing[TT_WS_RING][512];
    Uint64 incomingAt[TT_WS_RING];
    char url[512];
} sTagWs;
/* Closing may race a connection handshake. Publish every OS handle under the
 * same lock, so a cancelled handshake cannot leak or revive a closed session. */
static int TagWs_Publish(HINTERNET *slot,HINTERNET handle){
    int ok=0;if(!handle)return 0;
    EnterCriticalSection(&sTagWs.lock);
    if(!InterlockedCompareExchange(&sTagWs.stop,0,0)){*slot=handle;ok=1;}
    LeaveCriticalSection(&sTagWs.lock);
    if(!ok)WinHttpCloseHandle(handle);return ok;
}
static DWORD WINAPI TagWs_Rx(LPVOID unused){
    wchar_t url[512],host[256],path[256];URL_COMPONENTS parts;
    HINTERNET request=NULL;DWORD code=0,size=sizeof(code);
    (void)unused;if(InterlockedCompareExchange(&sTagWs.stop,0,0))return 0;memset(&parts,0,sizeof(parts));parts.dwStructSize=sizeof(parts);
    parts.lpszHostName=host;parts.dwHostNameLength=256;parts.lpszUrlPath=path;parts.dwUrlPathLength=256;
    MultiByteToWideChar(CP_UTF8,0,sTagWs.url,-1,url,512);
    if(!WinHttpCrackUrl(url,0,0,&parts))goto failed;
    if(!TagWs_Publish(&sTagWs.session,WinHttpOpen(L"BT4-Tag-Team/0.1.39",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0)))goto failed;
    WinHttpSetTimeouts(sTagWs.session,5000,5000,5000,10000);
    if(!TagWs_Publish(&sTagWs.connection,WinHttpConnect(sTagWs.session,host,parts.nPort,0)))goto failed;
    request=WinHttpOpenRequest(sTagWs.connection,L"GET",path,NULL,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,parts.nScheme==INTERNET_SCHEME_HTTPS?WINHTTP_FLAG_SECURE:0);
    if(!TagWs_Publish(&sTagWs.request,request))goto failed;
    if(!WinHttpSetOption(request,WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET,NULL,0) || !WinHttpSendRequest(request,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0) || !WinHttpReceiveResponse(request,NULL))goto failed;
    WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&code,&size,WINHTTP_NO_HEADER_INDEX);
    if(code!=101)goto failed;
    if(!TagWs_Publish(&sTagWs.websocket,WinHttpWebSocketCompleteUpgrade(request,0)))goto failed;
    if(InterlockedCompareExchange(&sTagWs.stop,0,0))return 0;
    InterlockedExchange(&sTagWs.status,1);
    SetEvent(sTagWs.sendWake);
    while(!InterlockedCompareExchange(&sTagWs.stop,0,0)){
        unsigned char packet[512];unsigned used=0;WINHTTP_WEB_SOCKET_BUFFER_TYPE kind;DWORD got;
        do{
            if(WinHttpWebSocketReceive(sTagWs.websocket,packet+used,sizeof(packet)-used,&got,&kind)!=NO_ERROR)goto failed;
            used+=got;if(kind==WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE)goto failed;
            if(used>512 || (used==512 && kind!=WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE))goto failed;
        }while(kind==WINHTTP_WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE);
        if(used!=512 || kind!=WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE)goto failed;
        EnterCriticalSection(&sTagWs.lock);
        if(sTagWs.inWrite-sTagWs.inRead>=TT_WS_RING){LeaveCriticalSection(&sTagWs.lock);goto failed;}
        sTagWs.incomingAt[sTagWs.inWrite%TT_WS_RING]=SDL_GetTicks();
        memcpy(sTagWs.incoming[sTagWs.inWrite++%TT_WS_RING],packet,512);LeaveCriticalSection(&sTagWs.lock);
        SetEvent(sTagWs.receiveWake);
    }
    return 0;
failed:
    if(!InterlockedCompareExchange(&sTagWs.stop,0,0))InterlockedExchange(&sTagWs.status,-1);
    return 0;
}
static DWORD WINAPI TagWs_Tx(LPVOID unused){
    (void)unused;
    while(!InterlockedCompareExchange(&sTagWs.stop,0,0)){
        unsigned char packet[512];int pending=0;
        if(InterlockedCompareExchange(&sTagWs.status,0,0)<0)break;
        if(InterlockedCompareExchange(&sTagWs.status,0,0)==1){
            EnterCriticalSection(&sTagWs.lock);
            if(sTagWs.outRead<sTagWs.outWrite){memcpy(packet,sTagWs.outgoing[sTagWs.outRead++%TT_WS_RING],512);pending=1;}
            LeaveCriticalSection(&sTagWs.lock);
            if(pending && WinHttpWebSocketSend(sTagWs.websocket,WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE,packet,512)!=NO_ERROR){InterlockedExchange(&sTagWs.status,-1);break;}
        }
        if(!pending)WaitForSingleObject(sTagWs.sendWake,1000);
    }return 0;
}
static void TagWs_Close(void){
    HINTERNET websocket=NULL,request=NULL;
    InterlockedExchange(&sTagWs.stop,1);
    if(sTagWs.sendWake)SetEvent(sTagWs.sendWake);
    if(sTagWs.receiveWake)SetEvent(sTagWs.receiveWake);
    if(sTagWs.lockReady){
        EnterCriticalSection(&sTagWs.lock);
        websocket=sTagWs.websocket;request=sTagWs.request;
        LeaveCriticalSection(&sTagWs.lock);
    }
    if(websocket)WinHttpCloseHandle(websocket);
    if(request)WinHttpCloseHandle(request);
    if(sTagWs.tx){WaitForSingleObject(sTagWs.tx,INFINITE);CloseHandle(sTagWs.tx);}
    if(sTagWs.rx){WaitForSingleObject(sTagWs.rx,INFINITE);CloseHandle(sTagWs.rx);}
    if(sTagWs.connection)WinHttpCloseHandle(sTagWs.connection);
    if(sTagWs.session)WinHttpCloseHandle(sTagWs.session);
    if(sTagWs.sendWake)CloseHandle(sTagWs.sendWake);
    if(sTagWs.receiveWake)CloseHandle(sTagWs.receiveWake);
    if(sTagWs.lockReady)DeleteCriticalSection(&sTagWs.lock);
    memset(&sTagWs,0,sizeof(sTagWs));
}
static int TagWs_Open(const char *root,const char *code){
    TagWs_Close();InitializeCriticalSection(&sTagWs.lock);sTagWs.lockReady=1;
    sTagWs.sendWake=CreateEventW(NULL,FALSE,FALSE,NULL);sTagWs.receiveWake=CreateEventW(NULL,FALSE,FALSE,NULL);
    if(!sTagWs.sendWake||!sTagWs.receiveWake){TagWs_Close();return 0;}
    snprintf(sTagWs.url,sizeof(sTagWs.url),"%s/v2/room/%s",root,code);
    sTagWs.rx=CreateThread(NULL,0,TagWs_Rx,NULL,0,NULL);sTagWs.tx=CreateThread(NULL,0,TagWs_Tx,NULL,0,NULL);
    if(!sTagWs.rx||!sTagWs.tx){TagWs_Close();return 0;}return 1;
}
static int TagWs_Send(const unsigned char *packet){
    int ok=1;if(!sTagWs.lockReady || InterlockedCompareExchange(&sTagWs.status,0,0)<0)return 0;
    EnterCriticalSection(&sTagWs.lock);
    if(sTagWs.outWrite-sTagWs.outRead>=TT_WS_RING)ok=0;
    else memcpy(sTagWs.outgoing[sTagWs.outWrite++%TT_WS_RING],packet,512);
    LeaveCriticalSection(&sTagWs.lock);if(ok)SetEvent(sTagWs.sendWake);return ok;
}
static int TagWs_Read(unsigned char *packet,Uint64 *arrived){
    int result=0;if(!sTagWs.lockReady)return 0;
    EnterCriticalSection(&sTagWs.lock);
    if(sTagWs.inRead<sTagWs.inWrite){if(arrived)*arrived=sTagWs.incomingAt[sTagWs.inRead%TT_WS_RING];memcpy(packet,sTagWs.incoming[sTagWs.inRead++%TT_WS_RING],512);result=1;}
    LeaveCriticalSection(&sTagWs.lock);
    return result?1:InterlockedCompareExchange(&sTagWs.status,0,0)<0?-1:0;
}
static void TagWs_QueueCounts(unsigned *rx,unsigned *tx){
    *rx=*tx=0;if(!sTagWs.lockReady)return;
    EnterCriticalSection(&sTagWs.lock);*rx=sTagWs.inWrite-sTagWs.inRead;*tx=sTagWs.outWrite-sTagWs.outRead;LeaveCriticalSection(&sTagWs.lock);
}
static void TagWs_Wait(unsigned ms){
    unsigned rx,tx;TagWs_QueueCounts(&rx,&tx);
    if(!rx&&sTagWs.receiveWake)WaitForSingleObject(sTagWs.receiveWake,ms);
}
#else
#include "curl_ws_io.h"
#define TT_WS_RING 1024
static struct {
    SDL_Mutex *lock;SDL_Condition *receiveReady;SDL_Thread *thread;
    SDL_AtomicInt stop,status;
    unsigned inRead,inWrite,outRead,outWrite;
    unsigned char incoming[TT_WS_RING][512],outgoing[TT_WS_RING][512];
    Uint64 incomingAt[TT_WS_RING];
    char url[512];Bt4CurlWs io;
} sTagWs;
static int TagWs_Thread(void *unused){
    unsigned char outgoing[512],incoming[512],chunk[512];size_t sentAt=0,used=0;
    int sending=0,connected=0,failed=0;
    (void)unused;
    SDL_LockMutex(sTagWs.lock);
    if(!Bt4CurlWs_Open(&sTagWs.io,sTagWs.url,NULL))failed=1;
    SDL_UnlockMutex(sTagWs.lock);
    while(!failed&&!SDL_GetAtomicInt(&sTagWs.stop)){
        int worked=0,reads=0;
        if(!connected){
            connected=Bt4CurlWs_Connect(&sTagWs.io);
            if(connected<0){failed=1;break;}
            if(!connected){if(!Bt4CurlWs_Wait(&sTagWs.io,0,100))failed=1;continue;}
            SDL_SetAtomicInt(&sTagWs.status,1);
        }
        if(!sending){
            SDL_LockMutex(sTagWs.lock);
            if(sTagWs.outRead!=sTagWs.outWrite){memcpy(outgoing,sTagWs.outgoing[sTagWs.outRead++%TT_WS_RING],512);sending=1;sentAt=0;}
            SDL_UnlockMutex(sTagWs.lock);
        }
        if(sending){
            size_t sent=0;CURLcode result=curl_ws_send(sTagWs.io.easy,outgoing+sentAt,512-sentAt,&sent,0,CURLWS_BINARY);
            if(result!=CURLE_OK&&result!=CURLE_AGAIN){failed=1;break;}
            if(sent){sentAt+=sent;worked=1;if(sentAt==512)sending=0;}
        }
        while(reads++<64){
            const struct curl_ws_frame *meta=NULL;size_t got=0;
            CURLcode result=curl_ws_recv(sTagWs.io.easy,chunk,sizeof(chunk),&got,&meta);
            if(result==CURLE_AGAIN)break;
            if(result!=CURLE_OK||!meta||(meta->flags&(CURLWS_CLOSE|CURLWS_TEXT))){failed=1;break;}
            worked=1;
            if(meta->flags&(CURLWS_PING|CURLWS_PONG))continue;
            if(used+got>512){failed=1;break;}
            memcpy(incoming+used,chunk,got);used+=got;
            if(meta->bytesleft==0&&!(meta->flags&CURLWS_CONT)){
                if(used!=512){failed=1;break;}
                SDL_LockMutex(sTagWs.lock);
                if(sTagWs.inWrite-sTagWs.inRead>=TT_WS_RING)failed=1;
                else{
                    sTagWs.incomingAt[sTagWs.inWrite%TT_WS_RING]=SDL_GetTicks();
                    memcpy(sTagWs.incoming[sTagWs.inWrite++%TT_WS_RING],incoming,512);
                    SDL_SignalCondition(sTagWs.receiveReady);
                }
                SDL_UnlockMutex(sTagWs.lock);used=0;
                if(failed)break;
            }
        }
        if(!worked&&!failed&&!Bt4CurlWs_Wait(&sTagWs.io,sending,1000))failed=1;
    }
    if(failed&&!SDL_GetAtomicInt(&sTagWs.stop))SDL_SetAtomicInt(&sTagWs.status,-1);
    SDL_LockMutex(sTagWs.lock);Bt4CurlWs_Close(&sTagWs.io);SDL_BroadcastCondition(sTagWs.receiveReady);SDL_UnlockMutex(sTagWs.lock);
    return 0;
}
static void TagWs_Close(void){
    SDL_SetAtomicInt(&sTagWs.stop,1);
    if(sTagWs.lock){SDL_LockMutex(sTagWs.lock);Bt4CurlWs_Wake(&sTagWs.io);if(sTagWs.receiveReady)SDL_BroadcastCondition(sTagWs.receiveReady);SDL_UnlockMutex(sTagWs.lock);}
    if(sTagWs.thread)SDL_WaitThread(sTagWs.thread,NULL);
    if(sTagWs.receiveReady)SDL_DestroyCondition(sTagWs.receiveReady);
    if(sTagWs.lock)SDL_DestroyMutex(sTagWs.lock);
    memset(&sTagWs,0,sizeof(sTagWs));
}
static int TagWs_Open(const char *root,const char *code){
    TagWs_Close();sTagWs.lock=SDL_CreateMutex();sTagWs.receiveReady=SDL_CreateCondition();
    if(!sTagWs.lock||!sTagWs.receiveReady){TagWs_Close();return 0;}
    snprintf(sTagWs.url,sizeof(sTagWs.url),"%s/v2/room/%s",root,code);
    sTagWs.thread=SDL_CreateThread(TagWs_Thread,"BT4 Tag Team TLS",NULL);
    if(!sTagWs.thread){TagWs_Close();return 0;}return 1;
}
static int TagWs_Send(const unsigned char *packet){
    int ok=1;if(!sTagWs.lock||SDL_GetAtomicInt(&sTagWs.status)<0)return 0;
    SDL_LockMutex(sTagWs.lock);
    if(sTagWs.outWrite-sTagWs.outRead>=TT_WS_RING)ok=0;
    else memcpy(sTagWs.outgoing[sTagWs.outWrite++%TT_WS_RING],packet,512);
    if(ok)Bt4CurlWs_Wake(&sTagWs.io);
    SDL_UnlockMutex(sTagWs.lock);return ok;
}
static int TagWs_Read(unsigned char *packet,Uint64 *arrived){
    int result=0;if(!sTagWs.lock)return 0;
    SDL_LockMutex(sTagWs.lock);
    if(sTagWs.inRead!=sTagWs.inWrite){if(arrived)*arrived=sTagWs.incomingAt[sTagWs.inRead%TT_WS_RING];memcpy(packet,sTagWs.incoming[sTagWs.inRead++%TT_WS_RING],512);result=1;}
    SDL_UnlockMutex(sTagWs.lock);
    return result?1:SDL_GetAtomicInt(&sTagWs.status)<0?-1:0;
}
static void TagWs_QueueCounts(unsigned *rx,unsigned *tx){
    *rx=*tx=0;if(!sTagWs.lock)return;
    SDL_LockMutex(sTagWs.lock);*rx=sTagWs.inWrite-sTagWs.inRead;*tx=sTagWs.outWrite-sTagWs.outRead;SDL_UnlockMutex(sTagWs.lock);
}
static void TagWs_Wait(unsigned ms){
    if(!sTagWs.lock)return;
    SDL_LockMutex(sTagWs.lock);
    if(sTagWs.inRead==sTagWs.inWrite&&SDL_GetAtomicInt(&sTagWs.status)>=0)SDL_WaitConditionTimeout(sTagWs.receiveReady,sTagWs.lock,ms);
    SDL_UnlockMutex(sTagWs.lock);
}
#endif
