/* Only the lower peer id initiates offers. Each offer has a monotonic revision,
 * so a delayed answer/candidate cannot attach to a recreated audio connection.
 * A device change or re-enable requests a fresh offer without resetting combat.
 */
static void reopenDirectVoice(VoicePeer &p,unsigned revision){
    unsigned id=p.id;int seat=p.seat,team=p.team,muted=p.muted;std::string name=p.name;
    closePeer(p);p.id=id;p.seat=seat;p.team=team;p.name=name;p.muted=muted;p.revision=revision;
    int error=0;p.decoder=opus.opus_decoder_create(16000,1,&error);if(!p.decoder){p.id=0;return;}openPeer(p);
}
static void directVoiceSignal(const VJson &message){
    auto *p=peerFor(number(message,"from"));auto item=message.find("body");if(!p||item==message.end()||!item->is_object())return;
    auto kind=str(message,"kind");unsigned revision=number(*item,"revision");Uint64 now=SDL_GetTicks();
    if(kind=="restart"){
        if(selfId<p->id&&now-p->offerAt>=1000)reopenDirectVoice(*p,0);
        return;
    }
    if(!revision||revision>0x7fffffffu)return;
    if(kind=="offer"){
        if(selfId<p->id||revision<incomingRevision[p->id]||(revision==p->revision&&p->remoteReady))return;
        incomingRevision[p->id]=revision;reopenDirectVoice(*p,revision);
    }else if(kind=="answer"){
        if(selfId>p->id||revision!=p->revision)return;
    }else if(kind=="candidate"){
        if(revision<p->revision)return;
        if(revision>p->revision){auto &queue=pendingVoiceCandidates[p->id];if(queue.size()<24)queue.push_back(*item);return;}
    }else return;
    if(p->pc<0)return;
    auto signal=*item;signal["type"]=kind;
    VJson envelope={{"type","signal"},{"from",p->id.load()},{"signal",signal}};auto encoded=envelope.dump();
    control((const unsigned char*)encoded.data(),encoded.size());
    if(kind=="offer"){
        auto &waiting=pendingVoiceCandidates[p->id];
        for(auto &candidate:waiting)if(number(candidate,"revision")==p->revision){
            auto value=candidate;value["type"]="candidate";VJson event={{"type","signal"},{"from",p->id.load()},{"signal",value}};
            auto bytes=event.dump();control((const unsigned char*)bytes.data(),bytes.size());
        }
        waiting.clear();
    }
}
static void directVoiceRetry(){
    Uint64 now=SDL_GetTicks();
    for(auto &p:peers)if(p.id&&!p.connected){
        if(selfId<p.id){if(now-p.offerAt>=5000)reopenDirectVoice(p,0);}
        else if(!p.requestAt||now-p.requestAt>=5000){p.requestAt=now;post({{"type","signal"},{"to",p.id.load()},{"signal",{{"type","restart"}}}});}
    }
}
