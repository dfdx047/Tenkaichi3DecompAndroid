/* Native stage expansion, before relocation. The original files stay intact.
 * Build a bounded set of coordinate words first, then apply it atomically.
 * Shared vertex packets are scaled once. Sky, UVs, normals, lighting, angles,
 * animation times and character resources are deliberately not coordinates. */
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <set>
#include <map>
#include <algorithm>

namespace {
struct Plan {
    uint8_t *data;
    size_t bytes, base=0, end;
    bool valid=true;
    const char *section="package";
    std::vector<uint8_t> marked;
    std::vector<size_t> fields;
    std::vector<std::pair<size_t,size_t>> parts;
    std::map<size_t,uint32_t> repairs;
    bool checkOnly=false;
    Plan(void *p,size_t n):data((uint8_t*)p),bytes(n),end(n),marked((n+3)/4) {}
    bool has(size_t p,size_t n) {
        if(p>end-base || n>end-base-p) valid=false;
        return valid;
    }
    uint32_t u(size_t p) {
        uint32_t v=0;
        auto r=repairs.find(base+p);
        if(r!=repairs.end()) return r->second;
        if(has(p,4)) std::memcpy(&v,data+base+p,4);
        return v;
    }
    uint16_t h(size_t p) {
        uint16_t v=0;
        if(has(p,2)) std::memcpy(&v,data+base+p,2);
        return v;
    }
    size_t word(size_t p) {return size_t(u(p))*4;}
    bool table(size_t p,size_t count,size_t stride) {
        if(count>1000000 || p%4) {valid=false; return false;}
        return has(p,count*stride);
    }
    void xyz(size_t p,size_t count=3,float limit=10000000.0f) {
        if(p%4 || !has(p,count*4)) {valid=false; return;}
        for(size_t i=0;i<count;i++,p+=4) {
            float f; std::memcpy(&f,data+base+p,4);
            if(!std::isfinite(f) || std::abs(f)>limit) {valid=false; return;}
            size_t k=(base+p)/4;
            if(!checkOnly && !marked[k]) {marked[k]=1;fields.push_back(base+p);}
        }
    }
    bool part(size_t n) {
        if(n>=parts.size()) {valid=false;return false;}
        base=parts[n].first;end=parts[n].second;return base!=end;
    }
    bool package() {
        size_t count=u(0), previous=4*(count+2);
        if(count<24 || count>64 || !has(0,previous)) return valid=false;
        for(size_t i=0;i<=count;i++) {
            size_t offset=u(4+i*4)&~size_t(3);
            if(offset<previous || offset>bytes) return valid=false;
            if(i) parts.emplace_back(previous,offset);
            previous=offset;
        }
        return valid;
    }
    void collider(size_t at) {
        if(!has(at,24)) return;
        if(int32_t(u(at))<=0) return;
        size_t nc=u(at),tc=u(at+4),np=word(at+12),tp=word(at+16),vp=word(at+20);
        if(!tc || !table(np,nc,32) || !table(tp,tc,32)) {valid=false;return;}
        xyz(at+24,6);
        for(size_t i=0;i<nc && valid;i++) {
            size_t n=np+i*32; xyz(n,6);
            uint32_t a=u(n+24),b=u(n+28);
            if(b==0xffffffffu ? a>=tc : a>=nc || b>=nc) valid=false;
        }
        for(size_t i=0;i<tc && valid;i++) {
            size_t t=tp+i*32;
            for(size_t o:{4,8,12}) {
                uint32_t index=u(t+o);
                if(int32_t(index)<0) {valid=false;return;}
                xyz(vp+size_t(index)*16);
            }
            xyz(t+28,1);
        }
    }
    void packet(size_t at) {
        uint32_t tag=u(at);
        size_t last=at+16+(tag&65535)*16,cur=at+8;
        if(((tag>>28)&7)!=6 || !has(at,last-at)) {valid=false;return;}
        while(cur<last && valid) {
            uint32_t op=u(cur);cur+=4;
            unsigned cmd=(op>>24)&127,count=(op>>16)&255,imm=op&65535;
            if(cmd==0 || cmd==0x17) continue;
            if(cmd!=0x6c || !count || count*16>last-cur) {valid=false;break;}
            if(imm==0x8003 && count%3==0) {
                // Some custom stages hide unused scenery at Y=1e9. It is
                // still valid vertex data and stays offstage when scaled.
                for(size_t p=cur;p<cur+count*16;p+=48) xyz(p,3,10000000000.0f);
            } else if(imm!=0x8000 || (count!=1 && count!=3)) valid=false;
            cur+=count*16;
        }
        if(cur!=last) valid=false;
    }
    void root() {
        section="terrain";part(0);
        if(!has(0,96)) return;
        xyz(word(64));xyz(76,1);
        size_t count=u(56),start=word(60);
        std::set<size_t> colliders,packets,sky;
        if(!table(start,count,64)) return;
        for(size_t i=0;i<count && valid;i++) {
            size_t p=start+i*64;xyz(p+28,1);xyz(p+32);xyz(p+48);
            if(word(p+4)) colliders.insert(word(p+4));
            size_t n=u(p+20),a=word(p+24);
            if(!table(a,n,16)) break;
            for(size_t j=0;j<n;j++) if(word(a+j*16+8)) colliders.insert(word(a+j*16+8));
        }
        section="collision";for(size_t c:colliders) if(valid) collider(c);
        if(!valid) return;
        section="visibility";
        std::vector<size_t> pending{word(80)};std::set<size_t> visited;
        while(!pending.empty() && valid) {
            size_t p=pending.back();pending.pop_back();
            if(!p || !visited.insert(p).second || visited.size()>100000 || !has(p,64)) {valid=false;break;}
            xyz(p+40);
            for(size_t k=1;k<=8;k++) if(word(p+k*4)) pending.push_back(word(p+k*4));
        }
        count=u(32);start=word(36);
        if(!table(start,count,8)) return;
        for(size_t i=0;i<count && valid;i++) {
            size_t p=start+i*8,n=u(p),a=word(p+4);
            if(!table(a,n,32)) break;
            for(size_t j=0;j<n;j++) xyz(a+j*32+12,4);
        }
        section="debris";count=u(48);start=word(52);
        if(!table(start,count,80)) return;
        for(size_t i=0;i<count && valid;i++) {
            size_t p=start+i*80,n=u(p+64),a=word(p+68);
            if(!table(a,n,32)) break;
            for(size_t j=0;j<n;j++) xyz(a+j*32);
        }
        section="vertices";count=u(24);start=word(28);
        if(!table(start,count,8)) return;
        for(size_t i=0;i<count && valid;i++) {
            size_t p=start+i*8,n=h(p+2),a=word(p+4);
            if(!table(a,n,16)) break;
            for(size_t j=0;j<n && valid;j++) {
                size_t q=a+j*16,m=h(q+2),b=word(q+4);
                if(!table(b,m,16)) break;
                for(size_t k=0;k<m;k++) (i?packets:sky).insert(word(b+k*16+8));
            }
        }
        for(size_t p:packets) {
            if(sky.count(p)) valid=false;
            if(valid) packet(p);
        }
    }
    void placements() {
        section="placements";part(1);if(!has(0,168)) return;
        for(size_t c:{8,40,48,56}) {
            size_t n=u(c),p=u(c+4),stride=c==8?64:32;
            if(!table(p,n,stride)) return;
            for(size_t i=0;i<n;i++) {
                if(c==8) xyz(p+i*stride+32);
                else {xyz(p+i*stride);xyz(p+i*stride+16);}
            }
        }
        size_t n=u(16),p=u(20);
        if(!table(p,n,16)) return;
        for(size_t i=0;i<n;i++) if(u(p+i*16+4)&2) xyz(p+i*16+8,1);
    }
    void cameras() {
        section="intro cameras";
        for(size_t i=2;i<5 && valid;i++) {
            if(!part(i)) continue;
            if(!has(0,48) || u(0)!=0x6e414d43 || u(4)!=6) {valid=false;return;}
            xyz(u(24));
            size_t n=u(12),p=u(8);if(n>8 || !table(p,n,16)) {valid=false;return;}
            for(size_t j=0;j<n && valid;j++) {
                size_t q=p+j*16,k=u(q+8),a=u(q+4);
                if(!table(a,k,32)) return;
                if(u(q)<3) for(size_t t=0;t<k;t++) xyz(a+t*32+8,1);
            }
        }
    }
    void surfaces() {
        section="water and lava";
        if(!part(12)) return;
        if(!has(0,96)) return;
        base+=64;
        if(u(0)!=0x336c646d || u(4)!=4) {valid=false;return;}
        size_t count=u(24),start=u(28);
        if(!table(start,count,96)) return;
        for(size_t i=0;i<count && valid;i++) {
            size_t p=start+i*96,n=u(p+4),a=u(p+8);
            xyz(p+48);xyz(p+64);xyz(p+80);
            if(!table(a,n,144)) return;
            for(size_t k=0;k<n*3;k++) xyz(a+k*48);
        }
    }
    void effects() {
        section="ambient sources";
        if(part(18)) {
            if(u(0)!=0x3046454d) {valid=false;return;}
            for(size_t c:{8,16,24}) {
                size_t n=u(c),p=u(c+4);
                if(!table(p,n,48)) return;
                for(size_t i=0;i<n;i++) xyz(p+i*48);
            }
        }
        section="stage paths";
        if(part(23)) {
            size_t n=u(8),p=u(12);
            if(!table(p,n,16)) return;
            for(size_t i=0;i<n && valid;i++) {
                size_t q=p+i*16,k=u(q),a=u(q+4);
                if(!table(a,k,48)) return;
                for(size_t j=0;j<k;j++) xyz(a+j*48);
            }
        }
    }
    bool build() {
        if(!package()) return false;
        root();if(!valid)return false;
        placements();if(!valid)return false;
        cameras();if(!valid)return false;
        surfaces();if(!valid)return false;
        effects();return valid;
    }
};
}
/* A few B14 maps contain stale offsets left by their map exporter. Repair
   only structurally verified self-contained meshes, or an invalid static
   mesh in a zone wholly outside the playable cylinder. Changes are applied
   together after every affected record has been checked. Disc files stay
   untouched, and the same repair is used at normal and expanded size. */
