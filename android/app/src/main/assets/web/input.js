/* Android/Web port addition. Pure input model; also exercised under node. */
(function (root) {
  'use strict';
  const bits = {up:1, down:2, left:4, right:8, action:256, run:512,
    aim:1024, inventory:32768, options:16384};
  const defaults = {opacity:0.55, scale:1, deadzone:0.30, hidden:false, movement:'arrows', menuArrows:true, showFPS:false,
    positions:{}, sizes:{}, toggles:{}, mapping:[256,512,1024,2048,4096,8192,262144,524288,16384,32768,65536,131072,1,2,4,8]};
  class Input {
    constructor(settings = {}) {
      this.settings = {...defaults, ...settings};
      this.settings.mapping = Array.isArray(settings.mapping) && settings.mapping.length === 16
        ? settings.mapping.map(x => Number.isInteger(x) && x >= 0 && x <= 0xfffff ? x : 0)
        : [...defaults.mapping];
      this.settings.deadzone = Math.max(0.1, Math.min(0.9, Number(this.settings.deadzone) || 0.3));
      this.settings.movement = this.settings.movement === 'joystick' ? 'joystick' : 'arrows';
      this.settings.menuArrows = this.settings.menuArrows !== false;
      this.settings.sizes = {};
      for (const name of [...Object.keys(bits),'quickturn','joystick']) {
        const value=settings.sizes?.[name];
        if (typeof value==='number' && Number.isFinite(value))this.settings.sizes[name]=Math.max(.5,Math.min(2,value));
      }
      this.settings.toggles = {};
      for(const action of Object.keys(bits))if(settings.toggles?.[action]===true)this.settings.toggles[action]=true;
      this.pointers = new Map(); this.native = 0; this.paused = false; this.pending = 0; this.latched = 0;
    }
    movementStyle(menu) {
      return this.settings.movement==='joystick' && !(menu && this.settings.menuArrows) ? 'joystick' : 'arrows';
    }
    press(id, action) {
      if (bits[action] && !this.paused) {this.pointers.set(id, bits[action]);this.pending |= bits[action];}
    }
    touchPress(id, action, menu=false) {
      const bit=bits[action];
      if(!bit||this.paused)return;
      if(menu||!this.settings.toggles[action]){this.press(id,action);return;}
      const opposite={up:bits.down,down:bits.up,left:bits.right,right:bits.left}[action]||0;
      const on=!(this.latched&bit);
      this.latched=on?(this.latched&~opposite)|bit:this.latched&~bit;
      if(on)this.pending=(this.pending&~opposite)|bit;
      else this.pending&=~bit; // a second tap before the next tick cancels it
    }
    isTouchLatched(action) {return !!(this.latched&bits[action]);}
    stick(id, x, y) {
      let mask=0;
      if (!this.paused && Number.isFinite(x) && Number.isFinite(y) && Math.hypot(x,y)>=0.2) {
        // Eight directions with a radial dead zone. The game retains its
        // original tank controls; diagonals combine turning and movement.
        if(Math.abs(x)>Math.abs(y)*0.41421356)mask|=x<0?bits.left:bits.right;
        if(Math.abs(y)>Math.abs(x)*0.41421356)mask|=y<0?bits.up:bits.down;
      }
      if(mask)this.pointers.set(id,mask);else this.pointers.delete(id);
    }
    release(id) { this.pointers.delete(id); }
    clear() { this.pointers.clear(); this.native = 0; this.pending = 0; this.latched = 0; }
    poll(pads) {
      if(this.paused){this.pending=0;return 0;}
      const held=this.mask(pads);let pending=this.pending;this.pending=0;
      // A short tap between two game ticks must reach one tick. Current
      // movement takes priority over an earlier, opposite direction tap.
      if(held&bits.up)pending&=~bits.down;if(held&bits.down)pending&=~bits.up;
      if(held&bits.left)pending&=~bits.right;if(held&bits.right)pending&=~bits.left;
      let mask=held|pending;
      if(mask&bits.up)mask&=~bits.down;if(mask&bits.left)mask&=~bits.right;
      return mask>>>0;
    }
    mask(pads) {
      if (this.paused) return 0;
      let mask = this.native | this.latched;
      for (const bit of this.pointers.values()) mask |= bit;
      if (pads === undefined) pads = root.navigator?.getGamepads?.() || [];
      for (const pad of pads) {
        if (!pad || !pad.connected || pad.mapping !== 'standard') continue;
        const d = this.settings.deadzone, x = pad.axes[0] || 0, y = pad.axes[1] || 0;
        if (x < -d) mask |= bits.left;
        if (x > d) mask |= bits.right;
        if (y < -d) mask |= bits.up;
        if (y > d) mask |= bits.down;
        for (let i=0; i<16; i++) if (pad.buttons[i]?.pressed || pad.buttons[i]?.value > 0.5)
          mask |= this.settings.mapping[i];
      }
      if (mask & 1) mask &= ~2;
      if (mask & 4) mask &= ~8;
      return mask >>> 0;
    }
  }
  root.RE1InputModel = {Input, bits, defaults};
  if (typeof module !== 'undefined') module.exports = root.RE1InputModel;
})(globalThis);
