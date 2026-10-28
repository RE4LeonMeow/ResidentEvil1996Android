// Data-driven campaign settings; imported Windows plugins are never executed.
// Wesker's Rebirth's spawn constants are verified from aydan.dll DllMain
// (0x10001280): MediaKite operands 0x0048db2c/3f/48 -> 8000/6615/3584.
// Its new-game hook (0x100012c0/12f0) follows the public RE1-Mod-SDK's
// fast-chris example at https://github.com/Gemini-Loboto3/RE1-Mod-SDK.
#include "../ModCampaign.h"
#include <array>
#include <string>
#include <memory>
#include <vector>
#include <cstdint>
#include <algorithm>
namespace {
struct CampaignProfile {
    unsigned character=0,skipSelection=0,countdownSeconds=180;
    short x=0,z=0,angle=0;
    std::array<unsigned char,1052> card{};
    std::array<unsigned char,40> inventory{};
    std::array<unsigned char,896> musicRooms{};
    std::array<std::array<std::string,4>,57> musicNames;
    std::array<std::array<unsigned char,4>,57> musicLoops{};
    std::array<std::array<std::string,48>,203> sounds;
    std::array<bool,203> soundRows{};
};
std::unique_ptr<CampaignProfile> s_campaign;
struct CampaignReader {
    const std::vector<unsigned char>& data;size_t at=0;bool good=true;
    unsigned u8(){if(at>=data.size()){good=false;return 0;}return data[at++];}
    unsigned u16(){const unsigned lo=u8();return lo|(u8()<<8);}
    unsigned u32(){const unsigned lo=u16();return lo|(u16()<<16);}
    bool copy(unsigned char* dest,size_t count){
        if(at>data.size()||count>data.size()-at){good=false;return false;}
        std::memcpy(dest,data.data()+at,count);at+=count;return true;
    }
    std::string name(){
        const unsigned size=u8();if(size>63){good=false;return {};}
        std::string text;
        for(unsigned n=0;n<size;n++){
            const unsigned c=u8();
            if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='+'||c=='&'||c=='-'||c=='.'))good=false;
            text.push_back(char(c));
        }
        if(text.find("..")!=std::string::npos)good=false;
        return text;
    }
};
bool CampaignLoad(const std::vector<unsigned char>& data){
    if(s_campaign||data.size()<2020||data.size()>262144)return false;
    CampaignReader r{data};
    if(r.u32()!=0x50433152||r.u32()!=1||r.u32()!=data.size())return false;
    auto parsed=std::make_unique<CampaignProfile>();
    parsed->character=r.u8();parsed->skipSelection=r.u8();
    parsed->x=short(r.u16());parsed->z=short(r.u16());parsed->angle=short(r.u16());
    const unsigned card=r.u16(),inventory=r.u16(),sounds=r.u16(),groups=r.u16(),rooms=r.u16(),seconds=r.u16();
    if(parsed->character>1||parsed->skipSelection>1||parsed->angle<0||parsed->angle>4095||card!=1052||inventory!=40||sounds>203||groups!=57||rooms!=224||(seconds&&(seconds<60||seconds>3600||seconds%60)))return false;
    parsed->countdownSeconds=seconds?seconds:180;
    r.copy(parsed->card.data(),card);r.copy(parsed->inventory.data(),inventory);r.copy(parsed->musicRooms.data(),896);
    if(!r.good||parsed->card[512]>=7||parsed->card[513]>=29||parsed->card[514]>=32)return false;
    for(unsigned n=0;n<40;n+=2)if(parsed->inventory[n]>128)return false;
    for(unsigned char value:parsed->musicRooms)if(value!=255&&value>=57)return false;
    for(unsigned group=0;group<57;group++)for(unsigned channel=0;channel<4;channel++){
        parsed->musicNames[group][channel]=r.name();const unsigned loop=r.u8();
        if(loop>1)r.good=false;parsed->musicLoops[group][channel]=static_cast<unsigned char>(loop);
    }
    for(unsigned row=0;row<sounds;row++){
        const unsigned key=r.u16();if(key>=203||parsed->soundRows[key])return false;
        parsed->soundRows[key]=true;
        for(auto& name:parsed->sounds[key])name=r.name();
    }
    if(!r.good||r.at!=data.size())return false;
    s_campaign=std::move(parsed);
    std::fprintf(stderr,"[MOD CAMPAIGN] Loaded data profile: stage %u room %02x, character %u, %u sound rows\n",s_campaign->card[512]+1,s_campaign->card[513],s_campaign->character,sounds);
    return true;
}
}
extern "C" EMSCRIPTEN_KEEPALIVE int re1_web_campaign_load(){
    FILE* file=std::fopen("/game/re1-campaign.bin","rb");if(!file)return 0;
    std::fseek(file,0,SEEK_END);const long size=std::ftell(file);std::rewind(file);
    if(size<2020||size>262144){std::fclose(file);return 0;}
    std::vector<unsigned char> data(size_t(size),0);
    const bool read=std::fread(data.data(),1,data.size(),file)==data.size();std::fclose(file);
    return read&&CampaignLoad(data)?1:0;
}
bool PortCampaignActive(){return bool(s_campaign);}
EM_JS(int,re1_web_campaign_background_override,(const char* path),{
    return globalThis.RE1Mods?.fileOverridden(UTF8ToString(path))?1:0;
});
bool PortCampaignBackgroundOverride(const char* gameRelativePath){
    return s_campaign&&gameRelativePath&&re1_web_campaign_background_override(gameRelativePath);
}
// REWM.exe 0x48cece, 0x48cfdd and 0x48cff4 encode 600, 9 and 539.
// The normal game's corresponding values are 180, 2 and 119.
unsigned PortCampaignCountdownSeconds(){return s_campaign?s_campaign->countdownSeconds:180;}
bool PortCampaignNewGame(){
    if(!s_campaign||!s_campaign->skipSelection)return false;
    g_main_state_flags&=~(MSF_CHAR_VARIANT|MSF_CONTINUE_GAME);
    if(s_campaign->character==1)g_main_state_flags|=MSF_CHAR_VARIANT;
    g_SelectedCharactedId=static_cast<unsigned char>(s_campaign->character);
    return true;
}
void PortCampaignCard(unsigned char* data,std::size_t size){
    if(!s_campaign||!data||size!=1052||(g_main_state_flags&MSF_CONTINUE_GAME))return;
    // Keep the selected GOG game's save-card header; only the documented
    // initial game-state payload comes from the campaign profile.
    std::memcpy(data+512,s_campaign->card.data()+512,540);
}
void PortCampaignInitPlayer(){
    if(!s_campaign||(g_main_state_flags&MSF_CONTINUE_GAME))return;
    g_playerEntity.position.x=s_campaign->x;g_playerEntity.position.z=s_campaign->z;
    g_playerEntity.directionAngle=s_campaign->angle;
    const unsigned offset=(g_playerEntity.id&3)==1?0:16;
    const unsigned count=(g_playerEntity.id&3)==1?8:6;
    for(unsigned n=0;n<8;n++){
        g_ItemsSlots[n].Id=n<count?s_campaign->inventory[offset+n*2]:0;
        g_ItemsSlots[n].qty=n<count?s_campaign->inventory[offset+n*2+1]:0;
    }
    for(unsigned n=0;n<6;n++){
        g_RebeccaItemSlots[n].Id=s_campaign->inventory[28+n*2];
        g_RebeccaItemSlots[n].qty=s_campaign->inventory[29+n*2];
    }
    std::fprintf(stderr,"[MOD CAMPAIGN] Initial player x=%d z=%d angle=%d; item0=%u count=%u\n",s_campaign->x,s_campaign->z,s_campaign->angle,g_ItemsSlots[0].Id,g_ItemsSlots[0].qty);
}
const char* PortCampaignRoomSound(unsigned room,unsigned slot,const char* fallback){
    if(!s_campaign||room>=203||slot>=48||!s_campaign->soundRows[room])return fallback;
    const auto& name=s_campaign->sounds[room][slot];return name.empty()?nullptr:name.c_str();
}
unsigned char PortCampaignBgmGroup(unsigned index,unsigned char fallback){
    return s_campaign&&index<896?s_campaign->musicRooms[index]:fallback;
}
const char* PortCampaignBgmName(unsigned group,unsigned channel,const char* fallback){
    if(!s_campaign||group>=57||channel>=4)return fallback;
    const auto& name=s_campaign->musicNames[group][channel];return name.empty()?nullptr:name.c_str();
}
unsigned char PortCampaignBgmLoop(unsigned group,unsigned channel,unsigned char fallback){
    return s_campaign&&group<57&&channel<4?s_campaign->musicLoops[group][channel]:fallback;
}
