// Port-added experimental cheats. No original addresses or packed fields added.
// Desktop builds keep their existing behavior.
#pragma once
#ifdef __EMSCRIPTEN__
void PortCheatsMaintain();
void PortCheatsProcessRequests();
#else
inline void PortCheatsMaintain() {}
inline void PortCheatsProcessRequests() {}
#endif
