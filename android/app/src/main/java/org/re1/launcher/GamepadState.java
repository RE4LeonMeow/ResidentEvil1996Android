package org.re1.launcher;

import java.util.LinkedHashMap;
import java.util.Map;

/** Native packets in Web Gamepad's standard button order, before user remapping.
 * Separate HID interfaces/devices must not release each other's held buttons.
 * Android's event dispatch lives in MainActivity; this state is JVM-testable.
 */
final class GamepadState {
    private static final class Pad {
        int keys, axes;
        float x, y;
    }
    static final class Snapshot {
        final int buttons;
        final float x, y;
        final boolean connected;
        Snapshot(int buttons,float x,float y,boolean connected) {
            this.buttons=buttons;this.x=x;this.y=y;this.connected=connected;
        }
    }
    private final Map<Integer,Pad> pads=new LinkedHashMap<>();
    boolean contains(int device) {return pads.containsKey(device);}
    private Pad pad(int device) {return pads.computeIfAbsent(device,id->new Pad());}
    boolean key(int device,int button,boolean down) {
        if(button<0||button>=16)throw new IllegalArgumentException("Invalid controller button");
        boolean added=!contains(device);
        Pad pad=pad(device);int before=pad.keys;
        if(down)pad.keys|=1<<button;else pad.keys&=~(1<<button);
        return added||before!=pad.keys;
    }
    private static float axis(float value) {
        return Float.isFinite(value)?Math.max(-1,Math.min(1,value)):0;
    }
    boolean motion(int device,float x,float y,float hatX,float hatY,float leftTrigger,float rightTrigger) {
        boolean added=!contains(device);Pad pad=pad(device);
        x=axis(x);y=axis(y);int buttons=0;
        if(hatX<-.5f)buttons|=1<<14;if(hatX>.5f)buttons|=1<<15;
        if(hatY<-.5f)buttons|=1<<12;if(hatY>.5f)buttons|=1<<13;
        if(leftTrigger>.5f)buttons|=1<<6;if(rightTrigger>.5f)buttons|=1<<7;
        boolean changed=added||pad.x!=x||pad.y!=y||pad.axes!=buttons;
        pad.x=x;pad.y=y;pad.axes=buttons;return changed;
    }
    boolean remove(int device) {return pads.remove(device)!=null;}
    void clear() {pads.clear();}
    Snapshot snapshot() {
        int buttons=0;float x=0,y=0;
        for(Pad pad:pads.values()) {
            buttons|=pad.keys|pad.axes;
            // A separate neutral HID interface must not zero the moving stick.
            if(pad.x*pad.x+pad.y*pad.y>x*x+y*y){x=pad.x;y=pad.y;}
        }
        return new Snapshot(buttons,x,y,!pads.isEmpty());
    }
}