extern "C" int Port_TagStageRepair(void *file,int capacity) {
    if(!file || capacity<128 || capacity>128*1024*1024) return 0;
    Plan p(file,size_t(capacity));p.checkOnly=true;
    if(!p.package() || !p.part(0) || !p.has(0,96)) return 0;
    size_t count=p.u(56),start=p.word(60),empty=0;
    if(!p.table(start,count,64)) return 0;
    std::set<size_t> meshes,objectMeshes;
    std::map<size_t,std::vector<size_t>> staticRefs;
    for(size_t i=0;i<count && p.valid;i++) {
        size_t z=start+i*64,c=p.word(z+4);
        if(c) {meshes.insert(c);staticRefs[c].push_back(z);}
        size_t n=p.u(z+20),a=p.word(z+24);
        if(!p.table(a,n,16)) return 0;
        for(size_t j=0;j<n;j++) if(p.word(a+j*16+8)) {
            size_t object=p.word(a+j*16+8);
            meshes.insert(object);objectMeshes.insert(object);
        }
    }
    for(size_t c:meshes) if(p.has(c,24) && p.u(c)==0) {empty=c;break;}
    if(!p.valid) return 0;
    for(size_t c:meshes) {
        p.collider(c);
        if(p.valid) continue;
        p.valid=true;
        size_t nc=p.u(c),tc=p.u(c+4),np=p.word(c+12),tp=p.word(c+16),vp=p.word(c+20);
        bool fixed=false;
        // B14's removed props use this invalid three-index marker on every
        // triangle. Their old normals and bounds remain in the file. The
        // negative indices land in unrelated data, sometimes the visibility
        // lists; never interpret or scale that memory as vertex positions.
        // Make these explicitly empty using the native collision convention.
        if(nc>0 && nc<100000 && tc>0 && vp==c+48 && tp>vp &&
           (tp-vp)%16==0 && np==tp+tc*32 && p.table(tp,tc,32)) {
            bool removed=true;
            for(size_t i=0;i<tc && removed;i++)
                for(size_t j=0;j<3;j++)
                    if(p.u(tp+i*32+4+j*4)!=0xffff4785u+j) removed=false;
            if(removed) {p.repairs[p.base+c]=0;fixed=true;}
        }
        if(!fixed && nc>0 && nc<100000 && tc>0 && nc==2*tc-1 && tp>vp &&
           (tp-vp)%16==0 && np==tp+tc*32 && vp!=c+48) {
            size_t newVp=c+48,newTp=newVp+(tp-vp),newNp=newVp+(np-vp);
            if(newNp<=p.end-p.base && nc*32<=p.end-p.base-newNp &&
               std::memcmp(p.data+p.base+c+24,p.data+p.base+newNp,24)==0) {
                p.repairs[p.base+c+12]=uint32_t(newNp/4);
                p.repairs[p.base+c+16]=uint32_t(newTp/4);
                p.repairs[p.base+c+20]=uint32_t(newVp/4);
                p.collider(c);fixed=p.valid;
            }
        }
        if(!fixed && empty && staticRefs.count(c) && !objectMeshes.count(c)) {
            p.valid=true;float radius;
            size_t bounds=p.word(64);
            if(!p.has(bounds,4)) return 0;
            std::memcpy(&radius,p.data+p.base+bounds,4);
            if(!std::isfinite(radius) || radius<=0) return 0;
            fixed=true;
            for(size_t z:staticRefs[c]) {
                float center[3],half[3];
                std::memcpy(center,p.data+p.base+z+32,12);
                std::memcpy(half,p.data+p.base+z+48,12);
                float x=std::max(0.0f,std::abs(center[0])-half[0]);
                float y=std::max(0.0f,std::abs(center[2])-half[2]);
                if(!std::isfinite(x+y) || x*x+y*y<=radius*radius) {fixed=false;break;}
                p.repairs[p.base+z+4]=uint32_t(empty/4);
            }
        }
        if(!fixed || !p.valid) return 0;
    }
    for(auto r:p.repairs) std::memcpy(p.data+r.first,&r.second,4);
    if(!p.repairs.empty()) std::fprintf(stderr,"tagstage: repaired %zu exported collision offsets\n",p.repairs.size());
    return 1;
}
extern "C" int Port_TagStageScale(void *file,int capacity,int percent) {
    if(percent<=100) return 100;
    if(!file || capacity<128 || capacity>128*1024*1024) return 100;
    Plan plan(file,size_t(capacity));
    if(!plan.build()) {
        std::fprintf(stderr,"tagstage: unsupported %s layout; kept original stage\n",plan.section);
        return 100;
    }
    float scale=std::min(percent,300)/100.0f;
    for(size_t p:plan.fields) {
        float v;std::memcpy(&v,plan.data+p,4);v*=scale;std::memcpy(plan.data+p,&v,4);
    }
    std::fprintf(stderr,"tagstage: scale=%d fields=%zu bytes=%zu\n",percent,plan.fields.size(),plan.parts.back().second);
    return std::min(percent,300);
}
#ifndef TAG_STAGE_SCALE_TOOL
extern "C" long Port_FileSize(const char *);
extern "C" size_t Port_FileReadAt(const char *,long,void *,size_t);
extern "C" int PortGame_IsBT4(void);
extern "C" int Port_TagStagePan(int stage,int fallback) {
    // PS2 split packages clear this flag to save rendering work. Read just
    // the original arena's parameter header, through the normal mod-aware
    // file layer, so each arena retains its own authored choice on PC.
    const bool bt4=PortGame_IsBT4()!=0;
    if(stage<0 || stage>=(bt4?99:36)) return fallback;
    char path[64]; std::snprintf(path,sizeof(path),bt4?"pzs4us1/%05d.bin":"pzs3us1/%05d.bin",(bt4?1522:369)+stage);
    long length=Port_FileSize(path);
    uint32_t head[4],param[2],flags;
    if(length<128 || length>128*1024*1024 ||
       Port_FileReadAt(path,0,head,sizeof(head))!=sizeof(head)) return fallback;
    size_t base=head[2]&~size_t(3),end=head[3]&~size_t(3);
    if(head[0]<2 || head[0]>64 || base<4*(head[0]+2) ||
       end>size_t(length) || end<base || end-base<0x78) return fallback;
    if(Port_FileReadAt(path,long(base+0x70),param,sizeof(param))!=sizeof(param) ||
       !param[0] || param[1]%4 || param[1]>end-base-4) return fallback;
    if(Port_FileReadAt(path,long(base+param[1]),&flags,4)!=4) return fallback;
    return (flags&8)!=0;
}
#endif
#ifdef TAG_STAGE_SCALE_TOOL
/* Standalone audit uses precisely the runtime parser on the user's own files. */
int main(int argc,char **argv) {
    int failed=0;
    for(int i=1;i<argc;i++) {
        FILE *f=std::fopen(argv[i],"rb");
        if(!f) return 2;
        std::fseek(f,0,SEEK_END);long length=std::ftell(f);std::rewind(f);
        std::vector<uint8_t> bytes(length);
        if(std::fread(bytes.data(),1,bytes.size(),f)!=bytes.size()) return 2;
        std::fclose(f);
        int result=Port_TagStageScale(bytes.data(),(int)bytes.size(),200);
        std::printf("%s %s\n",result==200?"PASS":"FAIL",argv[i]);
        failed+=result!=200;
    }
    return failed ? 1 : 0;
}
#endif
