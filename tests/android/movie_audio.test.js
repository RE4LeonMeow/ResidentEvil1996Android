'use strict';
const {test}=require('node:test');
const assert=require('node:assert/strict');
const {MovieAudio}=require('../../android/app/src/main/assets/web/movie-audio.js');
function fixture(){
 const context={currentTime:10,state:'running',destination:{},buffers:[],sources:[],copyDelay:0,
  createBuffer(channels,length,rate){this.currentTime+=this.copyDelay;const data=Array.from({length:channels},()=>new Float32Array(length));const b={length,sampleRate:rate,getChannelData:i=>data[i]};this.buffers.push(b);return b;},
  createBufferSource(){const s={stopCount:0,disconnectCount:0,connect(target){this.target=target;},start(...args){this.args=args;},stop(){this.stopCount++;},disconnect(){this.disconnectCount++;}};this.sources.push(s);return s;}};
 return {context,audio:new MovieAudio(()=>context)};
}
test('movie PCM preserves channels, signed samples and sample rate on its own source',()=>{
 const {context:c,audio:a}=fixture();
 assert(a.play(new Int16Array([-32768,32767,16384,-16384,0,0]),22050,0,0,123));
 assert.deepEqual(Array.from(c.buffers[0].getChannelData(0)),[-1,.5,0]);
 assert.deepEqual(Array.from(c.buffers[0].getChannelData(1)),[32767/32768,-.5,0]);
 assert.equal(c.buffers[0].sampleRate,22050);assert.equal(c.sources[0].target,c.destination);
 assert.deepEqual(c.sources[0].args,[10.05,0,3/22050]);assert.equal(a.position(),0);
});
test('loading stalls precede the scheduled onset and cannot consume the start of a movie',()=>{
 const {context:c,audio:a}=fixture();c.copyDelay=4;
 a.play(new Int16Array(4000),1000,0,0,123);
 assert.equal(c.sources[0].args[0],14.05);assert.equal(a.position(),0);
 c.currentTime=14.55;assert(Math.abs(a.position()-500)<=1);
});
test('the context clock freezes during pause and continues through the audio tail until video EOF',()=>{
 const {context:c,audio:a}=fixture();a.play(new Int16Array(4000),1000,0,0,123);
 c.currentTime=11.05;const paused=a.position();assert.equal(paused,1000);
 c.state='suspended';assert.equal(a.position(),paused);
 c.state='running';c.currentTime=11.3;assert(Math.abs(a.position()-1250)<=1);
 c.sources[0].onended();c.currentTime=12.55;
 assert(Math.abs(a.position()-2500)<=1);assert.equal(a.source,null);
});
test('scenario-cut seek stops the previous voice, reuses decoded PCM and resets the absolute clock',()=>{
 const {context:c,audio:a}=fixture(),pcm=new Int16Array(10000);
 a.play(pcm,1000,0,2000,123);c.currentTime=12;
 a.play(pcm,1000,3000,4500,123);
 assert.equal(c.buffers.length,1);assert.equal(c.sources[0].stopCount,1);assert.equal(c.sources[0].disconnectCount,1);
 assert.deepEqual(c.sources[1].args,[12.05,3,1.5]);assert.equal(a.position(),3000);
 c.currentTime=12.55;assert(Math.abs(a.position()-3500)<=1);
});
test('closing releases the decoded buffer so a later allocation at the same pointer cannot reuse old sound',()=>{
 const {context:c,audio:a}=fixture();a.play(new Int16Array(100),1000,0,0,123);a.stop();
 assert.equal(a.position(),-1);assert.equal(a.buffer,null);assert.equal(a.context,null);assert.equal(c.sources[0].stopCount,1);
 a.play(new Int16Array(100).fill(16384),1000,0,0,123);
 assert.equal(c.buffers.length,2);assert.equal(c.buffers[1].getChannelData(0)[0],.5);
});
test('invalid ranges, unsupported output and allocation failures leave no running movie voice',()=>{
 const {context:c,audio:a}=fixture(),pcm=new Int16Array(100);
 for(const [start,end] of [[50,0],[20,10],[NaN,50]]){assert.equal(a.play(pcm,1000,start,end,123),false);assert.equal(a.position(),-1);}
 c.state='closed';assert.equal(a.play(pcm,1000,0,0,123),false);
 c.state='running';c.createBuffer=()=>{throw Error('Allocation failed');};assert.equal(a.play(pcm,1000,0,0,123),false);assert.equal(a.position(),-1);
});
