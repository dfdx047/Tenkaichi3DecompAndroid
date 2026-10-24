#ifdef PORT
#include "common.h"
#include "menu/menu_a.h"
#include "menu/menu_e.h"
#include "menu/port_screen.h"
typedef struct LoadSprite {s32 flags,x0,y0,x1,y1,u0,v0,u1,v1,tex,r,g,b,a;} LoadSprite;
#include "sys/pad.h"
#include "../../port/src/gs/tag_net.h"

/* Native online pages. Resources, input, sounds and transitions use the game
   loop; the network owns only the shared room configuration. No battle is
   loaded to draw a lobby; menu cursors never become shared battle rules. */
extern void Sprite_DrawList(void *,s32,s32,LoadSprite *);
extern void PortTbo_TextMode(s32);
extern s32 PortTbo_TextRead(void);
extern void TagNet_MenuName(s32,s32,u32,s32),TagNet_MenuPointer(u32);
extern s32 Port_TagOnlinePreview(u32);
extern void Port_TagOptionFormat(s32,s32,u32,s32);
extern void IconWin_PortHideWords(void),Port_TagHudClear(void);
extern void Port_TagMenuTrace(s32,s32,s32,s32,s32,s32);
extern s32 gPortMenuMode;

enum { HOME, ADDRESS, ROOM, RESERVED_FIGHTER_PAGE, STAGE, MUSIC, RULES, GROUPS, OPTIONS, LEAVE, PRESETS };
typedef struct OnlineButton {s32 id,enabled;f32 x,y,w,h;} OnlineButton;
static OnlineButton buttons[80];
static s32 buttonCount,focus,page,frames,previousPhase,choice,group,option,editValue;
static s32 editing,returnPage,connectRole,quit,ownArc,oldMenuMode,pointerX,pointerY,draftRevision;
static char address[64]="127.0.0.1:7001",hostPort[8]="7001",notice[192],lastMessage[192];
static s32 noticeFrames;
static s32 pendingPick=-1;
void TagOnlineMenu_Open(void);
void TagOnlineMenu_Close(void);
static char *entry(void) {return connectRole==0?hostPort:address;}
static void *duelArt,*selectArt,*chipPack,*stagePack,*faceData,*stageData;
static MTexRes *background,*facePic,*stagePic;
static s32 faceId=-1,stageId=-1;
static TagNetStatus room;
static TlTournament bracket;
static char label[160],value[128];

