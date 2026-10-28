// Exercise the exact Android service and its gameplay guards in wasm32.
#include "Globals.h"
#include <assert.h>
#include <stdio.h>
#include <initializer_list>
#include "platform/web/presentation.h"
BOOL g_bWindowFocused=TRUE, g_bQuitFlag=FALSE, g_bHasFinalizedSettings=FALSE;
BOOL g_bUseFrameSkip=TRUE,g_bFrameSkipDetected=FALSE;
DWORD g_dwGameTimer1=1000;
static DWORD testTime=1000;
DWORD plat_time_ms(){return testTime;}
int g_playingGameFlag=1;
// Match the declarations in Globals.h; these are harness-owned game state.
decltype(g_bMCINotifyEnabled) g_bMCINotifyEnabled=0;
decltype(g_openMenuFlag) g_openMenuFlag=0;
decltype(g_loadSaveStateFlag) g_loadSaveStateFlag=0;
unsigned int g_MainStateFlagBank[2]={};
decltype(g_message_flags) g_message_flags=0x141;
decltype(g_messageFlagsBackup) g_messageFlagsBackup=0;
int g_roomTransitionBusy=0;
PlayerEntity g_playerEntity={};
decltype(g_EnemiesList) g_EnemiesList={};
decltype(g_enemy_count) g_enemy_count=0;
TaskControlBlock g_TasksTable[3]={};
void* g_TasksEIP[3]={};
BOOL g_displayExitGameScreen_flag=FALSE;
BioCardLayout g_BioCard={};
MATRIX g_RoomCameraData={};
int g_sceneRenderParam=192;
void game_start() {} void main_menu() {} void options_menu() {}
static bool inventoryLayout=true;
static bool itemBoxLayout=false;
static int mapFilesLayout=0;
decltype(g_itembox_state) g_itembox_state=0;
bool MenuUsesInventoryLayout(){return inventoryLayout;}
bool MenuUsesItemBoxLayout(){return itemBoxLayout;}
int MenuMapFilesLayout(){return mapFilesLayout;}
void ConfigFile_Save() {}
void* g_pMarniDirect3D=nullptr;
// Port-cheat service dependencies, all harness-owned, not game data.
decltype(g_typewriter_state) g_typewriter_state=0;
decltype(g_typewriter_id) g_typewriter_id=0;
decltype(g_debugOpenItemboxFlag) g_debugOpenItemboxFlag=0;
void* g_ItemSlotsPointer=nullptr;
unsigned char g_ItemMaxQty[448]={};
extern "C" int re1_web_quick_turn();
extern "C" int re1_web_cheat(int, int);
extern "C" void re1_web_set_smooth(int);
extern "C" unsigned re1_web_interpolation_scene();
extern "C" int re1_web_interpolation_wait_ms();
#include "game/PortCheats.h"

// New campaign/localization/input exports share this service translation unit.
// Supply their storage only; this test invokes no archive or localization data.
#include <cstring>
decltype(global_messages) global_messages={};
decltype(g_ItemDescriptions) g_ItemDescriptions={};
decltype(g_ItemNamePointers) g_ItemNamePointers={};
decltype(g_UnknownItemNamePointers) g_UnknownItemNamePointers={};
decltype(g_controllerConfig) g_controllerConfig=0;
decltype(g_JoyRemapTbl) g_JoyRemapTbl={};
decltype(g_joyRemapBackupKey) g_joyRemapBackupKey={};
decltype(g_joyRemapBackupJoy) g_joyRemapBackupJoy={};
DWORD g_PlayerPadHeld=0,g_PlayerPadPressed=0,g_PlayerPadHeldPrev=0,g_RawPadHeld=0,g_PadBtnWord=0,g_button_pressed_id=0;
WORD g_RawPadState=0,g_padEdgeDetectedWord=0,g_PlayerDpadHeldPrev=0;
int g_DisplayImageWidth=320,g_DisplayImageHeight=240,g_bgCacheMode=1;
void InstallPadDefaultBindings(){}

