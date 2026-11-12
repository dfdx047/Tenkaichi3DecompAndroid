/* Signed matchmaking authority. Endpoints are compiled allowlisted destinations,
   never addresses supplied by peers or an unsigned remote document. */
#ifndef BT4_ONLINE_ROUTE_H
#define BT4_ONLINE_ROUTE_H
#include <string>
#include <cstdint>
#include <ctime>
#include <utility>
#include "update_sign.h"
#include "../update_key.h"
#include "../../include/bt4_version.h"
extern "C" int UpdCrypto_Check(const uint8_t*,const uint8_t*,const uint8_t*,size_t);
namespace bt4route {
struct Endpoint {const char *id,*host,*url,*key;};
static const Endpoint endpoints[]={
 {"primary","mhgksaiitqoijnatpurf.supabase.co","https://mhgksaiitqoijnatpurf.supabase.co","sb_publishable_T2WjPRgHLMAVgRC6juKLqg_BoKiSgww"},
 {"secondary","houhjsfukkkicjejvett.supabase.co","https://houhjsfukkkicjejvett.supabase.co","sb_publishable_JZJl6DjcnWM6M3FyqF8Ojw_Gn-HHHPN"}
};
struct Policy {
 uint64_t epoch=1,issued=0,expires=0;int active=0,capability=1;
 bool trusted=false,requireUpdate=false;std::string minimum=BT4_CANONICAL_VERSION;
 bool fresh(uint64_t now)const{return !trusted || now<expires;}
 bool needsUpdate()const{return requireUpdate && (capability>1 || UpdateVer_Compare(BT4_CANONICAL_VERSION,minimum.c_str())<0);}
};
template<class Json> bool decode(const std::string &envelope,uint64_t now,const Policy &previous,Policy &out){
 if(envelope.empty()||envelope.size()>8192)return false;
 auto doc=Json::parse(envelope,nullptr,false);if(!doc.is_object()||doc.size()!=2||!doc.contains("payload")||!doc["payload"].is_string()||!doc.contains("signature")||!doc["signature"].is_string())return false;
 auto payload=doc["payload"].template get<std::string>(),signature=doc["signature"].template get<std::string>();
 if(payload.empty()||payload.size()>4096||signature.size()!=128)return false;
 uint8_t pub[32],sig[64];if(!Upd_HexBytes(PORT_UPDATE_KEY,pub,32)||!Upd_HexBytes(signature.c_str(),sig,64))return false;
 std::string message="BT4-ONLINE-ROUTE-V1\n"+payload;
 if(!UpdCrypto_Check(sig,pub,(const uint8_t*)message.data(),message.size()))return false;
 auto body=Json::parse(payload,nullptr,false);if(!body.is_object()||body.size()!=8)return false;
 for(auto key:{"schema","epoch","issued_at","expires_at","capability"})if(!body.contains(key)||!body[key].is_number_unsigned())return false;
 if(body["schema"].template get<uint64_t>()!=1||!body.contains("active")||!body["active"].is_string()||!body.contains("minimum")||!body["minimum"].is_string()||!body.contains("require_update")||!body["require_update"].is_boolean())return false;
 Policy candidate;candidate.trusted=true;candidate.epoch=body["epoch"].template get<uint64_t>();candidate.issued=body["issued_at"].template get<uint64_t>();candidate.expires=body["expires_at"].template get<uint64_t>();
 auto cap=body["capability"].template get<uint64_t>();if(cap<1||cap>100)return false;candidate.capability=(int)cap;
 auto active=body["active"].template get<std::string>();if(active=="primary")candidate.active=0;else if(active=="secondary")candidate.active=1;else return false;
 candidate.minimum=body["minimum"].template get<std::string>();int version[4];
 if(candidate.minimum.size()>40||!UpdateVer_Parse(candidate.minimum.c_str(),version))return false;
 candidate.requireUpdate=body["require_update"].template get<bool>();
 if(candidate.epoch<1||candidate.epoch>2147483647 || candidate.issued>now+300 || candidate.expires<=now || candidate.expires<=candidate.issued || candidate.expires-candidate.issued>86400)return false;
 if(previous.trusted && (candidate.epoch<previous.epoch || (candidate.epoch==previous.epoch && candidate.active!=previous.active) || candidate.issued<previous.issued || UpdateVer_Compare(candidate.minimum.c_str(),previous.minimum.c_str())<0))return false;
 out=std::move(candidate);return true;
}
/* Existing codes never contain 0 or 1. New six-character invitations reserve
   their first character (1 primary, 0 reserve) without changing packet sizes.
   Legacy invitations always remain primary; never probe both projects. */
inline int codeProject(const std::string &code){
 if(code.size()==6&&code[0]=='0')return 1;
 return 0;
}
inline bool admission(const std::string &op){return op=="create"||op=="join"||op=="queue"||op=="ranked_queue"||op=="list";}
}
#endif
