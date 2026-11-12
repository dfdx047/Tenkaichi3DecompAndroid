#include <pthread.h>
#include <assert.h>
#include <stdio.h>
#include <SDL3/SDL.h>
static pthread_t sThread;
static pthread_mutex_t sLock=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sDone=PTHREAD_COND_INITIALIZER;
static int sThreaded=1,sJob=2,releaseDraw,ready,waiting,returned,waitResult=-1;
#include "sync_under_test.inc"
static void *draw(void *unused){
    (void)unused;pthread_mutex_lock(&sLock);ready=1;pthread_cond_broadcast(&sDone);
    while(!releaseDraw)pthread_cond_wait(&sDone,&sLock);
    sJob=0;pthread_cond_broadcast(&sDone);pthread_mutex_unlock(&sLock);return NULL;
}
static void *unload(void *unused){
    (void)unused;pthread_mutex_lock(&sLock);waiting=1;pthread_cond_broadcast(&sDone);pthread_mutex_unlock(&sLock);
    int result=sceGsSyncPath(0,0);
    pthread_mutex_lock(&sLock);waitResult=result;returned=1;pthread_cond_broadcast(&sDone);pthread_mutex_unlock(&sLock);return NULL;
}
int main(void){
    pthread_t caller;assert(!pthread_create(&sThread,NULL,draw,NULL));
    pthread_mutex_lock(&sLock);while(!ready)pthread_cond_wait(&sDone,&sLock);pthread_mutex_unlock(&sLock);
    assert(sceGsSyncPath(1,0)==1);
    assert(!pthread_create(&caller,NULL,unload,NULL));
    pthread_mutex_lock(&sLock);while(!waiting)pthread_cond_wait(&sDone,&sLock);pthread_mutex_unlock(&sLock);
    SDL_Delay(20);
    pthread_mutex_lock(&sLock);assert(!returned);releaseDraw=1;pthread_cond_broadcast(&sDone);pthread_mutex_unlock(&sLock);
    pthread_join(sThread,NULL);pthread_join(caller,NULL);
    assert(returned&&waitResult==0&&sceGsSyncPath(1,0)==0);
    sThreaded=0;assert(sceGsSyncPath(0,0)==0);
    puts("Render fence: busy poll, delayed draw completion, blocking scene wait and single-thread path passed.");
    return 0;
}