static void unchanged(){short angle=g_playerEntity.directionAngle;assert(re1_web_quick_turn()==0);assert(g_playerEntity.directionAngle==angle);}
int main(){
 g_TasksTable[0].state=1;g_TasksEIP[0]=(void*)game_start;
 g_playerEntity.health=100;g_playerEntity.animationId=1;g_playerEntity.directionAngle=0xe00;
 assert(re1_web_quick_turn()==1);assert(g_playerEntity.directionAngle==0x600);
 assert(re1_web_quick_turn()==1);assert(g_playerEntity.directionAngle==0xe00);
 for(unsigned bit:{MSF_FMV_REQUEST,MSF_DOOR_TRANSITION,MSF_ROOM_TRANSITION,MSF_OBJECT_PUSH,MSF_PLAYER_DEAD}){g_main_state_flags=bit;unchanged();}g_main_state_flags=0;
 g_main_state_flags2=MSF2_ATTRACT_DEMO;unchanged();g_main_state_flags2=0;
 g_message_flags=0;unchanged();g_message_flags=0x41;
 g_openMenuFlag=1;unchanged();g_openMenuFlag=0;
 // Menu state is the live task, not historical load/menu-request flags.
 g_TasksEIP[1]=(void*)options_menu;g_TasksTable[1].state=1;unchanged();assert(WebPresentationMode()==0);
 g_TasksTable[1].state=0;
 g_loadSaveStateFlag=1;g_main_state_flags=MSF_MENU_ACTIVE|MSF_OPTIONS_REQUEST|MSF_FADE_ACTIVE;
 g_message_flags=1;g_playerEntity.isBeingAttackedFlag=0x80;
 assert(re1_web_quick_turn()==1);assert(WebPresentationMode()==0);
 g_loadSaveStateFlag=0;g_main_state_flags=0;g_message_flags=0x41;g_playerEntity.isBeingAttackedFlag=0;
 g_bMCINotifyEnabled=1;unchanged();g_bMCINotifyEnabled=0;
 g_bWindowFocused=FALSE;unchanged();g_bWindowFocused=TRUE;
 g_playingGameFlag=0;unchanged();g_playingGameFlag=1;
 g_playerEntity.animationId=8;unchanged();g_playerEntity.animationId=1;
 g_playerEntity.animFrameId=3;unchanged();g_playerEntity.animFrameId=0;
 g_playerEntity.isBeingAttackedFlag=1;unchanged();g_playerEntity.isBeingAttackedFlag=0;
 g_playerEntity.health=0;unchanged();g_playerEntity.health=100;
 g_playerEntity.action_behavior=0x11;unchanged();g_playerEntity.action_behavior=0x0d;
 assert(re1_web_quick_turn()==1);
 g_menu_choice_id=0x80;unchanged();assert(WebPresentationMode()==0);g_menu_choice_id=0;
 g_bMCINotifyEnabled=1;assert(WebPresentationMode()==0);g_bMCINotifyEnabled=0;
 g_TasksEIP[0]=nullptr;unchanged();assert(WebPresentationMode()==0);g_TasksEIP[0]=(void*)game_start;
 // Mansion intro: actor updates/idle animation between lines do not establish
 // player control. Every shot and subtitle must retain the same complete frame.
 g_playerEntity.animationId=1;g_playerEntity.action_behavior=0;
 for(unsigned flags:{0u,1u,0x41u,0x101u,0x141u,0xffffu}){
   g_message_flags=flags;g_main_state_flags=MSF_CAMERA_LOCK;
   for(unsigned message:{0u,0x80u}){
     g_menu_choice_id=message;assert(WebPresentationMode()==0);
   }
 }
 g_menu_choice_id=0;g_main_state_flags=0;
 // 0xFEFF is the mansion intro's live mask, including its unlocked final shot.
 for(unsigned flags:{1u,0x41u,0xfeffu}){g_message_flags=flags;assert(WebPresentationMode()==0);}
 g_message_flags=0xffff;
 for(unsigned animation:{0u,4u,5u,6u,7u,8u,9u}){
   g_playerEntity.animationId=animation;assert(WebPresentationMode()==0);
 }
 g_playerEntity.animationId=1;assert(WebPresentationMode()==1);
 // A hit or aim must not resize the scene while input is otherwise enabled.
 g_playerEntity.animationId=2;g_message_flags=0x101;assert(WebPresentationMode()==1);
 g_playerEntity.animationId=1;g_playerEntity.animFrameId=3;assert(WebPresentationMode()==1);
 // Player-controlled traversal keeps its crop while locking movement/camera.
 // Scripted state 8, death, menus and movies keep their earlier policy.
 for(unsigned behavior:{0x0bu,0x11u}){
   g_playerEntity.action_behavior=behavior;g_playerEntity.animFrameId=1;
   g_playerEntity.isBeingAttackedFlag=0x80;g_message_flags=0xfeff;
   for(unsigned flags:{0u,MSF_CAMERA_LOCK}){
     g_main_state_flags=flags;assert(WebTraversalActive()&&WebPresentationMode()==1);unchanged();
   }
   g_playerEntity.animationId=8;assert(WebPresentationMode()==0);g_playerEntity.animationId=1;
 }
 g_playerEntity.action_behavior=0;assert(WebPresentationMode()==0);
 g_main_state_flags=0;g_message_flags=0xffff;g_playerEntity.isBeingAttackedFlag=0;
 // A zombie's bite uses animation window 5 with the low attacked bit set.
 // Preserve widescreen through the grab/release even with directional input
 // disabled; a scripted animation window without a combat flag stays full.
 assert(!WebTouchMenuActive());g_playerEntity.health=100;
 for(unsigned animation:{2u,5u,6u,7u}){
   g_playerEntity.animationId=animation;g_playerEntity.isBeingAttackedFlag=1;
   for(unsigned flags:{0xffffu,0xffbfu,0xfeffu,0xfebfu}){
     g_message_flags=flags;assert(WebPresentationMode()==1);assert(!WebTouchMenuActive());unchanged();
   }
 }
 g_playerEntity.animationId=5;g_playerEntity.isBeingAttackedFlag=0;assert(WebPresentationMode()==0);
 g_playerEntity.isBeingAttackedFlag=0x80;assert(WebPresentationMode()==0);
 g_playerEntity.isBeingAttackedFlag=1;g_playerEntity.health=-1;assert(WebPresentationMode()==0);
 g_playerEntity.health=100;g_main_state_flags=MSF_CAMERA_LOCK;assert(WebPresentationMode()==0);
 g_main_state_flags=0;g_menu_choice_id=0x80;assert(WebPresentationMode()==0);g_menu_choice_id=0;
 g_TasksTable[1].state=1;g_TasksEIP[1]=(void*)main_menu;assert(WebTouchMenuActive());assert(WebPresentationMode()==5);
 g_TasksTable[1].state=0;assert(!WebTouchMenuActive());
 g_openMenuFlag=1;assert(WebTouchMenuActive());g_openMenuFlag=0;
 g_TasksEIP[0]=nullptr;assert(WebTouchMenuActive());g_TasksEIP[0]=(void*)game_start;
 g_playerEntity.isBeingAttackedFlag=0;g_playerEntity.animationId=1;g_message_flags=0xffff;assert(WebPresentationMode()==1);
 g_main_state_flags=MSF_PLAYER_DEAD;assert(WebPresentationMode()==0);
 g_main_state_flags=MSF_DOOR_TRANSITION;assert(WebPresentationMode()==1);assert(!WebTouchMenuActive());
 g_openMenuFlag=1;assert(WebPresentationMode()==1);assert(!WebTouchMenuActive());
 g_main_state_flags=MSF_ROOM_TRANSITION;assert(WebPresentationMode()==2);assert(!WebTouchMenuActive());
 g_main_state_flags=0;g_roomTransitionBusy=1;assert(WebPresentationMode()==2);g_roomTransitionBusy=0;
 assert(WebPresentationMode()==4);g_openMenuFlag=0;
 // World item/locked-door messages remember that player input was enabled
 // before pause_game masked it; scripted subtitles retain the full picture.
 g_menu_choice_id=0x80;g_messageFlagsBackup=0xffff;g_message_flags=0xff00;
 assert(WebPresentationMode()==3);assert(WebTouchMenuActive());
 g_messageFlagsBackup=0xfeff;assert(WebPresentationMode()==0);assert(!WebTouchMenuActive());
 g_menu_choice_id=0;g_messageFlagsBackup=0;
 g_TasksEIP[1]=(void*)main_menu;g_TasksTable[1].state=1;
 for(unsigned flag:{MSF_MENU_MODE_ITEM_VIEW,MSF_MENU_MODE_GOT_ITEM,MSF_MENU_MODE_KEY_DEPLETED}){
   g_main_state_flags=MSF_MENU_ACTIVE|flag;assert(WebPresentationMode()==5);
 }
 for(unsigned flag:{MSF_MENU_ACTIVE,MSF_MENU_MODE_ITEMBOX,MSF_PICKUP_SCREEN}){
   g_main_state_flags=flag;assert(WebPresentationMode()==5);
 }
 inventoryLayout=false;assert(WebPresentationMode()==0);inventoryLayout=true;
 itemBoxLayout=true;assert(WebPresentationMode()==6);itemBoxLayout=false;
 for(int layout:{1,2}){mapFilesLayout=layout;assert(WebPresentationMode()==7);}
 mapFilesLayout=0;
 g_TasksTable[1].state=0;g_main_state_flags=0;g_playerEntity.animFrameId=0;
 for(unsigned state:{1u,2u,3u,4u}){
   g_itembox_state=state;g_message_flags=0xffba;g_main_state_flags=MSF_CAMERA_LOCK;
   assert(WebPresentationMode()==3);assert(re1_web_interpolation_scene()==0);
   g_bMCINotifyEnabled=1;assert(WebPresentationMode()==0);g_bMCINotifyEnabled=0;
   g_typewriter_state=3;g_loadSaveStateFlag=1;assert(WebPresentationMode()==0);
   g_typewriter_state=0;g_loadSaveStateFlag=0;
 }
 g_itembox_state=0;g_main_state_flags=0;g_message_flags=0xffff;
 // Identity camera: torso projects above the feet; invalid depth centers.
 g_RoomCameraData.m[0][0]=g_RoomCameraData.m[1][1]=g_RoomCameraData.m[2][2]=4096;
 g_playerEntity.scaMatrixData.localMatrix.t[2]=4000;
 assert(WebPlayerScreenY()>81.5f&&WebPlayerScreenY()<81.7f);
 g_playerEntity.scaMatrixData.localMatrix.t[1]=800;assert(WebPlayerScreenY()==120);
 g_playerEntity.scaMatrixData.localMatrix.t[2]=-4000;assert(WebPlayerScreenY()==120);
 // Cheats require consent, safe live gameplay and an explicit user action.
 g_main_state_flags=0;g_menu_choice_id=0;g_message_flags=0xffff;
 g_playerEntity.health=50;g_playerEntity.maxHealth=140;
 g_playerEntity.animationId=1;g_playerEntity.animFrameId=0;
 assert(re1_web_cheat(2,1)==0);assert(re1_web_cheat(9,1)==0);assert(g_playerEntity.health==50);
 assert(re1_web_cheat(1,0)==1);
 g_bWindowFocused=FALSE; // settings pauses gameplay, but actions can be queued
 g_ItemSlotsPointer=g_ItemsSlots;
 g_ItemsSlots[0]={ITEM_BERETTA,1};g_ItemsSlots[1]={ITEM_SWORD_KEY,1};
 g_ItemsSlots[2]={ITEM_INK_RIBBONS,2};g_ItemsSlots[3]={ITEM_FLAMETHROWER,0};
 g_ItemsSlots[4]={ITEM_MINIMI,1};g_ItemsSlots[5]={ITEM_ROCKET_LAUNCHER,0x81};
 g_ItemMaxQty[ITEM_BERETTA*4]=15;g_ItemMaxQty[ITEM_FLAMETHROWER*4]=240;
 g_ItemMaxQty[ITEM_ROCKET_LAUNCHER*4]=4;
 assert(re1_web_cheat(2,1)==1);assert(g_playerEntity.health==140);
 assert(re1_web_cheat(3,1)==1);assert(g_ItemsSlots[0].qty==15);
 assert(g_ItemsSlots[1].qty==1&&g_ItemsSlots[2].qty==2&&g_ItemsSlots[3].qty==240);
 assert(g_ItemsSlots[4].qty==1&&g_ItemsSlots[5].qty==0x84);
 g_playerEntity.health=-3;g_ItemsSlots[0].qty=0;PortCheatsMaintain();
 assert(g_playerEntity.health==140&&g_ItemsSlots[0].qty==15);
 assert(re1_web_cheat(6,0)==1);PortCheatsProcessRequests();assert(g_typewriter_state==0);
 g_bWindowFocused=TRUE;PortCheatsProcessRequests();
 assert(g_typewriter_state==2&&g_typewriter_id==~0u&&g_menu_choice_id==0);
 assert((g_message_flags&0x45)==0); // normal typewriter flow resumes next
 g_typewriter_state=3;g_loadSaveStateFlag=1;
 assert(WebTouchMenuActive()&&WebPresentationMode()==0);
 g_loadSaveStateFlag=0;
 g_typewriter_state=0;g_message_flags=0xffff;
 assert(re1_web_cheat(7,0)==1);PortCheatsProcessRequests();assert(g_debugOpenItemboxFlag==1);
 g_debugOpenItemboxFlag=0;
 for(unsigned flag:{MSF_CAMERA_LOCK,MSF_PLAYER_DEAD,MSF_FMV_REQUEST,MSF_DOOR_TRANSITION,MSF_ROOM_TRANSITION}){
   g_main_state_flags=flag;assert(re1_web_cheat(6,0)==0);assert(re1_web_cheat(4,0)==0);
 }
 g_main_state_flags=0;g_TasksTable[1].state=1;g_TasksEIP[1]=(void*)main_menu;
 assert(re1_web_cheat(6,0)==0);g_TasksTable[1].state=0;
 assert(re1_web_cheat(6,0)==1);g_main_state_flags=MSF_CAMERA_LOCK;
 PortCheatsProcessRequests();assert(g_typewriter_state==0); // queued state recheck
 assert(re1_web_cheat(2,0)==1&&re1_web_cheat(3,0)==1); // always allow off
 g_main_state_flags=0;g_playerEntity.health=50;g_ItemsSlots[0].qty=1;
 PortCheatsMaintain();assert(g_playerEntity.health==50&&g_ItemsSlots[0].qty==1);
 g_playerEntity.health=-1;assert(re1_web_cheat(2,1)==0);
 assert(re1_web_cheat(8,0)==1);assert((re1_web_cheat(0,0)&3)==0);
 g_playerEntity.health=100;g_message_flags=0xffff;
 re1_web_set_smooth(1);assert(re1_web_interpolation_scene()!=0);
 g_playerEntity.action_behavior=0x0b;g_playerEntity.isBeingAttackedFlag=0x80;g_playerEntity.animFrameId=1;
 assert(WebPresentationMode()==1&&re1_web_interpolation_scene()==0);
 g_playerEntity.action_behavior=0;g_playerEntity.isBeingAttackedFlag=0;g_playerEntity.animFrameId=0;
 testTime=1000;assert(re1_web_interpolation_wait_ms()==16);
 testTime=1010;assert(re1_web_interpolation_wait_ms()==0);
 g_main_state_flags=MSF_ROOM_TRANSITION;assert(re1_web_interpolation_scene()==0);
 g_main_state_flags=0;g_bWindowFocused=FALSE;assert(re1_web_interpolation_scene()==0);
 g_bWindowFocused=TRUE;re1_web_set_smooth(0);assert(re1_web_interpolation_scene()==0);

 // Ignore player is reversible session state and never edits actor data.
 g_playerEntity.health=100;g_playerEntity.animationId=1;g_playerEntity.animFrameId=0;
 g_main_state_flags=0;g_message_flags=0x141;g_TasksTable[1].state=0;
 g_playerEntity.isBeingAttackedFlag=0;re1_web_cheat(8,0);
 Entity enemy={};enemy.status_flags=1;enemy.id=0;enemy.state=1;enemy.health=100;
 assert(!PortCheatsFreezeEnemy(&enemy,true));assert(re1_web_cheat(9,1)==1);
 assert((re1_web_cheat(0,0)&4)!=0);Entity original=enemy;
 assert(PortCheatsFreezeEnemy(&enemy,true));assert(!PortCheatsFreezeEnemy(&enemy,false));
 assert(!PortCheatsFreezeEnemy(nullptr,true));assert(memcmp(&enemy,&original,sizeof(enemy))==0);
 for(unsigned state:{0u,8u}){enemy.state=state;assert(!PortCheatsFreezeEnemy(&enemy,true));}enemy.state=1;
 for(unsigned flag:{0x40u,0x80u}){enemy.behavior_flags=flag;assert(!PortCheatsFreezeEnemy(&enemy,true));}enemy.behavior_flags=0;
 enemy.hit_state=0x80;assert(!PortCheatsFreezeEnemy(&enemy,true));enemy.hit_state=0;
 enemy.health=0;assert(!PortCheatsFreezeEnemy(&enemy,true));enemy.health=100;
 g_main_state_flags=MSF_CAMERA_LOCK;assert(!PortCheatsFreezeEnemy(&enemy,true));g_main_state_flags=0;
 g_message_flags=0xfeff;assert(!PortCheatsFreezeEnemy(&enemy,true));g_message_flags=0x141;
 g_playerEntity.isBeingAttackedFlag=1;assert(!PortCheatsFreezeEnemy(&enemy,true));g_playerEntity.isBeingAttackedFlag=0;
 assert(re1_web_cheat(8,0)==1);assert(!PortCheatsFreezeEnemy(&enemy,true));
 assert(re1_web_cheat(9,1)==1);g_main_state_flags=MSF_CAMERA_LOCK;
 assert(re1_web_cheat(9,0)==1);assert((re1_web_cheat(0,0)&4)==0);
 g_main_state_flags=0;
 puts("PASS: quick turn, presentation, cheats, enemy-freeze guards, queued actions and interpolation");
}
