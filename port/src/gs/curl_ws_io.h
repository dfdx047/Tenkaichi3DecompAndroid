/* Native POSIX WebSocket I/O shared by room and voice transports.
 * Keep the completed CONNECT_ONLY easy handle in its multi handle: removing it
 * invalidates the connection used by curl_ws_recv/curl_ws_send. */
#ifndef BT4_CURL_WS_IO_H
#define BT4_CURL_WS_IO_H
#include <curl/curl.h>
#include <curl/websockets.h>
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>
typedef struct {
    CURL *easy;
    CURLM *multi;
    curl_socket_t socket;
    int connected;
    char url[2048],ca[2048],error[CURL_ERROR_SIZE];
} Bt4CurlWs;
static int Bt4CurlWs_Global(void) {
    static SDL_InitState init;
    if(SDL_ShouldInit(&init)){
        int ok=curl_global_init(CURL_GLOBAL_DEFAULT)==CURLE_OK;
        SDL_SetInitialized(&init,ok!=0);return ok;
    }
    return 1;
}
static void Bt4CurlWs_Close(Bt4CurlWs *w) {
    if(w->multi&&w->easy)curl_multi_remove_handle(w->multi,w->easy);
    if(w->easy)curl_easy_cleanup(w->easy);
    if(w->multi)curl_multi_cleanup(w->multi);
    w->easy=NULL;w->multi=NULL;w->connected=0;w->socket=CURL_SOCKET_BAD;
}
static int Bt4CurlWs_Open(Bt4CurlWs *w,const char *url,struct curl_slist *headers) {
    const char *base;SDL_IOStream *ca;
    memset(w,0,sizeof(*w));w->socket=CURL_SOCKET_BAD;
    /* One process-lifetime initialization per native transport translation unit. */
    if(!Bt4CurlWs_Global())return 0;
    if(!strncmp(url,"https://",8))snprintf(w->url,sizeof(w->url),"wss://%s",url+8);
    else if(!strncmp(url,"http://",7))snprintf(w->url,sizeof(w->url),"ws://%s",url+7);
    else snprintf(w->url,sizeof(w->url),"%s",url);
    w->easy=curl_easy_init();w->multi=curl_multi_init();
    if(!w->easy||!w->multi)goto failed;
    curl_easy_setopt(w->easy,CURLOPT_URL,w->url);
    curl_easy_setopt(w->easy,CURLOPT_HTTPHEADER,headers);
    curl_easy_setopt(w->easy,CURLOPT_CONNECT_ONLY,2L);
    curl_easy_setopt(w->easy,CURLOPT_NOSIGNAL,1L);
    curl_easy_setopt(w->easy,CURLOPT_CONNECTTIMEOUT_MS,5000L);
    curl_easy_setopt(w->easy,CURLOPT_TIMEOUT_MS,10000L);
    curl_easy_setopt(w->easy,CURLOPT_TCP_NODELAY,1L);
    curl_easy_setopt(w->easy,CURLOPT_SSL_VERIFYPEER,1L);
    curl_easy_setopt(w->easy,CURLOPT_SSL_VERIFYHOST,2L);
    curl_easy_setopt(w->easy,CURLOPT_ERRORBUFFER,w->error);
    curl_easy_setopt(w->easy,CURLOPT_USERAGENT,"BT4decomp/WebSocket");
    base=SDL_GetBasePath();snprintf(w->ca,sizeof(w->ca),"%slib/cacert.pem",base?base:"");
    ca=SDL_IOFromFile(w->ca,"rb");
    if(ca){SDL_CloseIO(ca);curl_easy_setopt(w->easy,CURLOPT_CAINFO,w->ca);}
    if(curl_multi_add_handle(w->multi,w->easy)!=CURLM_OK)goto failed;
    return 1;
failed:
    Bt4CurlWs_Close(w);return 0;
}
static int Bt4CurlWs_Connect(Bt4CurlWs *w) {
    int running,left;CURLMsg *message;
    if(w->connected)return 1;
    if(!w->multi||curl_multi_perform(w->multi,&running)!=CURLM_OK)return -1;
    while((message=curl_multi_info_read(w->multi,&left)))if(message->msg==CURLMSG_DONE){
        if(message->data.result!=CURLE_OK)return -1;
        if(curl_easy_getinfo(w->easy,CURLINFO_ACTIVESOCKET,&w->socket)!=CURLE_OK||w->socket==CURL_SOCKET_BAD)return -1;
        w->connected=1;return 1;
    }
    return running?0:-1;
}
static int Bt4CurlWs_Wait(Bt4CurlWs *w,int write,int timeout) {
    struct curl_waitfd fd;int ready;
    if(!w->multi)return 0;
    fd.fd=w->socket;fd.events=CURL_WAIT_POLLIN|(write?CURL_WAIT_POLLOUT:0);fd.revents=0;
    return curl_multi_poll(w->multi,w->connected?&fd:NULL,w->connected?1:0,timeout,&ready)==CURLM_OK;
}
static void Bt4CurlWs_Wake(Bt4CurlWs *w){if(w->multi)curl_multi_wakeup(w->multi);}
#endif
