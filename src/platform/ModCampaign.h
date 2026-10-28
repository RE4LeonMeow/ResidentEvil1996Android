// Port-owned, data-only campaign hooks. Disabled when no campaign is installed.
#pragma once
#include <cstddef>
#ifdef __EMSCRIPTEN__
bool PortCampaignActive();
bool PortCampaignBackgroundOverride(const char* gameRelativePath);
unsigned PortCampaignCountdownSeconds();
bool PortCampaignNewGame();
void PortCampaignCard(unsigned char* data,std::size_t size);
void PortCampaignInitPlayer();
const char* PortCampaignRoomSound(unsigned room,unsigned slot,const char* fallback);
unsigned char PortCampaignBgmGroup(unsigned index,unsigned char fallback);
const char* PortCampaignBgmName(unsigned group,unsigned channel,const char* fallback);
unsigned char PortCampaignBgmLoop(unsigned group,unsigned channel,unsigned char fallback);
#else
inline bool PortCampaignActive(){return false;}
inline bool PortCampaignBackgroundOverride(const char*){return false;}
inline unsigned PortCampaignCountdownSeconds(){return 180;}
inline bool PortCampaignNewGame(){return false;}
inline void PortCampaignCard(unsigned char*,std::size_t){}
inline void PortCampaignInitPlayer(){}
inline const char* PortCampaignRoomSound(unsigned,unsigned,const char* fallback){return fallback;}
inline unsigned char PortCampaignBgmGroup(unsigned,unsigned char fallback){return fallback;}
inline const char* PortCampaignBgmName(unsigned,unsigned,const char* fallback){return fallback;}
inline unsigned char PortCampaignBgmLoop(unsigned,unsigned,unsigned char fallback){return fallback;}
#endif
