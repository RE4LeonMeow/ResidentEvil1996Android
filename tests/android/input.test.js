'use strict';
const {test}=require('node:test');
const assert=require('node:assert/strict');
const {Input,bits}=require('../../android/app/src/main/assets/web/input.js');
const pad=(axes,pressed=[])=>({connected:true,mapping:'standard',axes,buttons:Array.from({length:16},(_,i)=>({pressed:pressed.includes(i)}))});
test('controller R3 quick-turn fires once, buffers a short native tap and keeps normal input independent',()=>{
 const i=new Input();let turns=0;i.onQuickTurn=()=>turns++;
 i.updateNative((1<<11)|(1<<2),0,-1);assert.equal(i.poll(),bits.aim|bits.up);assert.equal(turns,1);
 i.poll();i.poll();assert.equal(turns,1);
 i.updateNative(0,0,0);i.poll();i.updateNative(1<<11,0,0);i.updateNative(0,0,0);i.poll();i.poll();assert.equal(turns,2);
});
test('browser quick-turn, remapping, disabling and pause/release do not repeat turns',()=>{
 const i=new Input();let turns=0;i.onQuickTurn=()=>turns++;
 i.poll([pad([0,0],[11])]);i.poll([pad([0,0],[11])]);assert.equal(turns,1);
 i.clear();i.paused=true;i.poll([pad([0,0],[11])]);i.paused=false;i.poll([pad([0,0],[11])]);assert.equal(turns,1);
 i.poll([pad([0,0])]);i.poll([pad([0,0],[11])]);assert.equal(turns,2);
 i.settings.quickTurnButton=4;i.clear();i.poll([]);assert.equal(i.poll([pad([0,0],[4,0])]),bits.action);assert.equal(turns,3);
 i.settings.quickTurnButton=-1;i.clear();assert.equal(i.poll([pad([0,0],[11])]),131072);assert.equal(turns,3);
 assert.equal(new Input(JSON.parse(JSON.stringify(i.settings))).settings.quickTurnButton,-1);
});
test('simultaneous movement, aim and action survive independent finger releases',()=>{
 const i=new Input();i.press(1,'up');i.press(2,'aim');i.press(3,'action');assert.equal(i.mask([]),1|1024|256);
 i.release(3);assert.equal(i.mask([]),1|1024);i.release(1);assert.equal(i.mask([]),1024);i.clear();assert.equal(i.mask([]),0);
});
test('multiple fingers on same action do not release each other',()=>{const i=new Input();i.press(1,'run');i.press(2,'run');i.release(1);assert.equal(i.mask([]),bits.run);});
test('opposite directions normalize consistently',()=>{const i=new Input();for(const x of Object.keys(bits))i.press(x,x);assert.equal(i.mask([])&15,5);});
test('controller deadzone, diagonals, D-pad, triggers and start match Marni masks',()=>{
 const i=new Input();assert.equal(i.mask([pad([.1,-.1])]),0);assert.equal(i.mask([pad([-.8,.8],[0,7,9])]),4|2|256|524288|32768);
 assert.equal(i.mask([pad([0,0],[12,14])]),1|4);
});
test('controller disconnect, focus loss and pause release buttons',()=>{const i=new Input();assert.equal(i.mask([pad([0,0],[0])]),256);assert.equal(i.mask([]),0);i.press(1,'action');i.paused=true;assert.equal(i.mask([]),0);i.clear();i.paused=false;assert.equal(i.mask([]),0);});
test('controller remapping persists and rejects invalid bit values',()=>{const map=new Array(16).fill(0);map[0]=bits.aim;map[1]=-1;const i=new Input({mapping:map});assert.equal(i.mask([pad([0,0],[0,1])]),bits.aim);});
test('unknown/nonstandard controllers do not guess button layouts',()=>{const i=new Input();assert.equal(i.mask([{...pad([1,1],[0]),mapping:''}]),0);});
test('native short button and D-pad taps survive between ticks exactly once',()=>{
 const i=new Input();
 for(const [button,expected] of [[0,bits.action],[2,bits.aim],[12,bits.up],[15,bits.right]]){
  i.updateNative(1<<button,0,0);i.updateNative(0,0,0);
  assert.equal(i.poll(),expected);assert.equal(i.poll(),0);
 }
});
test('native held inputs, remapping, deadzone and opposite movement preserve game masks',()=>{
 const i=new Input({deadzone:.4,mapping:[bits.aim,...new Array(15).fill(0)]});
 i.updateNative(1,.2,-.3);assert.equal(i.poll(),bits.aim);assert.equal(i.poll(),bits.aim);
 i.updateNative(1,-.8,.8);assert.equal(i.poll(),bits.aim|bits.left|bits.down);
 i.updateNative(1,.8,-.8);assert.equal(i.poll(),bits.aim|bits.right|bits.up);
 i.updateNative(0,0,0);assert.equal(i.poll(),0);
});
test('native detach drops native buffered taps while keeping independent touch taps',()=>{
 const i=new Input();i.press(7,'run');i.release(7);i.updateNative(1,0,0);
 i.updateNative(0,0,0,false);assert.equal(i.nativePad,null);assert.equal(i.poll([]),bits.run);assert.equal(i.poll([]),0);
});
test('pause/clear do not replay native events and next packet restores input',()=>{
 const i=new Input();i.paused=true;i.updateNative(1,0,0);i.updateNative(0,0,0);assert.equal(i.poll(),0);
 i.paused=false;assert.equal(i.poll(),0);i.updateNative(1,0,0);i.clear();assert.equal(i.poll([]),0);
 i.updateNative(1,0,0);assert.equal(i.poll(),bits.action);i.updateNative(0,0,0);assert.equal(i.poll(),0);
});
test('a tap released between engine ticks is delivered once without sticking',()=>{
 const i=new Input();i.press(1,'action');i.release(1);assert.equal(i.mask([]),0);
 assert.equal(i.poll([]),bits.action);assert.equal(i.poll([]),0);
 i.press(2,'aim');i.press(3,'action');i.release(3);assert.equal(i.poll([]),bits.aim|bits.action);assert.equal(i.poll([]),bits.aim);
});
test('pause and clear discard buffered taps; current movement wins over opposite tap',()=>{
 const i=new Input();i.press(1,'action');i.release(1);i.paused=true;assert.equal(i.poll([]),0);i.paused=false;assert.equal(i.poll([]),0);
 i.press(2,'up');i.release(2);i.press(3,'down');assert.equal(i.poll([]),bits.down);i.clear();assert.equal(i.poll([]),0);
});
test('virtual joystick has radial deadzone, eight directions and independent action touches',()=>{
 const i=new Input({movement:'joystick'});
 for(const [x,y,expected] of [[0,0,0],[.1,.1,0],[0,-1,bits.up],[1,0,bits.right],[0,1,bits.down],[-1,0,bits.left],[-.8,-.8,bits.up|bits.left],[.8,.8,bits.down|bits.right],[.1,-1,bits.up]]){
  i.stick(1,x,y);assert.equal(i.mask([]),expected);
 }
 i.press(2,'aim');i.press(3,'action');i.stick(1,-1,0);assert.equal(i.poll([]),bits.left|bits.aim|bits.action);
 i.release(1);assert.equal(i.poll([]),bits.aim|bits.action);i.clear();assert.equal(i.poll([]),0);
 i.stick(1,NaN,1);assert.equal(i.poll([]),0);
});
test('menu arrows default on for saved joystick layouts and can be disabled without changing gameplay movement',()=>{
 const i=new Input({movement:'joystick'});
 assert.equal(i.movementStyle(false),'joystick');assert.equal(i.movementStyle(true),'arrows');
 i.settings.menuArrows=false;assert.equal(i.movementStyle(true),'joystick');
 const restored=new Input(JSON.parse(JSON.stringify(i.settings)));
 assert.equal(restored.movementStyle(true),'joystick');
 const arrows=new Input({movement:'arrows',menuArrows:false});assert.equal(arrows.movementStyle(false),'arrows');assert.equal(arrows.movementStyle(true),'arrows');
});
test('individual control sizes survive saving, stay independent and reject invalid values',()=>{
 const i=new Input({sizes:{action:1.5,joystick:1.3,up:.8,run:500,aim:-20,options:NaN,inventory:'2'}});
 assert.deepEqual(i.settings.sizes,{up:.8,action:1.5,run:2,aim:.5,joystick:1.3});
 const restored=new Input(JSON.parse(JSON.stringify(i.settings)));
 assert.deepEqual(restored.settings.sizes,i.settings.sizes);
 restored.settings.sizes.action=1;assert.equal(i.settings.sizes.action,1.5);
 assert.deepEqual(new Input().settings.sizes,{});
});
test('touch toggles latch until the second tap; held touch and keyboard remain independent',()=>{
 const i=new Input({toggles:{run:true,aim:true}});
 i.touchPress(1,'run');i.release(1);assert.equal(i.poll([]),bits.run);assert.equal(i.poll([]),bits.run);
 i.touchPress(2,'aim');i.touchPress(3,'action');assert.equal(i.poll([]),bits.run|bits.aim|bits.action);
 i.release(3);assert.equal(i.poll([]),bits.run|bits.aim);
 i.touchPress(4,'run');assert.equal(i.poll([]),bits.aim);assert.equal(i.isTouchLatched('run'),false);
 i.clear();i.press('keyboard','run');i.release('keyboard');i.poll([]);assert.equal(i.poll([]),0);
});
test('touch toggles cancel buffered taps, opposite directions, pause and menu transitions safely',()=>{
 const i=new Input({toggles:{up:true,down:true,action:true}});
 i.touchPress(1,'action');i.touchPress(2,'action');assert.equal(i.poll([]),0);
 i.touchPress(1,'up');i.touchPress(2,'down');assert.equal(i.poll([]),bits.down);assert.equal(i.isTouchLatched('up'),false);
 i.clear();i.touchPress(3,'action',true);i.release(3);assert.equal(i.poll([]),bits.action);assert.equal(i.poll([]),0);
 i.paused=true;i.touchPress(4,'up');i.paused=false;assert.equal(i.poll([]),0);
 i.touchPress(5,'up');i.clear();assert.equal(i.poll([]),0);assert.equal(i.latched,0);
});
test('toggle preferences persist with strict validation and never persist pressed state',()=>{
 const i=new Input({toggles:{aim:true,run:'true',up:1,action:false,unknown:true,quickturn:true}});
 assert.deepEqual(i.settings.toggles,{aim:true});i.touchPress(1,'aim');
 const next=new Input(JSON.parse(JSON.stringify(i.settings)));
 assert.deepEqual(next.settings.toggles,{aim:true});assert.equal(next.poll([]),0);
 assert.deepEqual(new Input().settings.toggles,{});
});
