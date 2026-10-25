#include "platform/web/frame_pacing.h"
#include "platform/web/frame_pacing.inl"
#include <cassert>
#include <cstdio>
#include <initializer_list>

static int Now(){return EM_ASM_INT({return Math.floor(clockQA.now);});}

int main()
{
    // Retain the original deadline, overrun, pause and clock-wrap coverage.
    assert(WebFrameWaitMs(1000,1000,true,true,false)==33);
    assert(WebFrameWaitMs(1010,1000,true,true,false)==23);
    assert(WebFrameWaitMs(1032,1000,true,true,false)==1);
    assert(WebFrameWaitMs(1033,1000,true,true,false)==1);
    assert(WebFrameWaitMs(1050,1000,true,true,false)==1);
    assert(WebFrameWaitMs(1010,1000,true,false,false)==6);
    assert(WebFrameWaitMs(1016,1000,true,false,false)==1);
    assert(WebFrameWaitMs(1000,1000,true,true,true)==1);
    assert(WebFrameWaitMs(900,1000,true,true,false)==1);
    assert(WebFrameWaitMs(0,0xfffffff0u,true,true,false)==17);
    assert(WebFrameWaitMs(1000,1000,false,true,false)==100);
    EM_ASM({
        globalThis.clockQA=({now:0,period:1000/60,throttled:false,next:0,tasks:new Map(),raf:0,timers:0,wakes:0,maxPending:0});
        const flush=()=>{
            let first=null;
            for(const task of clockQA.tasks.values())if(!first||task.at<first.at)first=task;
            if(!first)return;
            clockQA.tasks.delete(first.id);clockQA.now=first.at;
            clockQA[first.type]++;clockQA.wakes++;first.callback(clockQA.now);
        };
        const schedule=(type,at,callback)=>{
            const id=++clockQA.next;
            clockQA.tasks.set(id,({id,type,at,callback}));
            clockQA.maxPending=Math.max(clockQA.maxPending,clockQA.tasks.size);
            queueMicrotask(flush);return id;
        };
        globalThis.requestAnimationFrame=callback=>schedule('raf',(Math.floor((clockQA.now+0.000001)/clockQA.period)+1)*clockQA.period,callback);
        globalThis.cancelAnimationFrame=id=>clockQA.tasks.delete(id);
        globalThis.setTimeout=(callback,delay)=>schedule('timers',clockQA.now+(clockQA.throttled?Math.max(delay,100):delay),callback);
        globalThis.clearTimeout=id=>clockQA.tasks.delete(id);
    });
    WebWaitForFrame(33,1); // Display callback wins, even with no touch events.
    assert(EM_ASM_INT({return clockQA.raf;})==1);
    assert(EM_ASM_INT({return clockQA.timers;})==0);
    assert(EM_ASM_INT({return clockQA.tasks.size;})==0);
    const int before=Now();
    WebWaitForFrame(3,1); // A near deadline must not wait another full refresh.
    assert(Now()-before==3);
    assert(EM_ASM_INT({return clockQA.timers;})==1);
    assert(EM_ASM_INT({return clockQA.tasks.size;})==0);
    EM_ASM({clockQA.throttled=true;});
    const int raf=EM_ASM_INT({return clockQA.raf;});
    WebWaitForFrame(1,1); // A delayed timer cannot block the display callback.
    assert(EM_ASM_INT({return clockQA.raf;})==raf+1);
    assert(EM_ASM_INT({return clockQA.tasks.size;})==0);
    EM_ASM({clockQA.throttled=false;});
    const int pausedAt=Now();
    WebWaitForFrame(100,0);
    assert(Now()-pausedAt==100);
    assert(EM_ASM_INT({return clockQA.raf;})==raf+1);
    WebWaitForFrame(33,1);
    assert(EM_ASM_INT({return clockQA.raf;})==raf+2);
    // Exercise the original 33 ms budget on several display rates with 5 ms
    // of render work, so early RAFs cannot introduce 50 ms pacing gaps.
    for(const int hz:{60,90,120,144}){
        EM_ASM({clockQA.period=1000/$0;clockQA.now=0;},hz);
        unsigned previous=0;
        for(int tick=1;tick<=30;++tick){
            EM_ASM({clockQA.now+=5;});
            int waits=0;
            do{assert(++waits<20);WebWaitForFrame(WebFrameWaitMs(Now(),previous,true,true,false),1);}
            while(unsigned(Now())-previous<33);
            assert(unsigned(Now())-previous==33);
            previous=Now();
        }
        assert(previous==990);
        assert(EM_ASM_INT({return clockQA.tasks.size;})==0);
    }
    assert(EM_ASM_INT({return clockQA.maxPending;})==2);
    puts("PASS: actual WASM waits, RAF/deadline race and cancellation, throttled timers, paused/resume, steady original tick budget at 60/90/120/144 Hz");
}
