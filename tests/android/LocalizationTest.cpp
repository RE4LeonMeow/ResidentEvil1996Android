// Compile the real port adapter with small synthetic game tables, no game data.
#define __EMSCRIPTEN__ 1
#define EMSCRIPTEN_KEEPALIVE
#include <cassert>
#include <cstdio>
#include <vector>
#include <utility>
static unsigned char nativeText[]={29,1,0};
static unsigned char nativePrompt[]={29,8,2,10,1,1,0};
unsigned char* global_messages[64];
unsigned char* g_ItemDescriptions[79];
const unsigned char* g_ItemNamePointers[128];
const unsigned char* g_UnknownItemNamePointers[16];
unsigned g_stageId=0,g_roomId=6,g_main_state_flags=0;
static const unsigned MSF_CHAR_VARIANT=0x800000;
static const unsigned char* lastPrinted=nullptr;
void PrintFormattedText(short,short,unsigned char,const unsigned char* text){lastPrinted=text;}
#include "../../src/platform/web/localization.inl"

static void put(LocaleBytes& bytes,unsigned n){for(unsigned i=0;i<4;i++)bytes.push_back((n>>(i*8))&255);}
static LocaleBytes pack(std::initializer_list<std::pair<unsigned,LocaleBytes>> records){
    LocaleBytes bytes={'R','1','L','T'};put(bytes,1);put(bytes,records.size());put(bytes,0);
    for(const auto& record:records){put(bytes,record.first);put(bytes,record.second.size());bytes.insert(bytes.end(),record.second.begin(),record.second.end());}
    const unsigned size=bytes.size();for(unsigned i=0;i<4;i++)bytes[12+i]=(size>>(i*8))&255;return bytes;
}
int main(){
    for(auto& p:global_messages)p=nativeText;
    for(auto& p:g_ItemDescriptions)p=nativeText;
    for(auto& p:g_ItemNamePointers)p=nativeText;
    for(auto& p:g_UnknownItemNamePointers)p=nativeText;
    global_messages[0]=nativePrompt;
    assert(!PortLocalizationActive());assert(PortLocalizationRoom(0,nativeText)==nativeText);
    const LocaleBytes translatedPrompt={61,8,1,0,0,0,0,0,0,0};
    auto valid=pack({{LocaleKey(0,0),translatedPrompt},{LocaleKey(0,1),translatedPrompt},
        {LocaleKey(1,0),{95,1,0}},{LocaleKey(2,1),{95,7,1,0}},{LocaleKey(2,113),{61,7,1,0}},
        {LocaleKey(4,4),{29,0,0,61,1,0}},
        {LocaleKey(6,0,1,6),translatedPrompt},{LocaleKey(6,0,1,6,1),{95,8,1,0,0,0,0,0,0,0}},
        {LocaleKey(6,1,1,6),translatedPrompt}});
    auto invalid=valid;invalid[4]=2;assert(!LocaleLoadData(invalid));assert(!PortLocalizationActive());
    invalid=valid;invalid.pop_back();assert(!LocaleLoadData(invalid));
    assert(!LocaleLoadData(pack({{LocaleKey(0,64),{29,1,0}}})));
    assert(!LocaleLoadData(pack({{LocaleKey(0,0),{29,1,0}},{LocaleKey(0,0),{29,1,0}}})));
    assert(!LocaleLoadData(pack({{LocaleKey(0,0),{4,0,29,1,0}}}))); // missing end of instant section
    assert(!LocaleLoadData(pack({{LocaleKey(0,0),{8,1}}}))); // incomplete action placeholder
    assert(LocaleLoadData(valid));assert(PortLocalizationActive());assert(!LocaleLoadData(valid));
    assert(global_messages[0][0]==61);
    assert(!std::memcmp(global_messages[0]+1,nativePrompt+1,6)); // original choice/action bytes
    assert(global_messages[1]==nativeText); // XML cannot add an action to a non-prompt
    assert(g_ItemDescriptions[0][0]==95);assert(g_ItemNamePointers[0][0]==95);assert(g_ItemNamePointers[0][1]==7);
    assert(g_UnknownItemNamePointers[0][0]==61);
    auto chris=PortLocalizationRoom(0,nativePrompt);assert(chris[0]==61);assert(!std::memcmp(chris+1,nativePrompt+1,6));
    g_main_state_flags=MSF_CHAR_VARIANT;auto jill=PortLocalizationRoom(0,nativePrompt);assert(jill[0]==95);assert(chris[0]==61);
    assert(PortLocalizationRoom(1,nativeText)==nativeText);assert(PortLocalizationRoom(2,nativeText)==nativeText);
    assert(PortLocalizationYesNo(0,0,0));assert(lastPrinted[0]==29);
    const auto choices=PortLocalizationSaveYesNo(nativeText);assert(choices[0]==0&&choices[1]==251&&choices[2]==29);
    std::puts("PASS: native translation cache validation, item/generic names, unchanged GOG choice actions, scenario-specific room text, fallbacks and save choices");
}