static void name(s32 kind,s32 id,char *out,s32 size) {TagNet_MenuName(kind,id,(u32)out,size);}
static void text(f32 x,f32 y,char *s,f32 scale,f32 width,s32 style) {Ps_Text(x,y,s,scale,width,style);}
static void left(f32 x,f32 y,char *s,f32 scale,f32 width,s32 style) {Ps_Align(0);text(x,y,s,scale,width,style);Ps_Align(1);}
static void say(char *s) {Ps_Copy(notice,s,sizeof(notice));noticeFrames=300;}
static void go(s32 next,s32 first) {page=next;focus=first;editing=0;notice[0]=0;PortTbo_TextMode(next==ADDRESS);Snd_PlaySe(1,0);}
static void plate(f32 x,f32 y,f32 w,f32 h,s32 lit,s32 red) {
    u32 edge=Ps_Rgba(70,24,84,128),top=Ps_Rgba(red?243:197,red?199:229,red?217:250,125),bottom=Ps_Rgba(red?176:105,red?111:160,red?151:203,125);
    if(lit){edge=Ps_Rgba(255,243,161,128);top=Ps_Rgba(255,246,182,128);bottom=Ps_Rgba(244,170,54,128);}
    Ps_Quad(x+6,y,x+w,y,x,y+h,x+w-6,y+h,edge,edge,edge,edge);
    Ps_Quad(x+7,y+2,x+w-3,y+2,x+3,y+h-2,x+w-7,y+h-2,top,top,bottom,bottom);
}
static void button(s32 id,f32 x,f32 y,f32 w,f32 h,char *s,s32 enabled) {
    if(buttonCount<80){OnlineButton *b=&buttons[buttonCount++];b->id=id;b->enabled=enabled;b->x=x;b->y=y;b->w=w;b->h=h;}
    plate(x,y,w,h,focus==id && enabled,0);
    if(s && *s)text(x+w/2,Ps_TextTop(y+h/2,0.76f),s,0.76f,w-16,enabled?(focus==id?1:0):4);
}
static void sprite(MTexRes *pic,s32 index,f32 x,f32 y,f32 w,f32 h,s32 u0,s32 v0,s32 u1,s32 v1,s32 alpha) {
    LoadSprite list[2];if(!pic)return;memset(list,0,sizeof(list));
    list[0].flags=1;list[0].tex=index;list[0].x0=(s32)Ps_X(x);list[0].y0=(s32)Ps_Y(y);list[0].x1=(s32)Ps_X(x+w);list[0].y1=(s32)Ps_Y(y+h);
    list[0].u0=u0;list[0].v0=v0;list[0].u1=u1;list[0].v1=v1;
    list[0].r=list[0].g=list[0].b=128;list[0].a=alpha;list[1].flags=2;
    Sprite_DrawList(pic,0,0,list);
}
static void texture(MTexRes *pic,f32 x,f32 y,f32 w,f32 h) {
    u64 tex;if(!pic)return;tex=*(u64 *)(pic->tex+0x30);
    sprite(pic,0,x,y,w,h,0,0,1<<((tex>>26)&15),1<<((tex>>30)&15),128);
}
static void portrait(s32 id,f32 x,f32 y,f32 w,f32 h) {
    MTexRes *pic;if(id<0||id>=PortGame_CharacterCount())return;
    pic=(MTexRes *)MPACK_AT(chipPack,id+1);texture(pic,x,y,w,h);
}
static void stage_preview(f32 x,f32 y,f32 w,f32 h) {
    s32 row,col;/* The original stage artwork is eight 128x256 tiles. */
    if(!stagePic)return;
    for(row=0;row<2;row++)for(col=0;col<4;col++)
        sprite(stagePic,col+row*4,x+w*col/4,y+h*row/2,w/4,h/2,0,0,128,256,128);
}
static void load_stage(s32 id) {
    void *file;if(id==stageId)return;
    sceGsSyncPath(0,0);if(stageData)Heap_Free(stageData);stageData=NULL;stagePic=NULL;stageId=id;
    file=File_LoadSync(TS_STAGE_FILE+id,NULL,0);if(!file)return;
    stageData=Sprite_Unpack(file,NULL,NULL);Heap_Free(file);
    stagePic=(MTexRes *)stageData;Res_RelocateOffsets(&stagePic,stagePic,stagePic);
}
static void load_face(s32 id) {
    void *file;if(id==faceId)return;
    sceGsSyncPath(0,0);if(faceData)Heap_Free(faceData);faceData=NULL;facePic=NULL;faceId=id;
    file=File_LoadSync(TS_FACE_FILE+id,NULL,0);if(!file)return;
    faceData=Sprite_Unpack(file,NULL,NULL);Heap_Free(file);
    facePic=(MTexRes *)faceData;Res_RelocateOffsets(&facePic,facePic,facePic);
}
static void heading(char *title,char *sub) {
    char lines[192],*s=noticeFrames>0&&notice[0]?notice:sub;
    s32 style=noticeFrames>0?1:3,n,split=0,i;
    f32 width,scale=0.57f;
    Ps_Heading(256,12,title,1.55f,460,128);
    if(Ps_TextWidth(s,0.65f)<=468){text(256,53,s,0.65f,468,style);return;}
    /* Keep long notices above the team headings instead of clipping at the
       page edge or overlapping the roster. Split at the nearest middle word. */
    Ps_Copy(lines,s,sizeof(lines));n=Ps_Len(lines);
    for(i=1;i<n-1;i++)if(lines[i]==' ' && (!split || __builtin_abs(i-n/2)<__builtin_abs(split-n/2)))split=i;
    if(!split){width=Ps_TextWidth(lines,scale);text(256,53,lines,scale*468/width,468,style);return;}
    lines[split]=0;width=Ps_TextWidth(lines,scale);
    if(Ps_TextWidth(lines+split+1,scale)>width)width=Ps_TextWidth(lines+split+1,scale);
    if(width>468)scale*=468/width;
    text(256,49,lines,scale,468,style);text(256,61,lines+split+1,scale,468,style);
}
static s32 can_edit(s32 i) {return room.host || (room.role<TN_PLAYERS && room.role>=0 && room.config.owner[i]==room.role);}
static void commit(TagNetConfig *c) {TagNet_Edit(c);Snd_PlaySe(1,1);}
static s32 claimed(void) {s32 i;for(i=0;i<room.config.size[0]+room.config.size[1];i++)if(room.config.owner[i]==room.role)return 1;return 0;}
static void ready(void) {
    if(room.config.party && bracket.active){if(bracket.active==1 && (room.host||claimed()))TagNet_Ready(!room.ready[room.role]);return;}
    if(room.config.party && !claimed() && !room.host)TagNet_SwitchRole(1,-1);
    else if(room.role>=0&&room.role<TN_PLAYERS)TagNet_Ready(!room.ready[room.role]);
}
static void select_fighter(s32 i) {
    if(room.config.party && room.config.mode==TL_GAUNTLET && i>=room.config.size[0])return;
    if(!can_edit(i)) {
        if(room.config.owner[i]<0){TagNet_SwitchRole(1,i);say("Taking this slot. Select it again to choose your fighter.");}
        else say("This fighter belongs to another player.");
        return;
    }
    if(room.role>=0 && room.role<TN_PLAYERS)TagNet_Ready(0);
    pendingPick=i;draftRevision=room.config.revision;
}
static s32 group_option(s32 index) {
    s32 i;for(i=0;i<TN_OPTIONS;i++)if(tagOptionInfo[i].group==group && index--==0)return i;return -1;
}
static void select_room(void) {go(ROOM,0);}
static void remove_slot(s32 id) {
    TagNetConfig c=room.config;s32 team=id/5,i,k;
    if(!room.host || id<0 || id>=10)return;
    if(c.party && c.mode==TL_GAUNTLET && team==1)return;
    if(id%5>=c.size[team])return;
    i=(team?c.size[0]:0)+id%5;
    if(c.owner[i]>=0){say("Only CPU slots can be removed. Players can choose Spectate.");return;}
    if(c.size[team]<=1){say("Keep at least one fighter on each team.");return;}
    for(k=i;k+1<c.size[0]+c.size[1];k++) {
        TagNet_CopyFighter(&c,k,k+1);
    }
    c.size[team]--;commit(&c);focus=team*5+(id%5<c.size[team]?id%5:c.size[team]-1);
}
static void connect(void) {
    s32 ip[4]={0},port=7001,n=0,i,j=0,digits=0;char host[64];
    if(connectRole==0) {
        port=0;for(i=0;hostPort[i]>='0'&&hostPort[i]<='9';i++)port=port*10+hostPort[i]-'0';
        if(!i||hostPort[i]||port<1024||port>65535){say("Use a port between 1024 and 65535.");return;}
        TagNet_Start(0,"",port);go(HOME,0);return;
    }
    /* Deliberately numeric IPv4, like the transport. Never accept a partial address. */
    for(i=0;address[i];i++) {
        s32 ch=address[i];
        if(ch>='0'&&ch<='9'){if(n<4)ip[n]=ip[n]*10+ch-'0';digits++;if(digits>5)break;}
        else if(ch=='.' && digits && n<3){n++;digits=0;}
        else if(ch==':' && n==3 && digits){j=i;break;}
        else break;
    }
    if(n!=3 || !digits || (address[i] && address[i]!=':')){say("Enter the host's IPv4 address, such as 192.168.1.5:7001");return;}
    for(n=0;n<4;n++)if(ip[n]>255){say("Each part of the address must be between 0 and 255.");return;}
    if(j){port=0;for(i=j+1;address[i]>='0'&&address[i]<='9';i++){port=port*10+address[i]-'0';if(port>65535)break;}
        if(address[i] || i==j+1 || port<1024 || port>65535){say("Use a port between 1024 and 65535.");return;}}
    sprintf(host,"%d.%d.%d.%d",ip[0],ip[1],ip[2],ip[3]);TagNet_Start(connectRole,host,port);go(HOME,connectRole);
}
static void activate(s32 id) {
    TagNetConfig c=room.config;s32 n,i,k,team;
    if(page==HOME) {
        if(id==0){connectRole=0;go(ADDRESS,14);}
        if(id==1 || id==2){connectRole=id;go(ADDRESS,0);}
        if(id==3)quit=1;
    } else if(page==ADDRESS) {
        static char keys[]="123456789.0:";char *a=entry();n=Ps_Len(a);
        if(id<12 && n<(connectRole?60:5) && (connectRole || (id!=9&&id!=11))){a[n]=keys[id];a[n+1]=0;}
        if(id==12 && n)a[n-1]=0;
        if(id==13)a[0]=0;
        if(id==14)connect();
        if(id==15)go(HOME,connectRole);
    } else if(page==ROOM) {
        if(id<10) {
            team=id/5;i=(team?c.size[0]:0)+id%5;
            if(id%5<c.size[team])select_fighter(i);
            else if(room.host && id%5==c.size[team]) {
                for(k=c.size[0]+c.size[1];k>i;k--)TagNet_CopyFighter(&c,k,k-1);
                c.chara[i]=0;c.costume[i]=0;c.owner[i]=-1;c.cpuLevel[i]=-1;c.lobby.draft.team[i]=team;memset(c.items[i],0,sizeof(c.items[i]));c.size[team]++;commit(&c);
            }
        } else if(id==10){choice=c.stage;go(STAGE,100+choice);}
        else if(id==11){choice=c.music;go(MUSIC,100+choice);}
        else if(id==12)go(RULES,0);
        else if(id==13)TagNet_SwitchRole(room.config.party?!claimed():room.role==TN_OBSERVER,-1);
        else if(id==14)ready();
        else if(id==15){returnPage=ROOM;go(LEAVE,1);}
        else if(id==16){choice=c.preset;go(PRESETS,100+choice);}
        else if(id==17)ready();
        else if(id==18)TagNet_SkipTournament();
        else if(id==19)TagNet_ReturnPractice();
    } else if(page==PRESETS) {
        extern s32 TagChallenge_Count(void),TagChallenge_Capacity(void),TagChallenge_FirstCount(void),TagChallenge_FirstFighter(s32,s32);
        extern void TagChallenge_Choose(s32);
        n=c.mode==TL_GAUNTLET?TagChallenge_Count():5;
        if(id>=100){choice=id-100;return;}
        if(id==0 || id==1){choice=Ps_Wrap(choice+(id==0?-6:6),0,n-1);focus=100+choice;}
        if(id==4 && room.host) {
            c.preset=choice;
            if(c.mode==TL_GAUNTLET){TagChallenge_Choose(choice);c.size[1]=TagChallenge_Capacity();
                for(i=0;i<c.size[1];i++){k=c.size[0]+i;c.chara[k]=TagChallenge_FirstFighter(i%TagChallenge_FirstCount(),0);c.costume[k]=TagChallenge_FirstFighter(i%TagChallenge_FirstCount(),1);c.owner[k]=-1;memset(c.items[k],0,sizeof(c.items[k]));}}
            commit(&c);select_room();
        }
        if(id==6)select_room();
    } else if(page==STAGE) {
        if(id>=100){choice=id-100;return;}
        if(id==0 || id==1){choice=Ps_Wrap(choice+(id==0?-9:9),0,PortGame_StageCount()-1);if(choice==35)choice=34;focus=100+choice;}
        if(id==4 && room.host){c.stage=choice;commit(&c);select_room();}
        if(id==6)select_room();
    } else if(page==MUSIC) {
        if(id>=100){choice=id-100;Bgm_Play(PortGame_MusicFile(choice==24?0:choice));return;}
        if(id==0 || id==1){choice=Ps_Wrap(choice+(id==0?-8:8),0,24);focus=100+choice;}
        if(id==4 && room.host){c.music=choice;commit(&c);select_room();}
        if(id==6)select_room();
    } else if(page==RULES) {
        if(id==0 && c.party && c.mode==TL_TOURNAMENT){choice=c.preset;go(PRESETS,100+choice);return;}
        if(id<2 && room.host){editing=1;option=id;editValue=id?c.cpu:c.party&&c.mode==TL_GAUNTLET?c.heal:c.time;}
        if(id==2)go(GROUPS,0);
        if(id==3)select_room();
    } else if(page==GROUPS) {
        if(id==99)go(RULES,2);
        else if(id==98){group=Ps_Wrap(group+8,0,TAG_OPTION_GROUP_COUNT-1);focus=group;}
        else {group=id;choice=0;option=0;go(OPTIONS,100+group_option(0));}
    } else if(page==OPTIONS) {
        if(id>=100){option=id-100;if(room.host){editing=1;editValue=c.options[option];}}
        if(id==0 || id==1){n=0;while(group_option(n)>=0)n++;choice=Ps_Wrap(choice+(id==0?-6:6),0,n-1);focus=100+group_option(choice);}
        if(id==6)go(GROUPS,group);
    } else if(page==LEAVE) {
        if(id==0){TagNet_Leave();go(HOME,0);}
        else go(returnPage,15);
    }
}
static void back(void) {
    if(editing){editing=0;Snd_PlaySe(1,2);return;}
    if(page==ROOM && room.config.party && !room.config.practice) {
        if(room.host)TagNet_ReturnPractice();else TagNet_SwitchRole(0,-1);
        return;
    }
    if(page==ROOM){returnPage=ROOM;go(LEAVE,1);}
    else if(page==HOME)quit=1;
    else if(page==ADDRESS)go(HOME,connectRole);
    else if(page==LEAVE)go(returnPage,15);
    else if(page==OPTIONS)go(GROUPS,group);
    else if(page==GROUPS)go(RULES,2);
    else select_room();
}
static void input(void) {
    u32 p=gPad[0].gamePressed,nav=gPad[0].gameRepeat,raw=gPad[0].repeat;
    s32 ptr[3],i,hit=-1,old=focus;
    if(raw&PAD_LSTICK_LEFT)nav|=PADG_LEFT;if(raw&PAD_LSTICK_RIGHT)nav|=PADG_RIGHT;
    if(raw&PAD_LSTICK_UP)nav|=PADG_UP;if(raw&PAD_LSTICK_DOWN)nav|=PADG_DOWN;
    TagNet_MenuPointer((u32)ptr);
    Ps_PageSet();Ps_SetShift(0);ptr[0]=(s32)((ptr[0]-Ps_X(0))*512/(Ps_X(512)-Ps_X(0)));
    ptr[1]=(s32)((ptr[1]-Ps_Y(0))*448/(Ps_Y(448)-Ps_Y(0)));
    if(page==ADDRESS) {
        s32 ch;while((ch=PortTbo_TextRead())!=0) {
            char *a=entry();s32 len=Ps_Len(a);
            if(((ch>='0'&&ch<='9')||(connectRole&&(ch=='.'||ch==':')))&&len<(connectRole?60:5)){a[len]=ch;a[len+1]=0;}
            else if(ch==8&&len)a[len-1]=0;
            else if(ch==13)connect();else if(ch==27)go(HOME,connectRole);
        }
    }
    if(p&PADG_TRIANGLE){back();return;}
    if(editing) {
        if(nav&(PADG_LEFT|PADG_RIGHT)) {
            s32 d=nav&PADG_LEFT?-1:1;
            if(page==RULES)editValue=Ps_Wrap(editValue+d,0,option || (room.config.party&&room.config.mode==TL_GAUNTLET)?4:5);
            else editValue=Ps_Wrap(editValue+d*tagOptionInfo[option].step,tagOptionInfo[option].min,tagOptionInfo[option].max);
            Snd_PlaySe(1,0);
        }
        if(p&PADG_CROSS) {
            TagNetConfig c=room.config;if(page==RULES){if(option)c.cpu=editValue;else if(c.party&&c.mode==TL_GAUNTLET)c.heal=editValue;else c.time=editValue;}else c.options[option]=editValue;
            commit(&c);editing=0;
        }
        return;
    }
    for(i=0;i<buttonCount;i++)if(buttons[i].enabled && ptr[0]>=buttons[i].x&&ptr[0]<buttons[i].x+buttons[i].w&&ptr[1]>=buttons[i].y&&ptr[1]<buttons[i].y+buttons[i].h)hit=i;
    if(hit>=0 && (ptr[0]!=pointerX||ptr[1]!=pointerY||ptr[2]))focus=buttons[hit].id;
    pointerX=ptr[0];pointerY=ptr[1];
    if(nav&(PADG_UP|PADG_DOWN|PADG_LEFT|PADG_RIGHT)) {
        OnlineButton *cur=NULL;f32 best=100000;
        for(i=0;i<buttonCount;i++)if(buttons[i].id==focus)cur=&buttons[i];
        if(cur)for(i=0;i<buttonCount;i++)if(buttons[i].enabled && buttons[i].id!=focus) {
            f32 dx=buttons[i].x+buttons[i].w/2-cur->x-cur->w/2,dy=buttons[i].y+buttons[i].h/2-cur->y-cur->h/2;
            f32 along,across,score;
            if(nav&(PADG_LEFT|PADG_RIGHT)){along=dx*(nav&PADG_LEFT?-1:1);across=__builtin_fabsf(dy)-(buttons[i].h+cur->h)/2;}
            else {along=dy*(nav&PADG_UP?-1:1);across=__builtin_fabsf(dx)-(buttons[i].w+cur->w)/2;}
            /* Prefer controls in the same row/column before a diagonal jump. */
            score=along+0.75f*__builtin_fabsf(nav&(PADG_LEFT|PADG_RIGHT)?dy:dx);
            if(across>0)score+=10000+across*3;
            if(along>2&&score<best){focus=buttons[i].id;best=score;}
        }
    }
    if(old!=focus) {
        Snd_PlaySe(1,0);
        if(focus>=100&&(page==STAGE||page==MUSIC||page==PRESETS))choice=focus-100;
    }
    if((p&PADG_START) && page==ROOM)ready();
    if((p&PADG_SQUARE) && page==ROOM && !bracket.active)remove_slot(focus);
    if(page==ROOM && room.config.party && room.config.mode==TL_TOURNAMENT && !bracket.active && focus>=0 && focus<10 && (p&(PADG_L1|PADG_R1))) {
        TagNetConfig c=room.config;s32 at=(focus/5?c.size[0]:0)+focus%5,j,n=0,t;
        if(focus%5<c.size[focus/5] && can_edit(at)) {
            t=Ps_Wrap(c.lobby.draft.team[at]+(p&PADG_L1?-1:1),0,3);
            for(j=0;j<c.size[0]+c.size[1];j++)if(j!=at && c.lobby.draft.team[j]==t)n++;
            if(n<5){c.lobby.draft.team[at]=t;commit(&c);}else say("That team already has five fighters.");
        }
    }
    if(page==ROOM && room.config.party && (room.config.mode==TL_TEAM || room.config.mode==TL_TRAINING) && (p&(PADG_L1|PADG_R1))) {
        s32 at,j,target=-1;for(at=0;at<room.config.size[0]+room.config.size[1];at++)if(room.config.owner[at]==room.role)break;
        if(at<room.config.size[0]+room.config.size[1]) {
            s32 start=at<room.config.size[0]?room.config.size[0]:0,end=at<room.config.size[0]?room.config.size[0]+room.config.size[1]:room.config.size[0];
            for(j=start;j<end;j++)if(room.config.owner[j]<0){target=j;break;}
            if(target>=0)TagNet_SwitchRole(1,target);else say("There is no open fighter slot on that team.");
        }
    }
    if((p&PADG_L1) && (page==STAGE||page==MUSIC||page==OPTIONS||page==PRESETS))activate(0);
    if((p&PADG_R1) && (page==STAGE||page==MUSIC||page==OPTIONS||page==PRESETS))activate(1);
    if(p&PADG_CROSS || (ptr[2]&&hit>=0)) {
        for(i=0;i<buttonCount;i++)if(buttons[i].id==focus&&buttons[i].enabled){activate(focus);break;}
    }
}
static void home(void) {
    heading(TagNet_IsParty()?"ONLINE LOBBY":"MODDED ONLINE","");
    load_face(PortGame_IsBT4()?3:0);texture(facePic,20,88,154,268);
    button(0,190,102,294,63,"Host a Room",1);left(209,144,"Choose the teams, stage and battle rules.",0.58f,258,2);
    button(1,190,181,294,63,"Join a Room",1);left(209,223,"Enter the host address and choose your fighter.",0.58f,258,2);
    if(!TagNet_IsParty()){button(2,190,260,294,63,"Watch a Room",1);left(209,302,"Follow any fighter or use the free camera.",0.58f,258,2);}
    else {left(209,277,"Practise together between matches.",0.67f,258,3);left(209,304,"Take a player slot at team setup, or watch.",0.58f,258,2);}
    button(3,190,343,140,30,"Back",1);
    text(256,386,"Players and spectators can switch places in the room.",0.65f,468,3);
}
static void connection(void) {
    s32 i;static char *keys[]={"1","2","3","4","5","6","7","8","9",".","0",":"};
    heading(connectRole==0?"HOST A ROOM":connectRole==2?"WATCH A ROOM":"JOIN A ROOM",connectRole==0?"CONNECTION PORT":"HOST ADDRESS");
    plate(36,87,440,45,1,0);text(256,96,entry()[0]?entry():"Type here or use the keypad",1.0f,415,1);
    for(i=0;i<12;i++)button(i,144+76*(i%3),151+39*(i/3),70,33,keys[i],connectRole || (i!=9&&i!=11));
    button(12,48,318,113,31,"Delete",1);button(13,171,318,113,31,"Clear",1);
    button(14,294,318,170,31,connectRole==0?"Open Room":connectRole==2?"Spectate":"Connect",1);button(15,48,360,113,30,"Back",1);
    text(319,367,connectRole?"Example  192.168.1.5:7001":"Default port  7001",0.67f,282,3);
}
static void lobby(void) {
    TagNetConfig *c=&room.config;s32 team,j,i,p,readyCount=0;f32 x,y;
    /* Character arrays avoid mixed-width pointer loads in the native game's
       32-bit data model when selecting a title in a conditional expression. */
    static char titles[][24]={"MODDED DUEL","FREE FOR ALL","MODDED TRAINING","MODDED GAUNTLET","WORLD TOURNAMENT"};
    if(c->party && c->mode==TL_TOURNAMENT && bracket.active) {
        heading("TOURNAMENT BRACKET",bracket.active==2?"CHAMPIONS CROWNED":"EVERYONE STAYS CONNECTED THROUGH EACH ROUND");
        for(i=0;i<7;i++) {
            s32 l=bracket.left[i],r=bracket.right[i],w=bracket.winner[i];f32 y=90+i*35;
            plate(28,y,456,30,bracket.match==i,0);
            if(l<0 || r<0)sprintf(label,Port_TagText("%s - Waiting for earlier winners"),Port_TagText(i==6?"Final":"Semi-final"));
            else if(w>=0)sprintf(label,Port_TagText("Team %d vs Team %d    -    Team %d wins"),l+1,r+1,w+1);
            else sprintf(label,Port_TagText("Team %d vs Team %d"),l+1,r+1);
            text(256,y+4,label,.78f,430,bracket.match==i?1:0);
        }
        if(bracket.active==2) {sprintf(label,Port_TagText("TEAM %d WINS THE TOURNAMENT"),bracket.winner[6]+1);text(256,345,label,.9f,462,1);}
        else {
            button(17,28,348,217,31,room.ready[room.role]?"Not Ready":claimed()?"Ready to Fight":room.host?"Watch Next Match":"Watching",room.host||claimed());
            button(18,263,348,221,31,"Skip CPU Match",room.host&&TlTournament_CpuMatch(&bracket));
        }
        button(19,151,389,210,29,"Return to Lobby",room.host);return;
    }
    if(c->party && c->practice) {
        heading("ONLINE LOBBY","WAITING FOR GUESTS TO ENTER PRACTICE");
        text(256,175,PortGame_IsBT4()?"South City - 200%":"Kami's Lookout - 200%",1.1f,460,1);
        text(256,220,"Guests enter the practice map when connected.",.78f,460,3);
        text(256,253,"The host stays in the mode menu.",.78f,460,3);
        button(15,177,345,158,36,"Leave Lobby",1);return;
    }
    heading(c->party?titles[c->mode]:"ONLINE BATTLE ROOM",c->party?(c->mode==TL_TOURNAMENT?"START - CLAIM / READY    L1 / R1 - CHANGE TEAM":"START - CLAIM A SLOT / READY   TRIANGLE - RELEASE SLOT"):room.host?"YOU ARE HOSTING  /  CHOOSE THE LINEUP AND READY UP":"CHOOSE YOUR FIGHTER AND READY UP");
    for(team=0;team<2;team++) {
        x=18+244*team;Ps_Heading(x+112,76,c->party&&c->mode==TL_GAUNTLET?(team?"NEXT WAVE":"YOUR TEAM"):c->party&&c->mode==TL_FFA?(team?"FIGHTERS 6 - 10":"FIGHTERS 1 - 5"):team?"TEAM 2":"TEAM 1",1.05f,208,128);
        for(j=0;j<5;j++) {
            y=102+j*39;i=(team?c->size[0]:0)+j;
            if(j<c->size[team]) {
                button(team*5+j,x,y,232,36,"",!(c->party&&c->mode==TL_GAUNTLET&&team));portrait(c->chara[i],x+7,y+3,30,30);
                name(0,c->chara[i],label,sizeof(label));left(x+43,y+1,label,0.65f,181,focus==team*5+j?1:0);
                p=c->owner[i];if(p<0)Ps_Copy(label,"CPU",sizeof(label));else sprintf(label,Port_TagText("P%d%s  -  %s"),p+1,Port_TagText(p==room.role?" (YOU)":""),Port_TagText(room.ready[p]?"READY":"CHOOSING"));
                if(c->party&&c->mode==TL_TOURNAMENT){char who[60];Ps_Copy(who,label,sizeof(who));sprintf(label,Port_TagText("TEAM %d  -  %s"),c->lobby.draft.team[i]+1,who);}
                left(x+43,y+18,label,0.55f,178,p>=0&&room.ready[p]?5:2);
            } else if(j==c->size[team] && !(c->party&&c->mode==TL_GAUNTLET&&team))button(team*5+j,x,y,232,36,room.host?"+ Add CPU":"Open fighter slot",room.host);
        }
    }
    load_stage(c->stage);button(10,18,308,145,70,"",1);stage_preview(22,312,137,50);
    name(1,c->stage,label,sizeof(label));text(90,362,label,0.52f,134,2);
    button(11,174,308,156,29,"Music",1);name(2,c->music,label,sizeof(label));text(252,338,label,0.5f,154,3);
    if(c->party && (c->mode==TL_GAUNTLET || c->mode==TL_TOURNAMENT))button(16,174,353,156,27,c->mode==TL_GAUNTLET?"Choose Gauntlet":"Tournament Type",1);
    button(12,340,308,154,29,"Battle Rules",1);
    button(13,340,347,154,29,c->party?(claimed()?"Release Slot":"Join as Player"):room.role==TN_OBSERVER?"Join as Player":"Spectate",1);
    for(p=0;p<10;p++)if(room.playerMask&(1<<p))readyCount+=room.ready[p]!=0;
    sprintf(label,Port_TagText("%d/%d ready   |   %d/%d watching"),readyCount,room.connected,room.spectators,TN_SPECTATORS);
    left(20,388,label,0.6f,265,3);
    button(14,287,385,113,29,room.role==TN_OBSERVER?"Watching":room.ready[room.role]?"Not Ready":"Ready",room.role<TN_PLAYERS && room.role>=0);
    button(15,411,385,83,29,"Leave",1);
}
static void stages(void) {
    s32 i,start=(choice/9)*9,last=start+9;if(last>PortGame_StageCount())last=PortGame_StageCount();
    heading("CHOOSE STAGE",room.host?"HOST'S BATTLEFIELD SELECTION":"THE HOST CHOOSES THE BATTLEFIELD");load_stage(choice);
    plate(18,90,249,221,0,0);stage_preview(23,95,239,209);
    for(i=start;i<last;i++)if(i!=35) {
        f32 x=279+73*((i-start)%3),y=91+70*((i-start)/3);MTexRes *pic=(MTexRes *)MPACK_AT(stagePack,i+1);
        button(100+i,x,y,67,63,"",1);texture(pic,x+3,y+4,61,54);
    }
    name(1,choice,label,sizeof(label));text(141,320,label,0.91f,246,1);
    button(0,279,309,57,28,"<",1);sprintf(label,"%d / %d",choice/9+1,(PortGame_StageCount()+8)/9);text(383,314,label,0.7f,70,3);button(1,432,309,60,28,">",1);
    button(4,143,365,170,34,"Confirm Stage",room.host);button(6,326,365,100,34,"Back",1);
}
static void music(void) {
    s32 i,start=(choice/8)*8,last=start+8;if(last>25)last=25;
    heading("BATTLE MUSIC","SELECT A TRACK TO LISTEN BEFORE THE MATCH");
    load_stage(room.config.stage);plate(20,102,207,210,0,0);stage_preview(25,107,197,172);
    text(123,286,choice==24?"RANDOM MUSIC":"NOW PLAYING",0.77f,190,1);
    for(i=start;i<last;i++){name(2,i,label,sizeof(label));button(100+i,242,91+(i-start)*32,250,28,label,1);}
    button(0,242,357,45,28,"<",1);button(1,447,357,45,28,">",1);
    button(4,20,357,142,32,"Confirm Music",room.host);button(6,20,399,142,28,"Back",1);
}
static char *time_name(s32 v) {static char a[][16]={"No Limit","60 seconds","90 seconds","180 seconds","240 seconds","45 seconds"};return a[v];}
static void presets(void) {
    extern s32 TagChallenge_Count(void);extern void TagChallenge_Choose(s32),TagTournament_Choose(s32);
    extern const char *TagChallenge_Title(void),*TagTournament_Title(void);
    s32 g=room.config.mode==TL_GAUNTLET,n=g?TagChallenge_Count():5,i,start=(choice/6)*6;
    heading(g?"CHOOSE GAUNTLET":"CHOOSE TOURNAMENT",room.host?"SELECT AN EVENT AND CONFIRM":"EVENT CHOSEN BY THE HOST");
    for(i=start;i<start+6&&i<n;i++){
        if(g)TagChallenge_Choose(i);else TagTournament_Choose(i);
        button(100+i,35,92+(i-start)*43,442,37,(char *)(g?TagChallenge_Title():TagTournament_Title()),1);
    }
    if(g)TagChallenge_Choose(room.config.preset);else TagTournament_Choose(room.config.preset);
    button(0,35,361,50,29,"<",n>6);button(1,427,361,50,29,">",n>6);
    button(4,113,361,180,29,"Confirm Event",room.host);button(6,310,361,100,29,"Back",1);
}
static const char *cpu_name(s32 v) {extern const char *CpuLevel_SettingName(s32);return CpuLevel_SettingName(v);}
static void rules(void) {
    s32 gauntlet=room.config.party&&room.config.mode==TL_GAUNTLET;
    s32 tour=room.config.party&&room.config.mode==TL_TOURNAMENT;
    s32 t=editing&&option==0?editValue:gauntlet?room.config.heal:room.config.time,c=editing&&option==1?editValue:room.config.cpu,i;
    heading("BATTLE RULES",room.host?"SELECT A RULE, THEN CHANGE IT AND CONFIRM":"RULES CHOSEN BY THE HOST");
    button(0,30,103,218,150,"",room.host);button(1,264,103,218,150,"",room.host);
    Ps_Disc(139,151,30,Ps_Rgba(248,240,180,128),Ps_Rgba(123,82,149,128));
    Ps_Rect(137,130,140,153,Ps_Rgba(68,28,90,128),Ps_Rgba(68,28,90,128));Ps_Rect(139,150,156,153,Ps_Rgba(68,28,90,128),Ps_Rgba(68,28,90,128));
    for(i=0;i<5;i++)Ps_Rect(328+i*19,172-i*9,342+i*19,181,i<=c?Ps_Rgba(255,191,53,128):Ps_Rgba(63,47,81,128),i<=c?Ps_Rgba(229,91,50,128):Ps_Rgba(63,47,81,128));
    text(139,192,tour?"TOURNAMENT":gauntlet?"HEAL PER WAVE":"DUEL TIME",0.94f,195,2);text(373,192,"COM LEVEL",0.94f,195,2);
    if(tour){extern void TagTournament_Choose(s32);extern const char *TagTournament_Title(void);TagTournament_Choose(room.config.preset);text(139,223,(char *)TagTournament_Title(),.7f,195,3);}
    else {static char heal[][12]={"None","1/4 bar","1/2 bar","1 bar","Full"};text(139,223,gauntlet?heal[t]:time_name(t),0.83f,195,editing&&option==0?1:3);}
    text(373,223,cpu_name(c),0.83f,195,editing&&option==1?1:3);
    button(2,30,279,452,43,"Gameplay, Cameras and HUD",1);button(3,30,351,126,33,"Back",1);
    text(325,361,editing?"Left / Right changes. Cross saves. Triangle cancels.":"All players see the same battle rules.",0.65f,310,3);
}
static void groups(void) {
    s32 i,start=(group/8)*8,last=start+8;if(last>TAG_OPTION_GROUP_COUNT)last=TAG_OPTION_GROUP_COUNT;
    heading("MATCH SETTINGS","GAMEPLAY  /  CAMERAS  /  HUD");
    for(i=start;i<last;i++)button(i,26+243*((i-start)%2),98+64*((i-start)/2),237,52,(char *)tagOptionGroups[i],1);
    button(98,269,365,237,31,"More Settings",TAG_OPTION_GROUP_COUNT>8);button(99,26,365,120,31,"Back",1);
}
/* Small native illustrations keep the settings legible without loading a battle. */
static void illustration(s32 id) {
    s32 g=tagOptionInfo[id].group,v=editing?editValue:room.config.options[id],i;
    f32 x=330,y=149,w=148;u32 gold=Ps_Rgba(255,213,89,128),blue=Ps_Rgba(90,213,255,128),dark=Ps_Rgba(34,29,56,128);
    struct {f32 rect[4];s32 id,values[TAG_OPT_COUNT];} preview;
    plate(324,99,169,241,0,0);text(408,110,"PREVIEW",0.7f,149,2);
    preview.rect[0]=Ps_X(330);preview.rect[1]=Ps_Y(144);preview.rect[2]=Ps_X(487);preview.rect[3]=Ps_Y(284);preview.id=id;
    for(i=0;i<TAG_OPT_COUNT;i++)preview.values[i]=room.config.options[i];preview.values[id]=v;
    if(Port_TagOnlinePreview((u32)&preview)) {
        Port_TagOptionFormat(id,v,(u32)value,sizeof(value));text(408,307,value,0.67f,146,1);return;
    }
    if(g==10) {
        f32 z=id==TAG_OPT_MAP_SIZE?(f32)v/300:room.config.options[TAG_OPT_MAP_SIZE]/300.0f;
        Ps_Rect(x+5,y+10,x+w-5,y+139,dark,dark);
        Ps_Rect(x+74-61*z,y+73-61*z,x+74+61*z,y+73+61*z,blue,Ps_Rgba(24,86,142,128));portrait(0,x+61,y+62,26,26);
    } else if(g==9) {
        f32 z=(id==TAG_OPT_GIANT_SIZE?v:room.config.options[TAG_OPT_GIANT_SIZE])/400.0f;
        portrait(0,x+10,y+100,28,28);portrait(31,x+67,y+128-90*z,50+30*z,50+40*z);
    } else if(g==2 || id==TAG_OPT_THREE_WIDE) {
        s32 shared=id==TAG_OPT_THREE_WIDE?0:v;
        for(i=0;i<(shared?1:2);i++) {
            f32 ww=shared?140:66,xx=x+4+i*74;
            Ps_Rect(xx,y+13,xx+ww,y+112,dark,dark);portrait(i?31:0,xx+ww/2-20,y+49,40,40);
            Ps_Rect(xx+4,y+19,xx+ww-4,y+23,i?gold:blue,i?gold:blue);
        }
        text(408,y+125,shared?"SHARED CAMERA":"PLAYER VIEWS",0.61f,151,2);
    } else if(g==0) {
        s32 on=id==TAG_OPT_REVIVE?v:room.config.options[TAG_OPT_REVIVE];
        portrait(0,x+16,y+43,40,40);portrait(31,x+97,y+43,40,40);
        if(on){Ps_Disc(x+117,y+98,27,Ps_Rgba(49,189,155,70),Ps_Rgba(151,255,210,128));
            Ps_Rect(x+14,y+119,x+135,y+130,dark,dark);Ps_Rect(x+16,y+121,x+94,y+128,blue,blue);}
        text(408,y+140,on?"REVIVE TOGETHER":"REVIVAL DISABLED",0.57f,150,2);
    } else if(g==7) {
        portrait(0,x+4,y+15,38,38);portrait(31,x+107,y+15,38,38);
        text(408,y+25,"+",1.2f,30,1);portrait(PortGame_IsBT4()?3:58,x+52,y+63,46,46);
        Ps_Rect(x+8,y+126,x+141,y+136,dark,dark);Ps_Rect(x+10,y+128,x+107,y+134,gold,gold);
        text(408,y+142,"FUSION",0.68f,150,2);
    } else if(g==3 || g==4) {
        portrait(31,x+51,y+38,49,49);
        if(room.config.options[TAG_OPT_TARGET_MARKER] && !(id==TAG_OPT_TARGET_MARKER&&!v))
            Ps_Quad(x+65,y+20,x+87,y+20,x+76,y+33,x+76,y+33,gold,gold,gold,gold);
        if(room.config.options[TAG_OPT_ENEMY_OVERHEAD] && !(id==TAG_OPT_ENEMY_OVERHEAD&&!v)) {
            Ps_Rect(x+40,y+9,x+109,y+16,dark,dark);Ps_Rect(x+42,y+11,x+95,y+14,blue,blue);
        }
        Ps_Rect(x+6,y+119,x+141,y+130,dark,dark);Ps_Rect(x+8,y+121,x+102,y+128,blue,blue);
        text(408,y+143,"BATTLE DISPLAY",0.64f,150,2);
    } else if(g==1) {
        portrait(0,x+6,y+30,40,40);portrait(31,x+105,y+81,40,40);
        Ps_Quad(x+37,y+56,x+129,y+88,x+36,y+63,x+129,y+95,blue,gold,blue,gold);
        text(408,y+141,"TEAM TECHNIQUES",0.6f,150,2);
    } else {
        for(i=0;i<3;i++)portrait(i==1?31:0,x+5+i*49,y+40,40,40);
        for(i=0;i<5;i++)Ps_Rect(x+19+i*22,y+125-i*6,x+34+i*22,y+137,blue,gold);
        text(408,y+147,(char *)tagOptionGroups[g],0.6f,147,2);
    }
    Port_TagOptionFormat(id,v,(u32)value,sizeof(value));text(408,307,value,0.67f,146,1);
}
static void options(void) {
    s32 i,n=0,start,idx;while(group_option(n)>=0)n++;
    if(choice<0||choice>=n)choice=0;start=(choice/6)*6;
    heading((char *)tagOptionGroups[group],room.host?"SELECT AN OPTION TO EDIT":"ROOM SETTINGS - HOST CONTROLS THESE");
    for(i=start;i<start+6 && i<n;i++) {
        s32 v;idx=group_option(i);v=editing&&option==idx?editValue:room.config.options[idx];
        button(100+idx,24,94+(i-start)*43,288,39,"",room.host);
        left(36,94+(i-start)*43,(char *)tagOptionInfo[idx].label,0.66f,262,focus==100+idx?1:0);
        Port_TagOptionFormat(idx,v,(u32)value,sizeof(value));left(36,112+(i-start)*43,value,0.59f,262,3);
    }
    idx=editing?option:focus>=100?focus-100:group_option(start);
    illustration(idx);
    text(256,354,(char *)tagOptionInfo[idx].help,0.6f,470,3);
    button(0,279,389,50,29,"<",n>6);button(1,438,389,50,29,">",n>6);button(6,24,389,120,29,"Back",1);
    text(384,395,editing?"EDITING":"PAGE",0.64f,92,editing?1:3);
}
static void draw(void) {
    Ps_PageSet();Ps_SetShift(0);Ps_Align(1);Sprite_DrawPicture(background,0,0,128);
    /* The original menu's purple/cyan backdrop and cloud frame remain visible. */
    Ps_Rect(9,73,503,420,Ps_Rgba(26,17,42,100),Ps_Rgba(30,15,44,114));
    IconWin_Draw();buttonCount=0;
    if(room.phase==3){heading("GET READY","EVERYONE IS READY - LOADING THE BATTLE");load_stage(room.config.stage);stage_preview(72,100,368,275);}
    else if(page==HOME)home();else if(page==ADDRESS)connection();else if(page==ROOM)lobby();
    else if(page==STAGE)stages();else if(page==MUSIC)music();
    else if(page==RULES)rules();else if(page==GROUPS)groups();else if(page==OPTIONS)options();else if(page==PRESETS)presets();
    else {heading("LEAVE THIS ROOM?",room.host?"HOST LEAVING WILL CLOSE THE ROOM FOR EVERYONE":"YOU CAN JOIN AGAIN BEFORE THE NEXT MATCH");button(0,66,200,183,47,"Leave Room",1);button(1,263,200,183,47,"Keep Playing",1);}
    if(noticeFrames>0)noticeFrames--;
    if(room.phase==5){Ps_Rect(20,155,492,291,Ps_Rgba(36,14,40,128),Ps_Rgba(36,14,40,128));text(256,169,"CONNECTION ENDED",1.1f,450,1);text(256,212,room.message,0.65f,450,2);text(256,259,"Cross - Return to Online Menu",0.74f,450,3);}
    {s32 i,found=0;for(i=0;i<buttonCount;i++)if(buttons[i].id==focus&&buttons[i].enabled)found=1;
     if(!found)for(i=0;i<buttonCount;i++)if(buttons[i].enabled){focus=buttons[i].id;break;}}
    Ps_PromptsClear();Ps_Prompt(0,editing?"Confirm":"Select");Ps_Prompt(2,editing?"Cancel":"Back");if(page==ROOM){Ps_Prompt(7,"Ready");if(room.host)Ps_Prompt(1,"Remove CPU");}Ps_PromptsDraw();
}
void TagOnlineMenu_Open(void) {
    s32 i;void *icons;MTexRes *pic;
    page=HOME;focus=0;frames=0;previousPhase=-1;quit=0;editing=0;buttonCount=0;pointerX=pointerY=-1;notice[0]=lastMessage[0]=0;noticeFrames=0;faceId=stageId=-1;choice=group=0;
    oldMenuMode=gPortMenuMode;gPortMenuMode=1;Port_TagHudClear();
    ownArc=gMenuArc5==NULL;if(ownArc){gMenuArc5=File_LoadSync(gProgress->baseFile,NULL,0);Snd_LoadBankFile(2,0x14C);}
    duelArt=Sprite_Unpack(MPACK_AT(gMenuArc5,1),NULL,NULL);
    background=(MTexRes *)MPACK_AT(duelArt,1);Res_RelocateOffsets(&background,background,background);
    icons=MPACK_AT(duelArt,24);Res_RelocateOffsets(&icons,icons,icons);IconWin_Init(MPACK_AT(duelArt,23),icons);IconWin_PortHideWords();IconWin_Open();
    selectArt=Sprite_Unpack(MPACK_AT(gMenuArc5,2),NULL,NULL);chipPack=MPACK_AT(selectArt,45);stagePack=MPACK_AT(selectArt,46);
    for(i=0;i<PortGame_CharacterCount();i++)if(PortGame_IsCharacter(i)){pic=(MTexRes *)MPACK_AT(chipPack,i+1);Res_RelocateOffsets(&pic,pic,pic);}
    for(i=0;i<PortGame_StageCount();i++)if(i!=35){pic=(MTexRes *)MPACK_AT(stagePack,i+1);Res_RelocateOffsets(&pic,pic,pic);}
    Pad_SetRepeat(20,5);Bgm_Play(0x10B16+2);ColorFade_StartIn(0,0,0,12);
}
s32 TagOnlineMenu_Frame(void) {
    TagNet_Status(&room);
    TagNet_Tournament((u32)&bracket);
    if(room.phase==2 && previousPhase!=2)select_room();
    if(room.phase<2 && previousPhase>=2)go(HOME,0);
    previousPhase=room.phase;
    if(strcmp(lastMessage,room.message)) {Ps_Copy(lastMessage,room.message,sizeof(lastMessage));say(lastMessage);}
    Job_Run();Gfx_BeginFrame();Pad_Update();Snd_Update();ColorFade_Update();
    if(room.phase==5){if(gPad[0].gamePressed&(PADG_CROSS|PADG_TRIANGLE)){TagNet_Leave();go(HOME,0);}}
    else if(room.phase!=3 && frames>12)input();

    draw();ColorFade_Draw();File_Stub264D90();Gfx_EndFrame(1);Dma_Flush();frames++;
    Port_TagMenuTrace(81,page,focus,room.phase,room.role,room.config.revision);
    if(pendingPick>=0) {
        TagNetConfig c=room.config;TsMember member;TagNetStatus after;
        s32 id=pendingPick,k,level=c.cpuLevel[id]<0?c.cpu:c.cpuLevel[id],accepted,oldFocus=focus;
        pendingPick=-1;memset(&member,0,sizeof(member));member.chara=c.chara[id];member.color=c.costume[id];
        for(k=0;k<8;k++)member.items.id[k]=c.items[id][k];
        TagOnlineMenu_Close();
        accepted=TeamSel_PickOnline(&member,c.owner[id]<0,&level,c.revision);
        TagNet_Status(&after);
        if(accepted && after.phase==2 && after.config.revision==c.revision) {
            c.chara[id]=member.chara;c.costume[id]=member.color;
            if(c.owner[id]<0)c.cpuLevel[id]=level;
            for(k=0;k<8;k++)c.items[id][k]=member.items.id[k];
            TagNet_Edit(&c);
        }
        TagOnlineMenu_Open();TagNet_Status(&room);previousPhase=room.phase;
        if(room.phase==2){select_room();focus=oldFocus;}
    }
    return quit;
}
void TagOnlineMenu_Close(void) {
    sceGsSyncPath(0,0);PortTbo_TextMode(0);IconWin_Term();
    if(faceData)Heap_Free(faceData);if(stageData)Heap_Free(stageData);faceData=stageData=NULL;facePic=stagePic=NULL;
    Heap_Free(selectArt);Heap_Free(duelArt);selectArt=duelArt=NULL;
    if(ownArc){Heap_Free(gMenuArc5);gMenuArc5=NULL;Snd_UnloadBank(2);}gPortMenuMode=oldMenuMode;
    Adx_StopAll();Dma_ResetBuffers();
}
#endif
