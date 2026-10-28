'use strict';
importScripts('hd-core.js','mod-core.js','rebirth-core.js','rebirth-plan.js');
const core=RE1ModCore,images=RE1HDCore;
let rootDir,library=[],scalerBinary,scalerReady,serial=0;
let mediaRoot,mediaReady,mediaSerial=0,mediaCached;
function scaler(){if(!scalerReady){if(typeof RE1HDResize!=='function')importScripts('hd-resize.js');scalerReady=RE1HDResize(scalerBinary?{wasmBinary:scalerBinary}:{});}return scalerReady;}
const replies=new Map(),archives=new Map();
function request(type,detail){const id=++serial;postMessage({type,id,...detail});return new Promise((resolve,reject)=>replies.set(id,{resolve,reject}));}
async function state(mode,value){
  const db=await new Promise((resolve,reject)=>{
    const r=indexedDB.open('re1-mods-state',1);r.onupgradeneeded=()=>r.result.createObjectStore('state');r.onsuccess=()=>resolve(r.result);r.onerror=()=>reject(r.error);
  });
  try{return await new Promise((resolve,reject)=>{
    let result;const tx=db.transaction('state',mode),store=tx.objectStore('state');
    const r=mode==='readonly'?store.get('library'):store.put(value,'library');r.onsuccess=()=>result=r.result;
    tx.oncomplete=()=>resolve(result);tx.onabort=tx.onerror=()=>reject(tx.error||Error('Mod storage could not be updated.'));
  });}finally{db.close();}
}
async function init(){
  rootDir=await (await navigator.storage.getDirectory()).getDirectoryHandle('re1-mods-v1',{create:true});
  const saved=await state('readonly');
  if(saved!==undefined&&(!Array.isArray(saved)||saved.length>100))throw Error('The mod library is damaged.');
  library=saved||[];
  for(const mod of library){
    if(!/^pack-[0-9a-f]{32}$/.test(mod.id)||!Array.isArray(mod.records)||!Array.isArray(mod.overrides)||mod.records.length>50000)throw Error('Invalid installed mod metadata.');
    try{
      const dir=await rootDir.getDirectoryHandle(mod.id),file=await (await dir.getFileHandle('images.bin')).getFile();
      for(const record of mod.records)if(!core.KEY.test(record.key)||!Number.isSafeInteger(record.offset)||record.offset<0||!Number.isSafeInteger(record.size)||record.size<=32||record.offset+record.size>file.size)throw Error('Invalid texture index');
      if(mod.localeSize){const locale=await (await dir.getFileHandle('locale.bin')).getFile();if(locale.size!==mod.localeSize||locale.size>2097152)throw Error('Invalid translation cache');}
      if(mod.campaignSize){const campaign=await (await dir.getFileHandle('campaign.bin')).getFile();if(campaign.size!==mod.campaignSize||campaign.size>262144)throw Error('Invalid campaign cache');core.campaign(new Uint8Array(await campaign.arrayBuffer()));}
    }catch(error){mod.enabled=false;mod.error='Cached textures are missing. Remove and reinstall this mod.';}
  }
  return library;
}
async function movie(message){
  const token=++mediaSerial,mod=library.find(item=>item.id===message.pack);
  const entry=mod?.overrides.find(item=>item.slot===message.slot&&/^movie\/[^/]+\.mp4$/i.test(item.target));
  if(!entry||!Number.isSafeInteger(entry.size)||entry.size<=0||entry.size>1024*1024*1024)throw Error('Installed movie is missing or invalid.');
  if(!mediaReady)mediaReady=(async()=>{
    const dir=await (await navigator.storage.getDirectory()).getDirectoryHandle('re1-mod-media-v1',{create:true});
    for await(const [name] of dir.entries())await dir.removeEntry(name,{recursive:true});
    mediaRoot=dir;
  })();
  await mediaReady;
  if(token!==mediaSerial)throw Error('Movie preparation cancelled.');
  const tag=mod.id+':'+entry.slot+':'+entry.size;
  if(mediaCached?.tag===tag){
    const file=await (await mediaRoot.getFileHandle(mediaCached.name)).getFile();
    if(file.size===entry.size)return file.slice(0,file.size,'video/mp4');
  }
  const name='movie-'+token+'.mp4';let output,complete=false;
  try{
    output=await (await mediaRoot.getFileHandle(name,{create:true})).createSyncAccessHandle();output.truncate(0);
    // Disk-backed File -> blob: URL avoids Android WebView's unreliable
    // intercepted HTTP media requests. Never collect a whole HD movie in RAM.
    for(let offset=0;offset<entry.size;offset+=262144){
      if(token!==mediaSerial)throw Error('Movie preparation cancelled.');
      const length=Math.min(262144,entry.size-offset);
      const bytes=new Uint8Array(await request('movie-chunk',{pack:mod.id,slot:entry.slot,offset,length}));
      if(token!==mediaSerial)throw Error('Movie preparation cancelled.');
      if(bytes.length!==length||output.write(bytes,{at:offset})!==length)throw Error('Incomplete installed movie read.');
      postMessage({type:'movie-progress',done:offset+length,total:entry.size});
    }
    output.flush();output.close();output=null;
    const file=await (await mediaRoot.getFileHandle(name)).getFile();
    if(token!==mediaSerial)throw Error('Movie preparation cancelled.');
    if(file.size!==entry.size)throw Error('Incomplete movie cache.');
    const previous=mediaCached;mediaCached={tag,name};complete=true;
    if(previous&&previous.name!==name)await mediaRoot.removeEntry(previous.name).catch(()=>{});
    return file.slice(0,file.size,'video/mp4');
  }finally{
    if(output)output.close();
    if(!complete)await mediaRoot.removeEntry(name).catch(()=>{});
  }
}
async function exclusive(task){
  return navigator.locks.request('re1-mod-library',{ifAvailable:true},async lock=>{
    if(!lock)throw Error('Close the game or mod manager in the other window first.');return task();
  });
}
async function cleanup(){
  const keep=new Set(library.map(m=>m.id));
  for await(const [name,handle] of rootDir.entries())if(handle.kind==='directory'&&/^pack-[0-9a-f]{32}$/.test(name)&&!keep.has(name))await rootDir.removeEntry(name,{recursive:true});
}
async function write(dir,name,bytes){
  const handle=await (await dir.getFileHandle(name,{create:true})).createSyncAccessHandle();
  try{handle.truncate(0);if(handle.write(bytes,{at:0})!==bytes.length)throw Error('Incomplete mod write');handle.flush();}finally{handle.close();}
}
async function sourceSignature(files){
  const signature=JSON.stringify(files.map(f=>[f.path.toLowerCase(),f.size]).sort((a,b)=>a[0].localeCompare(b[0])));
  return Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',new TextEncoder().encode(signature))),v=>v.toString(16).padStart(2,'0')).join('');
}
async function bytes(entry,id){
  if(entry.size>core.MAX_IMAGE_BYTES)throw Error('This texture/source file is too large: '+entry.path);
  const buffer=await request(id?'archive-read':'game-read',{pack:id,entry});
  const bytes=new Uint8Array(buffer);
  if(bytes.length!==entry.size||id&&core.crc32(bytes)!==entry.crc)throw Error('Incomplete or damaged mod file: '+entry.path);
  return bytes;
}
let lastSource=null,lastData;
async function original(definition,gameFiles,overrides,pack){
  const replacement=overrides.find(item=>item.target.toLowerCase()===definition.source.toLowerCase());
  const entry=replacement?.file||gameFiles.get(definition.source.toLowerCase());if(!entry)throw Error('Original game resource not found: '+definition.source);
  if(entry.size>images.MAX_BYTES)throw Error('Original texture container is too large: '+entry.path);
  const identity=replacement?pack+':'+entry.slot:entry.path;
  if(lastSource!==identity){lastData=await bytes(entry,replacement?pack:undefined);lastSource=identity;}
  const data=/\.pak$/i.test(definition.source)?images.unpackPak(lastData):lastData;
  let image=0;
  for(let at=0;at+20<=data.length;at++){
    if(data[at]!==16||data[at+1]||data[at+2]||data[at+3])continue;
    let tim;try{tim=images.tim(data,at);}catch{continue;}
    if(image++===definition.image){
      const rule=definition.rule||(/\.pak$/i.test(definition.source)&&tim.mode>=2?'opaque':'page');
      const rgba=images.decode(tim,definition.palette,rule);
      return {rgba,w:tim.w,h:tim.h,key:images.key(rgba,tim.w,tim.h)};
    }
    at=tim.end-1;
  }
  throw Error('Original image not found: '+entry.path);
}
async function decode(encoded){
  const size=core.imageSize(encoded),module=await scaler(),input=module._malloc(encoded.length),output=module._malloc(size.w*size.h*4);
  if(!input||!output){if(input)module._free(input);if(output)module._free(output);throw Error('Not enough memory to decode this texture.');}
  try{
    module.HEAPU8.set(encoded,input);
    if(!module._re1_mod_decode(input,encoded.length,size.w,size.h,output))throw Error('Could not decode texture. Use a still PNG, WebP or JPEG image.');
    return {...size,rgba:module.HEAPU8.slice(output,output+size.w*size.h*4)};
  }finally{module._free(input);module._free(output);}
}
function transformed(image,definition,source){
  const {w,h}=image,rgba=image.rgba.slice();
  if(definition.flipY){const row=w*4,scratch=new Uint8Array(row);for(let y=0;y<Math.floor(h/2);y++){
    const a=y*row,b=(h-1-y)*row;scratch.set(rgba.subarray(a,a+row));rgba.copyWithin(a,b,b+row);rgba.set(scratch,b);
  }}
  if(definition.swapRB)for(let at=0;at<rgba.length;at+=4){const red=rgba[at];rgba[at]=rgba[at+2];rgba[at+2]=red;}
  if(definition.profileAlpha==='opaque')for(let at=3;at<rgba.length;at+=4)rgba[at]=255;
  else if(definition.profileAlpha==='black')for(let at=0;at<rgba.length;at+=4)rgba[at+3]=(rgba[at]|rgba[at+1]|rgba[at+2])?255:0;
  else if(source&&definition.alpha!=='image'){
    let opaque=true;for(let at=3;at<rgba.length;at+=4)if(rgba[at]!==255){opaque=false;break;}
    if(opaque)for(let y=0;y<h;y++)for(let x=0;x<w;x++)rgba[(y*w+x)*4+3]=source.rgba[(Math.floor(y*source.h/h)*source.w+Math.floor(x*source.w/w))*4+3];
  }
  return rgba;
}
async function compress(rgba){
  if(rgba.length<=images.MAX_PIXELS*4){
    const module=await scaler(),input=module._malloc(rgba.length),output=module._malloc(rgba.length+65536);
    if(!input||!output){if(input)module._free(input);if(output)module._free(output);throw Error('Not enough memory to install this texture.');}
    try{module.HEAPU8.set(rgba,input);const size=module._re1_hd_compress(input,rgba.length,output,rgba.length+65536);if(!size)throw Error('Texture compression failed.');return module.HEAPU8.slice(output,output+size);}
    finally{module._free(input);module._free(output);}
  }
  return new Uint8Array(await new Response(new Blob([rgba]).stream().pipeThrough(new CompressionStream('deflate'))).arrayBuffer());
}
function seamlessPlan(files,prefix,profile,gameFiles){
  if(!profile||profile.format!=='re1-seamless-adapter'||profile.version!==1||!Array.isArray(profile.records))throw Error('The Seamless HD compatibility profile is missing.');
  const found=core.index(files),textures=[];
  for(const alias of profile.records){
    if(!core.KEY.test(alias.key))throw Error('Invalid Seamless compatibility profile.');
    const file=found.get((prefix+alias.file).toLowerCase());
    if(file)textures.push({...alias,file,profileAlpha:alias.alpha,alpha:'image'});
  }
  if(!textures.length)throw Error('No compatible RE1 Seamless HD textures were found in this ZIP.');
  // A patch-only ZIP uses the same directory names. Installing it as a full
  // replacement would silently discard most of an already installed pack.
  const coverage=new Set(textures.map(item=>item.file.path.toLowerCase())).size;
  const expected=new Set(profile.records.map(item=>item.file.toLowerCase())).size;
  if(coverage<expected*.9)throw Error('This appears to be an incomplete or patch-only Seamless HD ZIP. Select the full RE_SHDP_1.1.zip texture pack instead.');
  return {slug:'seamless-hd',name:'Seamless HD Project',author:'Seamless HD team',version:'1.1',
    adapterRevision:profile.revision||1,description:'RE1 Seamless HD textures and movies adapted for the GOG Android port. Original game files stay intact.',credits:profile.credits||'',textures,overrides:core.seamlessMovies(files,prefix,gameFiles)};
}
async function installationPlan(message,sourceFiles){
  const info=core.manifest(message.pack.files);
  const gameFiles=new Map(sourceFiles.map(f=>[f.path.toLowerCase(),f]));
  let plan,needsDetails=false;
  if(info.format==='native'){
    const encoded=await bytes(info.entry,message.pack.id);
    const json=JSON.parse(new TextDecoder('utf-8',{fatal:true}).decode(encoded));
    plan=core.plan(json,message.pack.files,info.prefix);needsDetails=typeof json.name!=='string'||!json.name.trim();
    if(plan.runtime){
      plan.campaign=new Uint8Array(await bytes(plan.runtime.campaign,message.pack.id));
      plan.campaignInfo=core.campaign(plan.campaign);
      if(plan.runtime.locale){
        plan.locale=new Uint8Array(await bytes(plan.runtime.locale,message.pack.id));
        const view=new DataView(plan.locale.buffer,plan.locale.byteOffset,plan.locale.byteLength);
        if(plan.locale.length<16||view.getUint32(0,true)!==0x544c3152||view.getUint32(4,true)!==1||view.getUint32(12,true)!==plan.locale.length||view.getUint32(8,true)>4096)throw Error('Invalid campaign translation data.');
        plan.localeCount=view.getUint32(8,true);
      }
      plan.localeOriginalFont=plan.runtime.originalFont;
      plan.compatibility=core.compatibility(message.pack.files,[info.entry.path,plan.runtime.campaign.path,...(plan.runtime.locale?[plan.runtime.locale.path]:[]),...plan.textures.map(t=>t.file.path),...plan.overrides.map(t=>t.file.path)],'Android campaign mod',[
        'Start a New Game for this campaign. Existing original-game saves are not campaign starting points.',
        'Test the campaign on its own before combining it with other packs. Windows plugins are not executed.'
      ]);
    }
  }else if(info.format==='pc-data'){
    plan=core.pcPlan(message.pack.files,message.pack.name,sourceFiles);
    const digest=await crypto.subtle.digest('SHA-256',new TextEncoder().encode(message.pack.name.toLowerCase()));
    plan.slug='pc.'+Array.from(new Uint8Array(digest),v=>v.toString(16).padStart(2,'0')).join('');
    const file=core.infoFile(message.pack.files);
    const metadata=file?core.parseInfo(new TextDecoder('utf-8',{fatal:true}).decode(await bytes(file,message.pack.id))):{};
    const previous=library.find(mod=>mod.slug===plan.slug);
    if(!metadata.name&&previous){plan.name=previous.name;plan.description=previous.description;}
    else if(!metadata.name&&metadata.description===undefined)plan.description='';
    for(const key of ['name','description','author','version'])if(metadata[key]!==undefined&&metadata[key]!=='')plan[key]=metadata[key];
    needsDetails=!metadata.name;
  }else if(info.format==='rebirth')plan=await rebirthPlan(message,info,sourceFiles);
  else plan=seamlessPlan(message.pack.files,info.prefix,message.profile,sourceFiles);
  return {plan,needsDetails};
}
async function inspect(message){
  await init();const files=await request('game-manifest',{}),{plan,needsDetails}=await installationPlan(message,files);
  return {name:plan.name,description:plan.description,needsDetails,compatibility:plan.compatibility};
}
async function install(message){return exclusive(async()=>{
  await init();await cleanup();lastSource=null;lastData=null;
  const sourceFiles=await request('game-manifest',{}),signature=await sourceSignature(sourceFiles);
  const gameFiles=new Map(sourceFiles.map(f=>[f.path.toLowerCase(),f]));
  const {plan}=await installationPlan(message,sourceFiles);
  if(message.details)Object.assign(plan,core.modDetails(message.details));
  if(library.length>=100&&!library.some(mod=>mod.slug===plan.slug))throw Error('Remove a mod before installing another; the library limit is 100 mods.');
  const id=message.pack.id;
  if(!/^pack-[0-9a-f]{32}$/.test(id)||library.some(m=>m.id===id))throw Error('Invalid mod installation id.');
  const dir=await rootDir.getDirectoryHandle(id,{create:true});
  let archive,offset=0,done=0,committed=false;const records=[],known=new Set(),groups=new Map();
  for(const definition of plan.textures){const slot=definition.file.slot;if(!groups.has(slot))groups.set(slot,[]);groups.get(slot).push(definition);}
  const progress=(phase,path='')=>postMessage({type:'progress',phase,path,done,total:groups.size,bytes:offset});
  try{
    archive=await (await dir.getFileHandle('images.bin',{create:true})).createSyncAccessHandle();
    for(const definitions of groups.values()){
      const file=definitions[0].file;progress('Converting textures',file.path);
      const encoded=await bytes(file,id),isFont=definitions[0].rebirthFont;
      const tim=isFont?images.tim(encoded,0):null;
      const decoded=isFont?{w:tim.w,h:tim.h,rgba:images.decode(tim)}:await decode(encoded),prepared=new Map(),crops=new Map();
      for(const definition of definitions){
        let image=decoded;
        if(definition.crop){const tag=definition.crop.join(',');if(!crops.has(tag))crops.set(tag,core.crop(decoded,definition.crop));image=crops.get(tag);}
        let originalImage,key=definition.key;
        if(definition.source){originalImage=await original(definition,gameFiles,plan.overrides,id);if(key&&key!==originalImage.key)throw Error('The original texture key does not match '+definition.source);key=originalImage.key;}
        if(definition.rebirthFont)image=RE1Rebirth.font(decoded,originalImage,definition.fontMap);
        else if(definition.pad)image=RE1Rebirth.pad(image,...definition.pad);
        if(!core.KEY.test(key))throw Error('Invalid original texture key.');
        if(known.has(key))continue;
        const [,w,h]=core.KEY.exec(key).map((value,n)=>n?Number(value):value),page=definition.layout==='page';core.dimensions(w,h,image.w,image.h,page);
        const variant=[w,h,definition.crop?.join(',')||'',definition.profileAlpha||'',definition.flipY||false,definition.swapRB||false,originalImage&&definition.alpha!=='image'?key:''].join(':');
        let record=prepared.get(variant);
        if(!record){
          const rgba=transformed(image,definition,originalImage),body=await compress(rgba),header=core.header(w,h,image.w,image.h,page);
          if(archive.write(header,{at:offset})!==header.length||archive.write(body,{at:offset+32})!==body.length)throw Error('Incomplete texture write.');
          record={offset,size:32+body.length};offset+=record.size;prepared.set(variant,record);
        }
        known.add(key);records.push({key,...record,width:image.w,height:image.h,path:definition.source||definition.path||'',nativeSize:!!plan.locale&&image.w===w&&image.h===h});
      }
      done++;progress('Converting textures',file.path);
      if(done%16===0){const storage=await navigator.storage.estimate();if(storage.quota-storage.usage<32*1024*1024)throw Error('Not enough storage to finish the mod. Free some space and retry.');}
    }
    archive.flush();archive.close();archive=null;
    const overrides=plan.overrides.map(({target,file})=>({target,slot:file.slot,size:file.size}));
    const {textures,overrides:ignored,locale,campaign,runtime,...meta}=plan;
    if(locale)await write(dir,'locale.bin',locale);
    if(campaign)await write(dir,'campaign.bin',campaign);
    const originalFontKey=locale&&plan.localeOriginalFont?(await original({source:'Data/fontus.tim',image:0,palette:0,rule:'page'},gameFiles,plan.overrides,id)).key:null;
    const mod={...meta,id,enabled:true,source:signature,bytes:offset,imageCount:groups.size,records,overrides,localeSize:locale?.length||0,campaignSize:campaign?.length||0,localeOriginalFontKey:originalFontKey,
      localeFontKey:locale?records.find(record=>record.path==='Data/fontus.tim')?.key:null,installedAt:Date.now()};
    await write(dir,'manifest.json',new TextEncoder().encode(JSON.stringify(mod)));
    progress('Finishing installation');
    await request('native-commit',{pack:id,slots:[...new Set(overrides.map(f=>f.slot))]});
    const old=library.find(m=>m.slug===mod.slug),position=old?library.indexOf(old):0;
    const next=library.filter(m=>m.slug!==mod.slug);next.splice(position,0,mod);
    await state('readwrite',next);library=next;committed=true;
    if(old){await rootDir.removeEntry(old.id,{recursive:true}).catch(()=>{});await request('native-remove',{pack:old.id}).catch(()=>{});}
    archives.clear();return library;
  }catch(error){
    if(archive){archive.close();archive=null;}
    if(!committed){await rootDir.removeEntry(id,{recursive:true}).catch(()=>{});await request('native-remove',{pack:id}).catch(()=>{});}throw error;
  }
  finally{if(archive)archive.close();lastSource=null;lastData=null;scalerReady=null;}
});}
async function update(message){return exclusive(async()=>{
  await init();const next=library.map(mod=>({...mod})),at=next.findIndex(m=>m.id===message.pack);
  if(at<0)throw Error('Mod is no longer installed.');
  if(message.action==='enabled'){if(next[at].error)throw Error(next[at].error);next[at].enabled=!!message.enabled;}
  else if(message.action==='details')Object.assign(next[at],core.modDetails(message.details));
  else if(message.action==='move'){
    const to=Math.max(0,Math.min(next.length-1,at+(message.direction<0?-1:1))),[item]=next.splice(at,1);next.splice(to,0,item);
  }else if(message.action==='remove')next.splice(at,1);
  else throw Error('Unknown mod action.');
  await state('readwrite',next);library=next;archives.clear();
  if(message.action==='remove'){
    await rootDir.removeEntry(message.pack,{recursive:true}).catch(()=>{});await request('native-remove',{pack:message.pack}).catch(()=>{});
  }
  return library;
});}
let loadQueue=Promise.resolve();const prefetched=new Set();
async function load(keys){
  const requested=new Set(keys),extra=new Set();
  for(const item of keys){
    const mod=library.find(m=>m.id===item.id),record=mod?.records.find(r=>r.key===item.key),match=record?.path?.match(/^(.*\/RC[0-9a-f]{3})[0-9a-f]\.pak$/i);
    if(match&&!prefetched.has(item.id+match[1])){
      prefetched.add(item.id+match[1]);for(const other of mod.records)if(other.path?.slice(0,-5).toLowerCase()===match[1].toLowerCase())extra.add({id:item.id,key:other.key});
    }
  }
  const seen=new Set();
  for(const item of [...requested,...extra]){
    const tag=item.id+item.key;if(seen.has(tag))continue;seen.add(tag);
    try{
      const mod=library.find(m=>m.id===item.id),record=mod?.records.find(r=>r.key===item.key);if(!record)throw Error('Unknown mod texture.');
      let file=archives.get(item.id);
      if(!file){const dir=await rootDir.getDirectoryHandle(item.id);file=await (await dir.getFileHandle('images.bin')).getFile();archives.set(item.id,file);}
      const packed=new Uint8Array(await file.slice(record.offset,record.offset+record.size).arrayBuffer()),view=new DataView(packed.buffer);
      const version=view.getUint32(4,true);
      if(packed.length<32||view.getUint32(0,true)!==0x44483152||![3,4].includes(version))throw Error('Invalid installed mod texture.');
      const w=view.getUint32(8,true),h=view.getUint32(12,true),ow=view.getUint32(16,true),oh=view.getUint32(20,true);core.dimensions(w,h,ow,oh,version===4);
      const size=ow*oh*4;
      const logical=record.crop?record.crop.slice(2,4):[w,h];
      if(record.crop&&(record.crop[4]!==w||record.crop[5]!==h))throw Error('Invalid installed crop source.');
      if(view.getUint32(24,true)!==size||view.getUint32(28,true)||!item.key.endsWith('_'+logical[0]+'x'+logical[1]))throw Error('Invalid mod texture dimensions.');
      let raw=new Uint8Array(32+size);raw.set(packed.subarray(0,32));let written=0;
      const reader=new Blob([packed.subarray(32)]).stream().pipeThrough(new DecompressionStream('deflate')).getReader();
      try{for(;;){const {done,value}=await reader.read();if(done)break;if(written+value.length>size){await reader.cancel();throw Error('Mod texture expands beyond its dimensions.');}raw.set(value,32+written);written+=value.length;}}
      finally{reader.releaseLock();}
      if(written!==size)throw Error('Incomplete mod texture.');
      if(record.crop){
        const image=core.crop({w:ow,h:oh,rgba:raw.subarray(32)},record.crop);
        raw=new Uint8Array(32+image.rgba.length);raw.set(core.header(logical[0],logical[1],image.w,image.h));raw.set(image.rgba,32);
      }
      postMessage({type:'pixels',key:item.key,raw:raw.buffer,prefetch:!requested.has(item)},[raw.buffer]);
    }catch(error){postMessage({type:'load-error',key:item.key,message:String(error.message||error)});}
  }
}
onmessage=async event=>{
  const message=event.data;
  if(message.type==='movie-cancel'){++mediaSerial;return;}
  if(message.type==='reply'){
    const pending=replies.get(message.id);if(pending){replies.delete(message.id);message.error?pending.reject(Error(message.error)):pending.resolve(message.result);}return;
  }
  if(message.type==='load'){loadQueue=loadQueue.then(()=>load(message.keys)).catch(error=>postMessage({type:'load-error',message:String(error)}));return;}
  try{
    let result;
    if(message.type==='init'){
      scalerBinary=message.scalerWasm;result=await init();
      await exclusive(async()=>{
        const next=library.map(mod=>core.upgradeSeamless(mod,message.profile));
        if(next.some((mod,n)=>mod!==library[n])){await state('readwrite',next);library=next;}
        await cleanup().catch(()=>{});await request('native-cleanup',{keep:library.map(m=>m.id)}).catch(()=>{});
      });
      result=library;
    }
    else if(message.type==='movie')result=await movie(message);
    else if(message.type==='locale'){
      const mod=library.find(m=>m.id===message.pack);if(!mod?.localeSize)throw Error('Translation is missing.');
      const dir=await rootDir.getDirectoryHandle(mod.id),file=await (await dir.getFileHandle('locale.bin')).getFile();
      if(file.size!==mod.localeSize||file.size>2097152)throw Error('Translation cache is invalid.');result=await file.arrayBuffer();
    }
    else if(message.type==='campaign'){
      const mod=library.find(m=>m.id===message.pack);if(!mod?.campaignSize)throw Error('Campaign data is missing.');
      const dir=await rootDir.getDirectoryHandle(mod.id),file=await (await dir.getFileHandle('campaign.bin')).getFile();
      if(file.size!==mod.campaignSize||file.size>262144)throw Error('Campaign cache is invalid.');result=await file.arrayBuffer();core.campaign(new Uint8Array(result));
    }
    else if(message.type==='inspect')result=await inspect(message);
    else if(message.type==='install')result=await install(message);
    else if(message.type==='update')result=await update(message);
    else throw Error('Unknown mod worker request.');
    postMessage({type:'result',id:message.id,result});
  }catch(error){postMessage({type:'result',id:message.id,error:String(error.message||error)});}
};
