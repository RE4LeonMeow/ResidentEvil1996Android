// Port-owned localization runtime. XML conversion happens once during import;
// gameplay only reads native byte strings. Branch actions stay with the GOG game.
#include "../ModLocalization.h"
#include <unordered_map>
#include <vector>
#include <cstring>
#include <cstdint>
#include <cstdio>
namespace {
using LocaleBytes=std::vector<unsigned char>;
static std::unordered_map<unsigned,LocaleBytes> s_localeStrings,s_roomStrings;
static LocaleBytes s_saveYesNo;
static bool s_localeActive=false;
static unsigned LocaleKey(unsigned kind,unsigned index,unsigned stage=0,unsigned room=0,unsigned variant=2)
{return (kind<<28)|(stage<<24)|(room<<16)|(variant<<14)|index;}
static unsigned LocaleRead32(const unsigned char* p)
{return unsigned(p[0])|(unsigned(p[1])<<8)|(unsigned(p[2])<<16)|(unsigned(p[3])<<24);}
static int LocaleBranch(const unsigned char* p,size_t size)
{
    for(size_t at=0;at<size;){
        const unsigned c=p[at];
        if(c==1||c==7)return -1;
        if(c==8)return int(at);
        at+=(c==3||c==4||c==5||c==6||c==248||c==249||c==250)?2:1;
    }
    return -2;
}
static bool LocaleValid(const LocaleBytes& bytes,unsigned kind)
{
    bool instant=false;const bool message=kind==0||kind==1||kind==6;
    for(size_t at=0;at<bytes.size();){
        const unsigned c=bytes[at++];
        if(c==1)return !instant&&at+1==bytes.size();
        if(c==7)return kind==2&&at+2==bytes.size()&&bytes[at]==1&&bytes[at+1]==0;
        if(c==8){
            if(instant||!message||at+8!=bytes.size()||bytes[at]!=1)return false;
            for(size_t n=at+1;n<bytes.size();n++)if(bytes[n])return false;
            return true;
        }
        if(c>=3&&c<=6){
            if(!message||at==bytes.size())return false;
            if(c==4){if(bytes[at]==0){if(instant)return false;instant=true;}else instant=false;}
            at++;continue;
        }
        if(c==2){if(kind!=0&&kind!=1&&kind!=6)return false;continue;}
        if(c==0||(c>=12&&c<248)||(c==251&&kind>=3&&kind<=5))continue;
        return false;
    }
    return false;
}
static bool LocaleKeepActions(LocaleBytes& translated,const unsigned char* original)
{
    const int to=LocaleBranch(translated.data(),translated.size());
    // Original strings are trusted game data, and their byte streams always
    // terminate. This cap also guards a malformed PC resource mod.
    const int from=original?LocaleBranch(original,8192):-1;
    if(to==-2||from==-2||(to>=0)!=(from>=0))return false;
    if(to>=0){
        const unsigned jump=original[from+1];if(jump>2)return false;
        const unsigned count=jump+4;translated.resize(size_t(to)+count+2);
        std::memcpy(translated.data()+to,original+from,count);
        translated[to+count]=1;translated[to+count+1]=0;
    }
    return true;
}
static unsigned char* LocaleBind(unsigned kind,unsigned index,unsigned char* original)
{
    auto it=s_localeStrings.find(LocaleKey(kind,index));
    if(it==s_localeStrings.end())return original;
    if(!LocaleKeepActions(it->second,original)){
        std::fprintf(stderr,"[MOD TEXT] Retaining original prompt with incompatible actions (%u:%u)\n",kind,index);return original;
    }
    return it->second.data();
}
}
bool PortLocalizationActive(){return s_localeActive;}
const unsigned char* PortLocalizationText(unsigned kind,unsigned index,const unsigned char* fallback)
{
    if(!s_localeActive)return fallback;
    auto it=s_localeStrings.find(LocaleKey(kind,index));return it==s_localeStrings.end()?fallback:it->second.data();
}
unsigned char* PortLocalizationRoom(unsigned index,unsigned char* fallback)
{
    if(!s_localeActive||index>=64)return fallback;
    const unsigned stage=unsigned(g_stageId)+1,room=unsigned(g_roomId),variant=(g_main_state_flags&MSF_CHAR_VARIANT)?1:0;
    const unsigned key=LocaleKey(6,index,stage,room,variant);
    auto ready=s_roomStrings.find(key);if(ready!=s_roomStrings.end())return ready->second.empty()?fallback:ready->second.data();
    auto text=s_localeStrings.find(key);
    if(text==s_localeStrings.end())text=s_localeStrings.find(LocaleKey(6,index,stage,room));
    if(text==s_localeStrings.end())return fallback;
    LocaleBytes copy=text->second;
    if(!LocaleKeepActions(copy,fallback)){
        std::fprintf(stderr,"[MOD TEXT] Retaining original room prompt with incompatible actions (%u/%02x:%u)\n",stage,room,index);
        s_roomStrings.emplace(key,LocaleBytes{});return fallback;
    }
    return s_roomStrings.emplace(key,std::move(copy)).first->second.data();
}
bool PortLocalizationYesNo(short x,short y,unsigned char colour)
{
    const unsigned char* text=PortLocalizationText(4,4,nullptr);if(!text)return false;
    PrintFormattedText(x,y,colour,text);return true;
}
const unsigned char* PortLocalizationSaveYesNo(const unsigned char* fallback)
{
    const unsigned char* text=PortLocalizationText(4,4,nullptr);if(!text)return fallback;
    if(s_saveYesNo.empty()){
        s_saveYesNo.push_back(0);s_saveYesNo.push_back(251);
        for(unsigned i=0;i<1024;i++){s_saveYesNo.push_back(text[i]);if(text[i]==1)break;}
        s_saveYesNo.push_back(0);
    }
    return s_saveYesNo.data();
}
static int LocaleLoadData(const LocaleBytes& bytes)
{
    if(s_localeActive)return 0; // one language per fresh engine lifetime
    if(bytes.size()<16||bytes.size()>2097152||std::memcmp(bytes.data(),"R1LT",4)||LocaleRead32(bytes.data()+4)!=1||LocaleRead32(bytes.data()+12)!=bytes.size())return 0;
    const unsigned count=LocaleRead32(bytes.data()+8);if(!count||count>4096)return 0;
    std::unordered_map<unsigned,LocaleBytes> parsed;size_t at=16;
    for(unsigned i=0;i<count;i++){
        if(at+8>bytes.size())return 0;
        const unsigned key=LocaleRead32(bytes.data()+at),size=LocaleRead32(bytes.data()+at+4);at+=8;
        const unsigned kind=key>>28,index=key&0x3fff,stage=(key>>24)&15,room=(key>>16)&255,variant=(key>>14)&3;
        const unsigned limits[]={64,79,129,21,8,14,64};
        if(kind>6||variant>2||index>=limits[kind]||size<2||size>8192||at+size>bytes.size()||parsed.count(key))return 0;
        if(kind==6){if(stage<1||stage>7)return 0;}else if(stage||room||variant!=2)return 0;
        LocaleBytes value(bytes.begin()+at,bytes.begin()+at+size);
        if(!LocaleValid(value,kind))return 0;
        parsed.emplace(key,std::move(value));at+=size;
    }
    if(at!=bytes.size())return 0;
    s_localeStrings.swap(parsed);
    for(unsigned i=0;i<64;i++)global_messages[i]=LocaleBind(0,i,global_messages[i]);
    for(unsigned i=0;i<79;i++)g_ItemDescriptions[i]=LocaleBind(1,i,g_ItemDescriptions[i]);
    for(unsigned i=0;i<128;i++)g_ItemNamePointers[i]=LocaleBind(2,i+1,const_cast<unsigned char*>(g_ItemNamePointers[i]));
    s_localeActive=true;
    for(unsigned i=0;i<16;i++)g_UnknownItemNamePointers[i]=PortLocalizationText(2,i+113,g_UnknownItemNamePointers[i]);
    std::fprintf(stderr,"[MOD TEXT] Loaded %u translated strings; native GOG prompt actions retained\n",count);
    return 1;
}
extern "C" EMSCRIPTEN_KEEPALIVE int re1_web_localization_load()
{
    FILE* file=std::fopen("/game/re1-localization.bin","rb");if(!file)return 0;
    std::fseek(file,0,SEEK_END);const long length=std::ftell(file);std::rewind(file);
    if(length<16||length>2097152){std::fclose(file);return 0;}
    LocaleBytes bytes(size_t(length),0);
    const bool read=std::fread(bytes.data(),1,bytes.size(),file)==bytes.size();std::fclose(file);
    return read?LocaleLoadData(bytes):0;
}
