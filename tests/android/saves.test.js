const {test}=require('node:test');
const assert=require('node:assert/strict');
const saves=require('../../android/app/src/main/assets/web/saves.js');
const slot=(n,length=0xa82)=>({name:'savedat'+n+'.dat',contents:Uint8Array.from({length},(_,i)=>(i*37+n)&255)});
test('all eight mixed-size slots round-trip byte-for-byte through a standard ZIP',async()=>{
  const files=Array.from({length:8},(_,i)=>slot(i+1,i%2?0x800:0xa82));
  assert.deepEqual(await saves.unzip(saves.zip(files)),files);
  assert.equal(saves.checksum(new TextEncoder().encode('123456789')),'cbf43926');
  assert.throws(()=>saves.zip([]),/No saved games/);
});
test('C/WASI signed arrays preserve bytes and offsets for both save sizes',async()=>{
  for(const length of [0x800,0xa82]){
    const memory=new Int8Array(length+64).fill(-77),signed=memory.subarray(17,17+length);
    for(let i=0;i<length;i++)signed[i]=(i*37+11)&255;
    const expected=Uint8Array.from({length},(_,i)=>(i*37+11)&255),original=memory.slice();
    for(const contents of [signed,new DataView(memory.buffer,17,length),expected.buffer])assert.deepEqual((await saves.unzip(saves.zip([{name:'savedat1.dat',contents}])))[0].contents,expected);
    assert.deepEqual(memory,original);
  }
});
test('GOG slot names canonicalize safely and unsupported sizes are rejected',()=>{
  const compact={name:'SAVEDAT8.DAT',contents:new Int8Array(0x800).fill(-1)};
  assert.deepEqual(saves.raw([compact]),[{name:'savedat8.dat',contents:new Uint8Array(0x800).fill(255)}]);
  assert.throws(()=>saves.raw([compact,{...compact,name:'savedat8.dat'}]));
  for(const length of [0,0x7ff,0x801,0xa81,0xa83,131072])assert.throws(()=>saves.raw([{name:'savedat1.dat',contents:new Uint8Array(length)}]));
  for(const name of ['../savedat1.dat','config.ini','savedat9.dat','SAVE/savedat1.dat'])assert.throws(()=>saves.raw([{...compact,name}]));
});
test('damaged, encrypted, oversized and incomplete ZIPs are rejected',async()=>{
  const good=saves.zip([slot(1)]);
  for(const value of [new Uint8Array(),new Uint8Array(131073),good.slice(0,-1)])await assert.rejects(saves.unzip(value));
  const corrupt=good.slice();corrupt[42]^=1;await assert.rejects(saves.unzip(corrupt),/Damaged save data/);
  const end=good.length-22,central=new DataView(good.buffer).getUint32(end+16,true);
  const encrypted=good.slice();new DataView(encrypted.buffer).setUint16(central+8,1,true);await assert.rejects(saves.unzip(encrypted),/Encrypted/);
  const traversal=good.slice();traversal.set(new TextEncoder().encode('../evil1.dat'),central+46);await assert.rejects(saves.unzip(traversal),/ZIP must contain/);
});
