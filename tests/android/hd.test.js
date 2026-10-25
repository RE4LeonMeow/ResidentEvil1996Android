const test=require('node:test'),assert=require('node:assert/strict');
const hd=require('../../android/app/src/main/assets/web/hd-core.js');
function tim16(){
  const data=new Uint8Array(20+4*4*2),v=new DataView(data.buffer);
  v.setUint32(0,16,true);v.setUint32(4,2,true);v.setUint32(8,44,true);v.setUint16(16,4,true);v.setUint16(18,4,true);
  for(let i=0;i<16;i++)v.setUint16(20+i*2,[0,31,992,31744][i%4],true);return data;
}
function pack(codes){
  let bits=9,output='';for(const code of codes){output+=code.toString(2).padStart(bits,'0');if(code===257)bits++;if(code===258)bits=9;}
  output=output.padEnd(Math.ceil(output.length/8)*8,'0');return Uint8Array.from(output.match(/.{8}/g),s=>parseInt(s,2));
}
test('1080p dimensions preserve proportions and bound texture allocations',()=>{
  assert.deepEqual(hd.dimensions(320,240),[1440,1080]);assert.deepEqual(hd.dimensions(256,256),[1152,1152]);assert.deepEqual(hd.dimensions(1024,512),[2048,1024]);
  for(const dims of [[0,1],[-1,10],[100000,100000],[1.5,5]])assert.throws(()=>hd.dimensions(...dims));
});
test('PAK decoding supports KwKwK, resets, width markers and malformed input',()=>{
  assert.deepEqual([...hd.unpackPak(pack([65,259,256]))],[65,65,65]);
  assert.deepEqual([...hd.unpackPak(pack([65,258,66,257,67,256]))],[65,66,67]);
  assert.throws(()=>hd.unpackPak(new Uint8Array()));assert.throws(()=>hd.unpackPak(pack([65,300,256])));
});
test('TIM base images keep real black; texture pages retain black cutouts',()=>{
  const data=tim16(),t=hd.tim(data,0),opaque=hd.decode(t,0,'opaque'),page=hd.decode(t,0,'page');
  assert.deepEqual([...opaque.slice(0,16)],[0,0,0,255,255,0,0,255,0,255,0,255,0,0,255,255]);
  assert.equal(page[3],0);assert.equal(page[7],255);assert.throws(()=>hd.tim(data.slice(0,-1),0));
  assert.equal([...hd.images(data,'Data/test.tim')].length,2);
});
test('Lanczos scaling preserves uniform colors and exact nearest alpha coverage',()=>{
  const source=new Uint8Array(4*4*4);for(let i=0;i<source.length;i+=4)source.set([190,65,12,255],i);
  const out=hd.upscale(source,4,4);assert.equal(out.w,18);assert.equal(out.h,18);
  for(let i=0;i<out.rgba.length;i+=4)assert.deepEqual([...out.rgba.slice(i,i+4)],[190,65,12,255]);
  source[3]=0;const masked=hd.upscale(source,4,4).rgba;
  for(let y=0;y<18;y++)for(let x=0;x<18;x++)assert.equal(masked[(y*18+x)*4+3],Math.floor((x+.5)*4/18)===0&&Math.floor((y+.5)*4/18)===0?0:255);
});
test('image keys are content and dimension specific',()=>{
  const a=hd.decode(hd.tim(tim16(),0),0,'opaque');
  assert.equal(hd.key(a,4,4),'3338aa45a0a44d056b166c3958f0cbdb_4x4');
  assert.notEqual(hd.key(a,4,4),hd.key(a,8,2));a[0]^=1;assert.notEqual(hd.key(a,4,4),'3338aa45a0a44d056b166c3958f0cbdb_4x4');
});
test('camera worker transfers each requested/prefetched image once with exact RGBA',async()=>{
  const vm=require('node:vm'),fs=require('node:fs'),{deflateSync}=require('node:zlib');
  const rgba=Buffer.alloc(18*18*4);for(let i=0;i<rgba.length;i++)rgba[i]=i%251;
  const header=Buffer.alloc(32);header.write('R1HD');[2,4,4,18,18,rgba.length,0].forEach((v,i)=>header.writeUInt32LE(v,4+i*4));
  const packed=Buffer.concat([header,deflateSync(rgba)]),messages=[];
  const keys=['0'.repeat(32)+'_4x4','1'.repeat(32)+'_4x4'];
  const context=vm.createContext({Blob,DecompressionStream,RE1HDCore:hd,importScripts(){},postMessage:m=>messages.push(m),fixture:{keys,packed:new Blob([packed])}});
  vm.runInContext(fs.readFileSync(require.resolve('../../android/app/src/main/assets/web/hd-worker.js'),'utf8'),context);
  await vm.runInContext(`(async()=>{
    activeDir={};active={version:2,records:fixture.keys.map((key,i)=>({key,path:'Stage1/RC105'+i+'.pak'}))};
    imageFiles=new Map(fixture.keys.map(key=>[key,fixture.packed]));
    await loadImages([fixture.keys[0]]);
  })()`,context);
  assert.equal(messages.length,2);assert.deepEqual(messages.map(m=>m.key),keys);
  for(const [i,m] of messages.entries()){
    assert.equal(m.type,'pixels');assert.equal(m.prefetch,i!==0);
    assert.deepEqual(Buffer.from(m.raw),Buffer.concat([header,rgba]));
  }
});
