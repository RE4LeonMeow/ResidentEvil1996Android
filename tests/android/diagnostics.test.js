'use strict';
const {test}=require('node:test');
const assert=require('node:assert/strict');
const {AimTiming}=require('../../android/app/src/main/assets/web/diagnostics.js');

test('a slow aiming window survives release, Settings and subsequent normal frames',()=>{
  const t=new AimTiming();let now=100;
  for(let i=0;i<12;i++){now+=80;t.frame(now,55,true,0x10701,2);}
  assert.equal(t.best[1].gap,80);assert.equal(t.best[1].tick,55);
  t.breakWindow();
  for(let i=0;i<100;i++){now+=34;t.frame(now,4,false,0x10701,2);}
  assert.equal(t.best[1].gap,80);assert.equal(t.best[1].camera,0x10701);
});

test('empty-room aiming has its own sample and does not overwrite combat',()=>{
  const t=new AimTiming();let now=100;
  for(const [gap,enemies] of [[80,2],[34,0]])for(let i=0;i<12;i++){now+=gap;t.frame(now,5,true,0x10701,enemies);}
  assert.equal(t.best[1].gap,80);assert.equal(t.best[0].gap,34);
  t.reset();assert.deepEqual(t.best,[null,null]);
});

test('pauses and camera changes are excluded, and a short tap is not a sustained dip',()=>{
  const t=new AimTiming();let now=100;
  for(let i=0;i<6;i++){now+=34;t.frame(now,4,true,1,2);}
  now+=20000;t.frame(now,4,true,2,2);
  for(let i=0;i<8;i++){now+=34;t.frame(now,4,true,2,2);}
  assert.equal(t.best[1].gap,34);assert.equal(t.best[1].peak,34);
  t.breakWindow();now+=60000;t.frame(now,4,true,2,2);
  assert.equal(t.best[1].gap,34);
});

test('interpolated frames do not inflate the real aiming FPS',()=>{
  const t=new AimTiming();let now=100;
  for(let i=0;i<12;i++){
    now+=16;t.frame(now,3,true,1,2,true);
    now+=18;t.frame(now,5,true,1,2,false);
  }
  assert.equal(t.best[1].gap,34);assert.equal(t.best[1].tick,5);
});
