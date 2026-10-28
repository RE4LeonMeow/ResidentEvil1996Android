'use strict';
const {test}=require('node:test'),assert=require('node:assert/strict');
const core=require('../../android/app/src/main/assets/web/mod-core.js');
function profile(){
 const bytes=new Uint8Array(32+1052+40+896+57*4*2+2+48),v=new DataView(bytes.buffer);
 bytes.set(Buffer.from('R1CP'));v.setUint32(4,1,true);v.setUint32(8,bytes.length,true);
 bytes[13]=1;v.setInt16(14,8000,true);v.setInt16(16,6615,true);v.setInt16(18,3584,true);
 [1052,40,1,57,224,0].forEach((x,n)=>v.setUint16(20+n*2,x,true));bytes[32+512]=4;bytes[32+513]=19;
 bytes.fill(255,32+1052+40,32+1052+40+896);
 return bytes;
}
test('campaign profile reads bounded data tables and signed spawn coordinates',()=>{
 const p=profile(),v=new DataView(p.buffer);v.setInt16(14,-1234,true);
 assert.deepEqual(core.campaign(p),{character:0,skipSelection:true,x:-1234,z:6615,angle:3584,soundRows:1,countdownSeconds:180});
});
test('campaign profile rejects malformed headers, room, inventory, table counts and indexes',()=>{
 for(const [offset,value] of [[0,0],[4,2],[8,0],[12,2],[13,2],[19,255],[20,0],[22,0],[24,204],[26,56],[28,223],[30,1],[32+512,7],[32+513,29],[32+514,32],[32+1052,129],[32+1052+40,57],[32+1052+40+896+1,2]]){
  const p=profile();p[offset]=value;assert.throws(()=>core.campaign(p),'offset '+offset);
 }
 for(const length of [0,2020,profile().length-1])assert.throws(()=>core.campaign(profile().slice(0,length)));
 const p=new Uint8Array(profile().length+1);p.set(profile());new DataView(p.buffer).setUint32(8,p.length,true);assert.throws(()=>core.campaign(p),/Unexpected data/);
});
test('campaign sound names reject traversal and oversized names; sound rows cannot repeat',()=>{
 const start=32+1052+40+896;
 const replace=(name)=>{
  const base=profile(),p=new Uint8Array(base.length+name.length);p.set(base.subarray(0,start),0);p[start]=name.length;p.set(Buffer.from(name),start+1);p.set(base.subarray(start+1),start+name.length+1);new DataView(p.buffer).setUint32(8,p.length,true);return p;
 };
 for(const name of ['../a','a/b','a\\b','a:','a'.repeat(64)])assert.throws(()=>core.campaign(replace(name)));
 assert.equal(core.campaign(replace('m_001')).soundRows,1);
 const base=profile(),p=new Uint8Array(base.length+50);p.set(base);new DataView(p.buffer).setUint32(8,p.length,true);new DataView(p.buffer).setUint16(24,2,true);assert.throws(()=>core.campaign(p),/duplicate/);
});
test('campaign format keeps executable and save paths outside resource overrides',()=>{
 const files=[{path:'mod.json',slot:0,size:100,crc:0},{path:'campaign.bin',slot:1,size:profile().length,crc:0}];
 const manifest={format:'re1-android-mod',formatVersion:2,game:'re1-usa',id:'test',runtime:{campaign:'campaign.bin'},textures:[],files:[]};
 assert.equal(core.plan(manifest,files).runtime.campaign.path,'campaign.bin');
 assert.throws(()=>core.plan({...manifest,formatVersion:1},files));
 for(const target of ['aydan.dll','Data/aydan.dll','SaveData/savedat1.dat','Data/bio_card.dat'])assert.throws(()=>core.gamePath(target));
});

test('campaign countdown allows whole-minute limits with the original default',()=>{for(const seconds of [60,600,3600]){const p=profile();new DataView(p.buffer).setUint16(30,seconds,true);assert.equal(core.campaign(p).countdownSeconds,seconds);}for(const seconds of [59,61,3601,65535]){const p=profile();new DataView(p.buffer).setUint16(30,seconds,true);assert.throws(()=>core.campaign(p));}});
