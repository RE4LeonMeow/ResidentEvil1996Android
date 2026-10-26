'use strict';
const {test}=require('node:test'),assert=require('node:assert/strict');
const core=require('../../android/app/src/main/assets/web/mod-core.js');
const entry=(path,slot=0,size=10)=>({path,slot,size,crc:0});
const manifest={format:'re1-android-mod',formatVersion:1,game:'re1-usa',id:'tester.example',name:'Example'};
test('plain mod info supports UTF-8/BOM, multiline descriptions and bounded metadata',()=>{
 const parsed=core.parseInfo('\uFEFFName: Claire Redfield\r\nDescription=Replaces Jill.\r\nSecond line: animations stay.\r\n\r\nAnother paragraph.\r\nAuthor: Mod author\r\nVersion: 1.1');
 assert.deepEqual(parsed,{name:'Claire Redfield',description:'Replaces Jill.\nSecond line: animations stay.\n\nAnother paragraph.',author:'Mod author',version:'1.1'});
 assert.equal(core.infoFile([entry('Claire/MOD-INFO.TXT')]).path,'Claire/MOD-INFO.TXT');
 assert.throws(()=>core.infoFile([entry('mod-info.txt'),entry('other/mod-info.txt',1)]),/one mod-info/);
 assert.throws(()=>core.infoFile([entry('mod-info.txt',0,16385)]),/16 KiB/);
 assert.throws(()=>core.parseInfo('Name: A\nNAME=B'),/Duplicate/);
 assert.throws(()=>core.parseInfo('Name: '+ 'x'.repeat(121)),/too long/);
 assert.throws(()=>core.modDetails({name:' ',description:''}));
 assert.deepEqual(core.modDetails({name:' <img src=x> ',description:' Literal <script> '}),{name:'<img src=x>',description:'Literal <script>'});
 assert.throws(()=>core.manifest([entry('Claire.rar')]),/inner archive/);
});
test('PC resource ZIPs keep model paths, ignore plugins, and require matching GOG resources',()=>{
 const files=[entry('Mega/USA/Enemy/EM1000.EMD'),entry('Mega/USA/Players/W01.TMD',1),entry('Mega/ddraw.dll',2),entry('Mega/USA/SAVE/savedat1.dat',3)];
 assert.equal(core.manifest(files).format,'pc-data');
 const plan=core.pcPlan(files,'Megaman.zip',[{path:'Enemy/em1000.emd'},{path:'Players/w01.tmd'}]);
 assert.equal(plan.name,'Megaman');assert.equal(plan.textures.length,0);
 assert.deepEqual(plan.overrides.map(r=>r.target),['Enemy/EM1000.EMD','Players/W01.TMD']);
 assert.throws(()=>core.pcPlan(files,'Megaman.zip',[{path:'Enemy/EM1001.EMD'}]),/No PC resources match/);
 assert.throws(()=>core.manifest([entry('setup.exe'),entry('ddraw.dll',1)]));
 assert.throws(()=>core.manifest([entry('a/Enemy/EM1000.EMD'),entry('b/Enemy/em1000.emd',1)]),/Duplicate PC/);
 const original={id:'pc',enabled:true,source:'gog',records:[],overrides:plan.overrides};
 assert.equal(core.active([original],'gog').files.size,2);
 assert.equal(core.active([{...original,enabled:false}],'gog').files.size,0);
});
test('installed SHDP hall aliases reuse exact payloads without reimport and leave other mods alone',()=>{
 const profile=require('../../android/app/src/main/assets/web/shdp-profile.json');
 const aliases=profile.records.filter(record=>record.reuse&&!record.crop);assert.equal(aliases.length,4);
 const records=[...new Set(aliases.map(alias=>alias.reuse))].map((key,n)=>({key,offset:1000*n,size:200,width:1024,height:1024}));
 const mod={slug:'seamless-hd',adapterRevision:2,enabled:false,source:'gog',records};
 const next=core.upgradeSeamless(mod,profile);assert.notEqual(next,mod);assert.equal(mod.records.length,2);
 assert.equal(next.enabled,false);assert.equal(next.source,'gog');
 for(const alias of aliases){const source=records.find(r=>r.key===alias.reuse),copy=next.records.find(r=>r.key===alias.key);assert.deepEqual(copy,{...source,key:alias.key,path:alias.path});}
 assert.equal(core.upgradeSeamless(next,profile),next);
 for(const other of [{...mod,slug:'custom'},{...mod,adapterRevision:1},{...mod,error:'missing'}])assert.equal(core.upgradeSeamless(other,profile),other);
 const missing={...mod,records:[]};assert.equal(core.upgradeSeamless(missing,profile),missing);
 const wrong={revision:3,records:[{key:aliases[0].key.replace('256x256','128x128'),reuse:aliases[0].reuse}]};
 assert.equal(core.upgradeSeamless(mod,wrong),mod);
});
test('installed padded SHDP icons migrate to exact sprite crops without altering their cached source',()=>{
 const full=require('../../android/app/src/main/assets/web/shdp-profile.json'),alias=full.records.find(r=>r.reuse&&r.crop);
 assert.ok(alias);assert.deepEqual(alias.crop,[0,0,40,30,64,64]);
 const source={key:alias.reuse,offset:128,size:500,width:256,height:256};
 const mod={slug:'seamless-hd',adapterRevision:3,records:[source]};
 const upgraded=core.upgradeSeamless(mod,{revision:4,records:[alias]});
 assert.deepEqual(upgraded.records.find(r=>r.key===alias.key),{...source,key:alias.key,path:'Data/ITEM_ALL.PIX',crop:alias.crop,width:160,height:120});
 assert.equal(mod.records[0],source);assert.equal(mod.records.length,1);
 assert.equal(core.upgradeSeamless(upgraded,{revision:4,records:[alias]}),upgraded);
});
test('texture-page layout keeps all HD pixels, cropped masks keep exact RGBA and user proportions remain strict',()=>{
 assert.throws(()=>core.header(128,256,1024,1024));
 const page=core.header(128,256,1024,1024,true);assert.equal(new DataView(page.buffer).getUint32(4,true),4);
 const rgba=Uint8Array.from({length:4*4*4},(_,i)=>i),image={w:4,h:4,rgba};
 assert.deepEqual(core.crop(image,[0,0,2,2,4,4]),{w:2,h:2,rgba:Uint8Array.from([...rgba.slice(0,8),...rgba.slice(16,24)])});
 assert.throws(()=>core.crop(image,[3,0,2,2,4,4]));
});
test('Seamless movie mapping uses only original direct MP4 choices and valid GOG movie names',()=>{
 const files=[entry('Pack/movie/PJ.mp4'),entry('Pack/movie/OJ.mp4',1),entry('Pack/movie/DM7 clips/Original/Dm7.mp4',2),entry('Pack/movie/Dm7.mp4',3),entry('Pack/movie/Ed4.mp4',4)];
 const result=core.seamlessMovies(files,'Pack/',[{path:'Movie/pu.avi'},{path:'Movie/ou.avi'},{path:'Movie/dm7.avi'},{path:'Movie/eu4.avi'}]);
 assert.deepEqual(result.map(r=>r.target),['Movie/pu.mp4','Movie/ou.mp4','Movie/dm7.mp4','Movie/eu4.mp4']);
});
test('mods identify a wrapped manifest and infer simple texture/data folders',()=>{
 const files=[entry('Example/mod.json'),entry('Example/textures/Stage1/RC1000.pak.png',1),entry('Example/files/Sound/gun01.wav',2)];
 const found=core.manifest(files);assert.equal(found.prefix,'Example/');
 const plan=core.plan(manifest,files,found.prefix);assert.equal(plan.textures[0].source,'Stage1/RC1000.pak');assert.equal(plan.overrides[0].target,'Sound/gun01.wav');
});
test('mods reject traversal, save/config/code targets and ambiguous ZIP contents',()=>{
 for(const value of ['../x','/x','C:/x','a/../x','a//b','a\0b'])assert.throws(()=>core.path(value));
 for(const target of ['SAVE/savedat1.dat','Data/../../SAVE/savedat1.dat','config.ini','Data/evil.js','Players/mod.dll'])assert.throws(()=>core.gamePath(target));
 assert.throws(()=>core.index([entry('mod.json'),entry('MOD.JSON',1)]));
 assert.throws(()=>core.manifest([entry('a/mod.json'),entry('b/mod.json',1)]));
 assert.throws(()=>core.manifest([entry('index.html')]));
});
test('native mods require an explicit supported game/format and bounded definitions',()=>{
 const files=[entry('mod.json'),entry('image.png',1)];
 for(const changes of [{game:'re2'},{formatVersion:9},{id:'../bad'}])assert.throws(()=>core.plan({...manifest,...changes},files));
 assert.throws(()=>core.plan({...manifest,textures:[{source:'Stage1/RC1000.pak',file:'missing.png'}]},files));
 assert.throws(()=>core.plan({...manifest,textures:[{source:'Stage1/RC1000.pak',file:'image.png',palette:256}]},files));
 assert.throws(()=>core.plan({...manifest,files:[{target:'Sound/gun01.wav',file:'image.png'},{target:'sound/GUN01.WAV',file:'image.png'}]},files));
});
test('a top-priority enabled mod wins conflicts while other replacements survive',()=>{
 const mods=[{id:'high',source:'gog',enabled:true,records:[{key:'A'}],overrides:[{target:'Sound/gun.wav'}]},
 {id:'low',source:'gog',enabled:true,records:[{key:'A'},{key:'B'}],overrides:[{target:'SOUND/GUN.WAV'},{target:'Sound/step.wav'}]},
 {id:'other',source:'other',enabled:true,records:[{key:'C'}]},
 {id:'off',source:'gog',enabled:false,records:[{key:'D'}]}];
 const active=core.active(mods,'gog');assert.equal(active.textures.get('A').id,'high');assert.equal(active.textures.get('B').id,'low');
 assert.equal(active.files.get('sound/gun.wav').id,'high');assert.equal(active.files.get('sound/step.wav').id,'low');assert.equal(active.conflicts.get('low'),2);
 assert.equal(active.textures.has('C'),false);assert.equal(active.textures.has('D'),false);
});
test('mod headers support proportional artist images without fixed AI-pack sizing',()=>{
 const header=core.header(320,240,1280,960),view=new DataView(header.buffer);
 assert.equal(view.getUint32(4,true),3);assert.equal(view.getUint32(16,true),1280);assert.equal(view.getUint32(24,true),1280*960*4);
 for(const dims of [[320,240,1920,1080],[320,240,160,120],[1,1,8192,8192],[0,240,1280,960]])assert.throws(()=>core.header(...dims));
});
test('image headers are bounded before browser image decoding',()=>{
 const png=new Uint8Array(24);png.set([137,80,78,71,13,10,26,10]);const p=new DataView(png.buffer);p.setUint32(12,0x49484452);p.setUint32(16,1280);p.setUint32(20,960);
 assert.deepEqual(core.imageSize(png),{w:1280,h:960});p.setUint32(16,99999);assert.throws(()=>core.imageSize(png));
 const webp=new Uint8Array(30);webp.set(Buffer.from('RIFF'));webp.set(Buffer.from('WEBPVP8X'),8);const w=new DataView(webp.buffer);w.setUint32(4,22,true);webp[24]=255;webp[25]=3;webp[27]=255;webp[28]=3;
 assert.deepEqual(core.imageSize(webp),{w:1024,h:1024});w.setUint32(4,999999,true);assert.throws(()=>core.imageSize(webp));
 assert.throws(()=>core.imageSize(new Uint8Array(100)));
});
test('Seamless detection skips Directors Cut folders and CRC32 matches ZIP standard',()=>{
 assert.equal(core.manifest([entry('Pack/hires/bgd/ABCDEF12.webp')]).prefix,'Pack/');
 assert.throws(()=>core.manifest([entry('bio1dc/hires/bgd/ABCDEF12.webp')]));
 assert.equal(core.crc32(Buffer.from('123456789')),0xcbf43926);
});
