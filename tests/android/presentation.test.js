'use strict';
const {test}=require('node:test');
const assert=require('node:assert/strict');
const {Pan,viewport,imageRect,menuExtra}=require('../../android/app/src/main/assets/web/presentation.js');
test('wide inventory has extra horizontal space without dropping source rows',()=>{
 assert(Math.abs(menuExtra(viewport(1920,1080,true))-320/3)<.001);
 assert.equal(menuExtra(viewport(640,480,true)),0);
 assert.equal(menuExtra(viewport(600,1000,true)),0);
});
test('16:9 uses uniform scaling and exposes 180 of the original 240 source rows',()=>{
 const v=viewport(1920,1080,true),r=imageRect(v,true,30);
 assert.equal(v.visible,180);assert.deepEqual(r,{x:0,y:-180,width:1920,height:1440});
 assert.equal(r.width/320,r.height/240);
});
test('menus retain the complete 4:3 image within a wide viewport',()=>{
 const v=viewport(1920,1080,true),r=imageRect(v,false);
 assert.deepEqual(r,{x:240,y:0,width:1440,height:1080});
 assert.equal(r.width/r.height,4/3);
 const original=viewport(2400,1080,false);assert.equal(original.width,1440);assert.equal(original.visible,240);
});
test('wide phones keep a 16:9 limit; narrow windows preserve 4:3 without stretching',()=>{
 const wide=viewport(2400,1080,true);assert.equal(wide.x,240);assert.equal(wide.width,1920);
 const narrow=viewport(600,1000,true);assert.equal(narrow.width,600);assert.equal(narrow.height,450);assert.equal(narrow.visible,240);
});
test('tracking clamps to image edges, snaps at camera cuts, and holds an eight pixel safe band',()=>{
 const p=new Pan();assert.equal(p.update(-100,1,.016),0);assert.equal(p.update(1000,2,.016),60);
 p.center(3);assert.equal(p.update(125,3,.016),30);
 const moved=p.update(180,3,.016);assert(moved>30&&moved<60);
 assert.equal(p.update(90,4,.016),0);assert.equal(p.center(4),30);
 assert.equal(imageRect(viewport(1920,1080,true),true,1000).y,-360);
});
test('tracking stays finite and bounds long frame gaps',()=>{
 const a=new Pan(),b=new Pan();a.center(1);b.center(1);
 assert.equal(a.update(180,1,100),b.update(180,1,.1));
 assert(Number.isFinite(a.update(NaN,1,.016)));
 assert.equal(a.update(120,2,.016,240),0);
});
test('world messages hold the existing framing and reflow text instead of panning down',()=>{
 const p=new Pan();p.update(105,7,0);assert.equal(p.position,15);
 assert.equal(p.freeze(180,7),15);assert.equal(p.freeze(-100,7),15);
 const shift=60-p.position,r=imageRect(viewport(1920,1080,true),true,p.position);
 for(const y of [181,197,213,225]){
   const screen=(y-shift)*6+r.y;assert(screen>=0&&screen+14*6<=1080);
 }
 assert.equal(p.freeze(130,8),40); // new camera gets its own correct crop
 assert.equal(p.freeze(NaN,9),30);
 assert.equal(p.freeze(180,9,240),0); // 4:3 has no crop or translation
});
test('interaction crop retains bottom text, Yes/No choices and blink cursor; doors center independently of the player',()=>{
 const p=new Pan(),v=viewport(1920,1080,true),r=imageRect(v,true,p.bottom(7,v.visible));
 for(const y of [181,186,202,216,230,239])assert(y*(r.height/240)+r.y>=0&&y*(r.height/240)+r.y<v.height);
 assert.equal(r.width/320,r.height/240);
 const door=imageRect(v,true,p.center(9,v.visible));assert.equal(door.y,-180);
 assert.equal(p.bottom(10,240),0);
});
