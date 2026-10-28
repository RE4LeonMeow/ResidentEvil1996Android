// Port-added experimental cheats. No original addresses or packed fields added.
// Desktop builds keep their existing behavior.
#pragma once
struct Entity;
#ifdef __EMSCRIPTEN__
void PortCheatsMaintain();
void PortCheatsProcessRequests();
bool PortCheatsFreezeEnemy(const Entity* entity,bool hostile);
#else
inline void PortCheatsMaintain() {}
inline void PortCheatsProcessRequests() {}
inline bool PortCheatsFreezeEnemy(const Entity*,bool){return false;}
#endif
