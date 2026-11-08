#ifndef MENU_TAG_CONTROLLERS_H
#define MENU_TAG_CONTROLLERS_H
/* IDs pin actual devices, never their shifting index in SDL's connected list.
   Negative IDs are keyboard binding profiles. Zero is an empty seat. */
typedef struct TagControllerSeats { int device[4], count; } TagControllerSeats;
static int TagSeats_Find(const TagControllerSeats *s,int device) {
    int i;
    if(!device) return -1;
    for(i=0;i<s->count;i++) if(s->device[i]==device) return i;
    return -1;
}
static int TagSeats_Join(TagControllerSeats *s,int device) {
    int i=TagSeats_Find(s,device);
    if(i>=0 || !device) return i;
    for(i=0;i<s->count;i++) if(!s->device[i]) {s->device[i]=device; return i;}
    return -1;
}
static void TagSeats_Leave(TagControllerSeats *s,int device) {
    int i=TagSeats_Find(s,device);
    if(i>=0) s->device[i]=0;
}
void Port_TagControllersBegin(int players);
void Port_TagControllersPoll(void);
int Port_TagControllersReady(int players);
int Port_TagControllersCommit(void);
void Port_TagControllersCancel(void);
void Port_TagControllersName(int player,unsigned address,int capacity);
int Port_TagControllersActive(void);
#endif
