// Port-owned translation hooks. No saved-game fields or desktop behavior change.
#pragma once
#ifdef __EMSCRIPTEN__
const unsigned char* PortLocalizationText(unsigned kind,unsigned index,const unsigned char* fallback);
unsigned char* PortLocalizationRoom(unsigned index,unsigned char* fallback);
bool PortLocalizationYesNo(short x,short y,unsigned char colour);
bool PortLocalizationActive();
const unsigned char* PortLocalizationSaveYesNo(const unsigned char* fallback);
#else
inline const unsigned char* PortLocalizationText(unsigned,unsigned,const unsigned char* fallback){return fallback;}
inline unsigned char* PortLocalizationRoom(unsigned,unsigned char* fallback){return fallback;}
inline bool PortLocalizationYesNo(short,short,unsigned char){return false;}
inline bool PortLocalizationActive(){return false;}
inline const unsigned char* PortLocalizationSaveYesNo(const unsigned char* fallback){return fallback;}
#endif
