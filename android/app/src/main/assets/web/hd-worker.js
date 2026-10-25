'use strict';
importScripts('hd-core.js');
const core=RE1HDCore;
let scalerReady,scalerBinary;
function getScaler(){if(!scalerReady){if(typeof RE1HDResize!=='function')importScripts('hd-resize.js');scalerReady=RE1HDResize(scalerBinary?{wasmBinary:scalerBinary}:{});}return scalerReady;}
let root,active=null,activeDir=null,serial=0;
const replies=new Map();
function request(type,detail){
  const id=++serial;postMessage({type,id,...detail});return new Promise((resolve,reject)=>replies.set(id,{resolve,reject}));
}
async function state(mode,value){
  const db=await new Promise((resolve,reject)=>{
    const r=indexedDB.open('re1-hd-state',1);r.onupgradeneeded=()=>r.result.createObjectStore('state');r.onsuccess=()=>resolve(r.result);r.onerror=()=>reject(r.error);
  });
  return new Promise((resolve,reject)=>{
    let result;const tx=db.transaction('state',mode),store=tx.objectStore('state');
    const r=mode==='readonly'?store.get('active'):value?store.put(value,'active'):store.delete('active');
    r.onsuccess=()=>{result=r.result;};tx.oncomplete=()=>{db.close();resolve(result);};tx.onabort=tx.onerror=()=>{db.close();reject(tx.error||Error('HD pack storage failed'));};
  });
}
async function write(dir,name,bytes){
  const handle=await dir.getFileHandle(name,{create:true}),file=await handle.createSyncAccessHandle();
  try{file.truncate(0);if(file.write(bytes,{at:0})!==bytes.length)throw Error('HD pack write was incomplete');file.flush();}finally{file.close();}
}
async function list(dir){
  const files=[];
  if(active?.records){
    const archive=await (await dir.getFileHandle('images.bin')).getFile();
    for(const {key,offset,size} of active.records){
      if(!/^[0-9a-f]{32}_[0-9]+x[0-9]+$/.test(key)||!Number.isSafeInteger(offset)||!Number.isSafeInteger(size)||offset<0||size<32||offset+size>archive.size)throw Error('Invalid HD archive index');
      files.push({key,file:archive.slice(offset,offset+size)});
    }
  }
  for await(const [name,handle] of dir.entries())if(/^[0-9a-f]{32}_[0-9]+x[0-9]+\.r1h$/.test(name)&&handle.kind==='file')files.push({key:name.slice(0,-4),file:await handle.getFile()});
  return files;
}
async function init(){
  if(!navigator.storage?.getDirectory||!self.CompressionStream)throw Error('This WebView/browser needs an update to build local HD packs.');
  root=await (await navigator.storage.getDirectory()).getDirectoryHandle('re1-hd-v1',{create:true});
  active=await state('readonly');
  if(active){
    try{activeDir=await root.getDirectoryHandle(active.id);const manifest=JSON.parse(await (await (await activeDir.getFileHandle('manifest.json')).getFile()).text());if(![1,2].includes(manifest.version)||manifest.source!==active.source)throw Error('Invalid HD manifest');active={...active,...manifest};}
    catch(_){active=null;activeDir=null;await state('readwrite',null);}
  }
  return active;
}
async function exclusive(task){
  if(!navigator.locks?.request)throw Error('Update your WebView/browser to enable HD pack storage locks.');
  return navigator.locks.request('re1-hd-library',{ifAvailable:true},lock=>{if(!lock)throw Error('Close the other game/HD pack tab first.');return task();});
}
async function cleanup(keep){for await(const [name,handle] of root.entries())if(handle.kind==='directory'&&name!==keep)await root.removeEntry(name,{recursive:true});}
async function encode(rgba,w,h,originalW,originalH,scaler,pixels){
  let body;
  const rawLength=w*h*4;
  if(scaler){
    const capacity=rawLength+65536,output=scaler._malloc(capacity);if(!output)throw Error('Not enough memory to compress this image');
    try{const length=scaler._re1_hd_compress(pixels,rawLength,output,capacity);if(!length)throw Error('HD image compression failed');body=scaler.HEAPU8.slice(output,output+length);}
    finally{scaler._free(output);}
  }else body=new Uint8Array(await new Response(new Blob([rgba]).stream().pipeThrough(new CompressionStream('deflate'))).arrayBuffer());
  const bytes=new Uint8Array(32+body.length),v=new DataView(bytes.buffer);
  bytes.set([82,49,72,68]);[2,originalW,originalH,w,h,rawLength,0].forEach((value,i)=>v.setUint32(4+i*4,value,true));bytes.set(body,32);return bytes;
}
async function readSource(entry){
  if(entry.file)return new Uint8Array(await entry.file.arrayBuffer());
  return new Uint8Array(await request('read',{path:entry.path,size:entry.size}));
}
function imageWork(w,h){return (w+12*Math.ceil(w/48))*(h+12*Math.ceil(h/48));}
async function build(message){
  await exclusive(async()=>{
    const started=performance.now();
    await init();await cleanup(active?.id);
    const id='pack-'+Date.now()+'-'+Math.random().toString(16).slice(2),dir=await root.getDirectoryHandle(id,{create:true});
    const seen=new Set(),seenBlocks=new Set(),scaler=await getScaler(),plans=[];
    const sharpness=[0,.7,1.1].includes(message.sharpness)?message.sharpness:.7;
    let count=0,bytes=0,checked=0,unsupported=0,committed=false,archive=null,totalWork=0,doneWork=0,totalImages=0;
    const failed=[],records=[],manifest=message.files.filter(f=>/\.(pak|tim|ivm|emd|emw|rdt|dor|etm|ptc|pix)$/i.test(f.path));
    function keyOf(image){
      const input=scaler._malloc(image.rgba.length);if(!input)throw Error('Not enough memory to inspect this image');
      try{scaler.HEAPU8.set(image.rgba,input);return scaler.UTF8ToString(scaler._re1_hd_key(input,image.w,image.h));}
      finally{scaler._free(input);}
    }
    const progress=(phase,path)=>postMessage({type:'progress',phase,checked,total:manifest.length,count,totalImages,bytes,doneWork,totalWork,path});
    try{
      // Inventory first: count the actual, deduplicated neural workload. File
      // count alone is a poor ETA because one container may hold many images.
      for(const entry of manifest){
        progress('scan',entry.path);
        if(entry.size>core.MAX_BYTES){unsupported++;checked++;continue;}
        const data=await readSource(entry);if(data.length!==entry.size)throw Error('Truncated game file: '+entry.path);
        const keys=new Set();let found=0;
        try{
          for(const image of core.images(data,entry.path,seenBlocks)){
            found++;if(image.w<4||image.h<4)continue;
            const key=keyOf(image);if(seen.has(key))continue;
            seen.add(key);keys.add(key);totalWork+=imageWork(image.w,image.h);totalImages++;
          }
        }catch(error){throw Error(entry.path+': '+error.message);}
        if(keys.size)plans.push({entry,keys});if(!found)unsupported++;checked++;
      }
      if(!totalImages)throw Error('No supported still images found in the selected game files.');
      seenBlocks.clear();checked=0;
      archive=await (await dir.getFileHandle('images.bin',{create:true})).createSyncAccessHandle();
      for(const {entry,keys} of plans){
        progress('build',entry.path);
        const data=await readSource(entry);if(data.length!==entry.size)throw Error('Game files changed during HD installation. Select the folder again.');
        try{
          // Decode again instead of retaining all original images in RAM.
          for(const image of core.images(data,entry.path,seenBlocks)){
            if(image.w<4||image.h<4)continue;
            const key=keyOf(image);if(!keys.delete(key))continue;
            const input=scaler._malloc(image.rgba.length),[w,h]=core.dimensions(image.w,image.h),output=scaler._malloc(w*h*4);
            if(!input||!output){if(input)scaler._free(input);if(output)scaler._free(output);throw Error('Not enough memory to upscale this image');}
            let packed;
            try{
              scaler.HEAPU8.set(image.rgba,input);
              if(!scaler._re1_hd_resize(input,image.w,image.h,output,sharpness))throw Error('Could not upscale image');
              packed=await encode(null,w,h,image.w,image.h,scaler,output);
            }finally{scaler._free(output);scaler._free(input);}
            if(archive.write(packed,{at:bytes})!==packed.length)throw Error('HD archive write was incomplete');
            records.push({key,offset:bytes,size:packed.length,path:entry.path});bytes+=packed.length;count++;doneWork+=imageWork(image.w,image.h);
            if(count%20===0){const estimate=await navigator.storage.estimate();if(estimate.quota-estimate.usage<32*1024*1024)throw Error('Not enough storage to finish the HD pack. Free space and try again.');}
            progress('build',entry.path);
          }
        }catch(error){throw Error(entry.path+': '+error.message);}
        if(keys.size)throw Error('Game images changed during HD installation. Select the folder again.');
        checked++;
      }
      progress('finish','Saving the completed pack');archive.flush();archive.close();archive=null;
      const meta={version:2,id,source:message.source,count,bytes,checked:manifest.length,unsupported,failed,records,method:'FSRCNN-x4',sharpness,buildMs:Math.round(performance.now()-started),scene:'1440x1080'};
      await write(dir,'manifest.json',new TextEncoder().encode(JSON.stringify(meta)));
      await state('readwrite',meta);active=meta;activeDir=dir;committed=true;
      await cleanup(id).catch(()=>{});
      imageFiles=null;prefetchedRooms.clear();postMessage({type:'installed',meta,files:await list(dir)});
    }catch(error){if(archive)archive.close();if(!committed)await root.removeEntry(id,{recursive:true}).catch(()=>{});throw error;}
    finally{scalerReady=null;}
  });
}
// All storage reads and DEFLATE decoding happen on this worker, never in a
// camera-switch upload. Prefetch adjacent views of the same room when known.
let imageFiles,loads=Promise.resolve();const prefetchedRooms=new Set();
async function inflate(bytes,size,header){
  const reader=new Blob([bytes]).stream().pipeThrough(new DecompressionStream('deflate')).getReader();
  const output=new Uint8Array(32+size);output.set(header);let at=0;
  try{
    for(;;){
      const {done,value}=await reader.read();if(done)break;
      if(at+value.length>size){await reader.cancel();throw Error('AI image exceeds its declared size');}
      output.set(value,32+at);at+=value.length;
    }
    if(at!==size)throw Error('Truncated AI image payload');return output;
  }finally{reader.releaseLock();}
}
async function loadImages(keys){
  if(!activeDir||active?.version!==2)return;
  if(!imageFiles)imageFiles=new Map((await list(activeDir)).map(e=>[e.key,e.file]));
  const requested=new Set(keys),prefetch=new Set();
  for(const key of keys){
    const record=active.records.find(r=>r.key===key),match=record?.path?.match(/^(.*\/RC[0-9a-f]{3})[0-9a-f]\.pak$/i);
    if(match&&!prefetchedRooms.has(match[1])){prefetchedRooms.add(match[1]);for(const other of active.records)if(other.path?.slice(0,-5).toLowerCase()===match[1].toLowerCase())prefetch.add(other.key);}
  }
  // The requested camera is also in its room's prefetch set. Decode and
  // transfer it once; a second reply needlessly copies another HD frame.
  for(const key of new Set([...requested,...prefetch])){
    if(!imageFiles.has(key))continue;
    try{
      const bytes=new Uint8Array(await imageFiles.get(key).arrayBuffer()),v=new DataView(bytes.buffer);
      if(bytes.length<32||v.getUint32(0,true)!==0x44483152||v.getUint32(4,true)!==2)throw Error('Invalid AI image header');
      const w=v.getUint32(16,true),h=v.getUint32(20,true),size=w*h*4;
      if(!w||!h||w>2048||h>2048||size>core.MAX_PIXELS*4||v.getUint32(24,true)!==size||v.getUint32(28,true))throw Error('Invalid AI image size');
      const originalW=v.getUint32(8,true),originalH=v.getUint32(12,true),target=core.dimensions(originalW,originalH);
      if(!key.endsWith('_'+originalW+'x'+originalH)||w!==target[0]||h!==target[1])throw Error('AI image dimensions do not match the original');
      const raw=await inflate(bytes.subarray(32),size,bytes.subarray(0,32));
      postMessage({type:'pixels',key,raw:raw.buffer,prefetch:!requested.has(key)},[raw.buffer]);
    }catch(error){postMessage({type:'load-error',key,message:String(error.message||error)});}
  }
}
onmessage=async event=>{
  const m=event.data;
  if(m.type==='reply'){
    const reply=replies.get(m.id);if(reply){replies.delete(m.id);m.error?reply.reject(Error(m.error)):reply.resolve(m.bytes);}return;
  }
  try{
    if(m.type==='init'){
      scalerBinary=m.scalerWasm;
      await init();
      // A terminated worker releases its exclusive lock. Reclaim abandoned
      // staging files after cancellation/reload, without touching a live build.
      if(navigator.locks?.request)await navigator.locks.request('re1-hd-library',{ifAvailable:true},lock=>lock?cleanup(active?.id):undefined);
      const files=activeDir?await list(activeDir):[];
      postMessage({type:'ready',meta:active?{...active,count:files.length,bytes:files.reduce((sum,e)=>sum+e.file.size,0)}:null,files});
    }
    else if(m.type==='build')await build(m);
    else if(m.type==='remove')await exclusive(async()=>{await init();await state('readwrite',null);active=null;activeDir=null;await cleanup(null);postMessage({type:'removed'});});
    else if(m.type==='load'){loads=loads.then(()=>loadImages(m.keys)).catch(error=>postMessage({type:'load-error',message:String(error)}));}

  }catch(error){postMessage({type:'error',message:String(error.message||error)});}
};
