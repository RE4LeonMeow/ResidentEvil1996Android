package org.re1.launcher;

public final class GamepadStateTest {
    private static void check(boolean condition,String description) {
        if(!condition)throw new AssertionError(description);
    }
    public static void main(String[] args) {
        GamepadState state=new GamepadState();
        check(!state.snapshot().connected,"Cleanup must not invent a connected pad");
        for(int button=0;button<16;button++) {
            state.key(1,button,true);
            check(state.snapshot().buttons==(1<<button),"Standard button "+button);
            state.key(1,button,true);
            check(state.snapshot().buttons==(1<<button),"Repeated down changes state");
            state.key(1,button,false);
            check(state.snapshot().buttons==0,"Release "+button);
        }
        state.motion(1,-.8f,.6f,-1,-1,1,1);
        GamepadState.Snapshot pad=state.snapshot();
        check(pad.x==-.8f&&pad.y==.6f,"Left stick axes lost");
        check(pad.buttons==((1<<14)|(1<<12)|(1<<6)|(1<<7)),"Hat/trigger mapping");
        state.key(1,12,true);state.key(1,0,true);state.key(1,12,false);
        check((state.snapshot().buttons&(1<<12))!=0,"Key release incorrectly released held hat");
        // Controllers may expose axes and buttons as distinct Android devices.
        state.key(2,2,true);state.motion(2,0,0,0,0,0,0);
        pad=state.snapshot();
        check(pad.x==-.8f&&pad.y==.6f,"Neutral HID interface erased other stick");
        check((pad.buttons&5)==5,"Separate interfaces lost each other's buttons");
        state.motion(1,0,0,0,0,0,0);
        check(state.snapshot().buttons==5,"Motion release erased held face buttons");
        state.key(1,2,true);state.remove(2);
        check(state.snapshot().buttons==5,"Disconnect released another device's same button");
        check(!state.remove(99),"Unknown removal claimed a pad");
        state.remove(1);check(!state.snapshot().connected,"Last disconnect still connected");
        state.motion(3,Float.NaN,Float.POSITIVE_INFINITY,0,0,0,0);
        check(state.snapshot().x==0&&state.snapshot().y==0,"Nonfinite axis leaked into JavaScript");
        state.motion(3,-3,2,0,0,0,0);
        check(state.snapshot().x==-1&&state.snapshot().y==1,"Unbounded stick axes");
        state.clear();check(!state.snapshot().connected&&state.snapshot().buttons==0,"Focus/pause cleanup");
        state.key(4,9,true);check(state.snapshot().buttons==(1<<9),"Reconnect did not recover");
        System.out.println("PASS: native buttons/axes/triggers, composite interfaces, independent release/removal, focus cleanup and reconnect");
    }
}
