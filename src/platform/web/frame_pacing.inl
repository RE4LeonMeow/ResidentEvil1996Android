// Port-owned browser scheduling. The original limiter still decides when
// a game tick is due. Keep a foreground display callback scheduled without
// touch input, while retaining the tick deadline between display refreshes.
#include <emscripten.h>

EM_ASYNC_JS(void, WebWaitForFrame, (unsigned delay, int focused), {
    await new Promise(resolve => {
        if (!focused) {setTimeout(resolve, delay);return;}
        let done=false;
        const wake=()=>{
            if(done)return;
            done=true;
            clearTimeout(timer);cancelAnimationFrame(frame);
            resolve();
        };
        const frame=requestAnimationFrame(wake);
        const timer=setTimeout(wake, delay);
    });
});
