// Actual platform backend: browser shortcuts must have one owner. Native
// keyboard input retains its held/edge contract when the browser bridge is off.
#include "platform/platform.h"
#include <SDL2/SDL.h>
#include <cassert>
#include <cstdlib>
#include <cstdio>
int main(){
    setenv("RE1_BROWSER_INPUT","0",1);
    plat_key_event(SDL_SCANCODE_LEFT,TRUE);
    assert(plat_key_state(VK_LEFT)==0x8001);
    assert(plat_key_state(VK_LEFT)==0x8000);
    plat_key_event(SDL_SCANCODE_LEFT,FALSE);assert(plat_key_state(VK_LEFT)==0);
    setenv("RE1_BROWSER_INPUT","1",1);
    plat_key_event(SDL_SCANCODE_LEFT,TRUE); // keydown while a launcher button owns focus
    assert(plat_key_state(VK_LEFT)==0); // keyup may be intercepted by app.js
    plat_key_event(SDL_SCANCODE_RIGHT,TRUE);assert(plat_key_state(VK_RIGHT)==0);
    setenv("RE1_BROWSER_INPUT","0",1);
    plat_key_event(SDL_SCANCODE_RIGHT,TRUE);assert(plat_key_state(VK_RIGHT)==0x8001);
    plat_key_event(SDL_SCANCODE_RIGHT,FALSE);assert(plat_key_state(VK_RIGHT)==0);
    puts("PASS: browser SDL keys cannot latch; native keyboard held/edge input remains available");
}
