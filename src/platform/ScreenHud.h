// Port-owned placement for HUD text when the web viewport crops the frame.
#pragma once
#ifdef __EMSCRIPTEN__
short PortCountdownY(short original);
#else
inline short PortCountdownY(short original){return original;}
#endif
