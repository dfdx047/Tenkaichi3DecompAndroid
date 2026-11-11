/* A tournament's players need not be able to contact each other directly.
 * The lobby owner already has a connection to each. Only the two assigned
 * players may exchange wrapped match packets, for this match's generation.
 * The 1v1 input, rollback and spectator protocols inside are unchanged. */
enum { T_L_ROUTE=42, ROUTE_HEADER=20, ROUTE_MAX=2048 };
int Lobby_RouteSend(const void *data,int n,const struct sockaddr_in *to) {
    const uint8_t *body=data;uint8_t packet[ROUTE_HEADER+ROUTE_MAX];
    if(n<8||get(body,0)!=MAGIC||get(body,1)==T_L_ROUTE)return 0;
    if(sL.on!=2||!sL.routePeerId||!sL.inGen||sL.leaving||!same(to,&sL.routePeer))return 0;
    if(getenv("BT4_RENDER_TEST")&&getenv("BT4_LOBBY_TEST_NO_ROUTE"))return 0;
    if(!sL.routeViaOwner) {
        if(NetIo_Connected()||now_ns()-sL.goNs<2*SEC)return 0;
        sL.routeViaOwner=1;
        fprintf(stderr,"bt4: lobby: match %u using lobby owner after direct peer timeout\n",(unsigned)sL.inGen);
    }
    if(n>ROUTE_MAX)return 1;
    put(packet,0,MAGIC);put(packet,1,T_L_ROUTE);put(packet,2,sL.inGen);
    put(packet,3,sL.myId);put(packet,4,sL.routePeerId);memcpy(packet+ROUTE_HEADER,data,(size_t)n);
    NetIo_Send(packet,n+ROUTE_HEADER,&sL.owner);return 1;
}
int Lobby_RouteRecv(uint8_t *packet,int n,struct sockaddr_in *from) {
    uint32_t gen,source,target;int a,b,k=-1,i;
    if(n<8||get(packet,0)!=MAGIC||get(packet,1)!=T_L_ROUTE)return n;
    if(n<ROUTE_HEADER+8||n>ROUTE_HEADER+ROUTE_MAX||get(packet+ROUTE_HEADER,0)!=MAGIC||get(packet+ROUTE_HEADER,1)==T_L_ROUTE)return 0;
    gen=get(packet,2);source=get(packet,3);target=get(packet,4);
    if(sL.on==1) {
        if(sL.phase!=1||gen!=sL.gen)return 0;
        a=sL.players[0];b=sL.players[1];
        if(a<=0||a>=LOBBY_SLOTS||b<=0||b>=LOBBY_SLOTS||a==b)return 0;
        for(i=1;i<LOBBY_SLOTS;++i)if(sL.m[i].on&&sL.m[i].id==source&&same(from,&sL.m[i].addr)){k=i;break;}
        if(k==a&&target==sL.m[b].id&&sL.m[b].on)NetIo_Send(packet,n,&sL.m[b].addr);
        else if(k==b&&target==sL.m[a].id&&sL.m[a].on)NetIo_Send(packet,n,&sL.m[a].addr);
        return 0;
    }
    if(sL.on!=2||!same(from,&sL.owner)||gen!=sL.inGen||target!=sL.myId||source!=sL.routePeerId)return 0;
    sL.routeViaOwner=1;
    memmove(packet,packet+ROUTE_HEADER,(size_t)(n-ROUTE_HEADER));*from=sL.routePeer;
    return n-ROUTE_HEADER;
}
