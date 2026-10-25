// Shared Android/browser HD pack launcher. All generated images stay in OPFS.
(function(){
  'use strict';
  const $=id=>document.getElementById(id),install=$('hd-install'),cancel=$('hd-cancel'),remove=$('hd-remove'),toggle=$('hd-enabled'),status=$('hd-status'),progress=$('hd-progress'),timer=$('hd-time'),sharpness=$('hd-sharpness');
  let worker,meta=null,files=new Map(),busy=false,initialized=false,error=false,game=false,source='',sourceFiles=[],holdLock;
  const decoded=new Map(),pending=new Set(),failed=new Set();let decodedBytes=0;
  const MAX_DECODED=32*1024*1024;
  let buildClock=null;
  function updateTime(){
    timer.hidden=!buildClock&&!meta?.buildMs;
    if(!timer.hidden)timer.textContent=buildClock?buildClock.text(performance.now()):'Built in '+RE1HDTime.duration(meta.buildMs);
  }
  const sharpLabel=value=>value===1.1?'Sharp':value===.7?'Crisp':'Natural';
  let resolveInit;let initDone=new Promise(resolve=>{resolveInit=resolve;});
  const directFile=location.protocol==='file:';
  const supported=!directFile&&!!(navigator.storage?.getDirectory&&window.Worker);
  const api=window.RE1HDPack={
    active:false,hits:0,misses:0,
    busy:()=>busy,
    size(key){return files.get(key)?.size||0;},
    async read(key,offset,length){
      const file=files.get(key);if(!file||offset<0||length<=0||length>262144||offset+length>file.size)throw Error('Invalid HD image read');
      return new Uint8Array(await file.slice(offset,offset+length).arrayBuffer());
    },
    pixels(key){return decoded.get(key)?.byteLength||0;},
    take(key,dest){
      const raw=decoded.get(key);if(!raw||dest.length!==raw.length)return false;
      dest.set(raw);decoded.delete(key);decodedBytes-=raw.length;return true;
    },
    request(key){
      if(!worker||!initialized||!files.has(key)||pending.has(key)||decoded.has(key)||failed.has(key))return;
      pending.add(key);worker.postMessage({type:'load',keys:[key]});
    },
    async prepare(){
      if(busy)throw Error('Wait for the HD pack build to finish or cancel it first.');
      if(supported)await initDone;
      await refreshSource();api.active=!!(meta?.version===2&&source===meta.source&&toggle.checked);
      if(api.active){
        // Keep generated File snapshots stable while this game is running.
        await new Promise((resolve,reject)=>navigator.locks.request('re1-hd-library',{mode:'shared',ifAvailable:true},lock=>{
          if(!lock){reject(Error('Wait for the other HD pack build to finish.'));return;}
          return new Promise(done=>{holdLock=done;resolve();});
        }));
        game=true;render();
        // Prepare startup fonts while still in the launcher. This keeps short
        // boot screens from flashing low-resolution assets before the first
        // game frame and gives the worker a head start without blocking input.
        const startup=meta.records.filter(r=>/(?:^|\/)Data\/(?:fontus|font03t|optkey03)\.tim$/i.test(r.path||'')).slice(0,8);
        for(const image of startup)api.request(image.key);
        const deadline=performance.now()+10000;
        while(startup.some(image=>pending.has(image.key))&&performance.now()<deadline)await new Promise(resolve=>setTimeout(resolve,25));
      }
      game=true;render();return api.active;
    }
  };
  function render(){
    install.textContent=meta?'Reinstall HD pack':'Install HD pack';
    sharpness.disabled=busy||game;
    install.disabled=busy||game||!initialized||!sourceFiles.length;cancel.hidden=!busy;
    remove.hidden=!meta;remove.disabled=!initialized||busy||game;toggle.disabled=!initialized||busy||game||!meta||meta.version!==2||source!==meta.source;
    progress.hidden=!busy;
    if(!busy&&!error)status.textContent=meta?(meta.version!==2?'A previous 720p pack is installed. Reinstall for AI 1080p.':source&&meta.source!==source?'HD pack belongs to a different game folder. Build a pack for this folder.':meta.count+' prepared images · '+(meta.bytes/1048576).toFixed(0)+' MB · '+(toggle.checked?'enabled':'disabled')+'. FSRCNN AI · 1080p · '+sharpLabel(meta.sharpness)+'.'+((meta.sharpness||0)!==Number(sharpness.value)?' Reinstall to apply '+sharpLabel(Number(sharpness.value))+' sharpness.':'')):'Build a local AI 1080p pack from your game files. Original files stay intact.';
    updateTime();

    window.dispatchEvent(new Event('re1-hd-changed'));
  }
  async function refreshSource(){
    const bridge=window.RE1BrowserStorage||window.AndroidLauncher;
    if(!bridge?.ready()){sourceFiles=[];source='';render();return;}
    let selected;
    if(window.RE1BrowserStorage)selected=window.RE1BrowserStorage.hdFiles();
    else{const response=await fetch('/manifest');if(!response.ok)throw Error('Select the game folder again.');selected=await response.json();}
    const signature=JSON.stringify(selected.map(f=>[f.path,f.size]).sort((a,b)=>a[0].localeCompare(b[0])));
    source=Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',new TextEncoder().encode(signature))),v=>v.toString(16).padStart(2,'0')).join('');
    sourceFiles=selected;render();
  }
  async function start(){
    let workerURL,scalerWasm;
    try{
      if(window.AndroidLauncher){
        // Route app-asset reads through the WebView's main-frame loader. The
        // dedicated worker then needs no DNS/network interception at all.
        const names=['hd-core.js','hd-resize.js','hd-worker.js','hd-resize.wasm'];
        const assets=await Promise.all(names.map(async name=>{
          const response=await fetch(name);if(!response.ok)throw Error('Missing HD builder file: '+name);
          return name.endsWith('.wasm')?response.arrayBuffer():response.text();
        }));
        scalerWasm=assets[3];
        const source=assets[0]+'\n'+assets[1]+'\n'+assets[2].replace("importScripts('hd-core.js');",'');
        workerURL=URL.createObjectURL(new Blob([source],{type:'application/javascript'}));
      }
      worker=new Worker(workerURL||'hd-worker.js');
    }catch(e){if(workerURL)URL.revokeObjectURL(workerURL);busy=false;error=true;initialized=false;resolveInit();status.textContent='Could not start HD pack builder: '+e.message;render();return;}
    const currentWorker=worker;
    currentWorker.onmessage=async event=>{
      if(currentWorker!==worker)return;
      const m=event.data;
      if(m.type==='read'){
        try{
          const bytes=new Uint8Array(m.size);
          // Reads are chunked and yielded so cancel/settings stay responsive.
          for(let offset=0;offset<m.size;offset+=262144){
            if(currentWorker!==worker)return;
            const count=Math.min(262144,m.size-offset),reply=window.AndroidLauncher.readFileChunk(m.path,offset,count);
            if(!reply.startsWith('ok:'))throw Error(reply.slice(6));const raw=atob(reply.slice(3));if(raw.length!==count)throw Error('Truncated game file');
            for(let i=0;i<count;i++)bytes[offset+i]=raw.charCodeAt(i)&255;
            await new Promise(resolve=>setTimeout(resolve,0));
          }
          if(currentWorker===worker)currentWorker.postMessage({type:'reply',id:m.id,bytes:bytes.buffer},[bytes.buffer]);
        }catch(e){if(currentWorker===worker)currentWorker.postMessage({type:'reply',id:m.id,error:String(e)});}return;
      }
      if(m.type==='progress'){
        buildClock?.update(m,performance.now());updateTime();
        progress.max=m.phase==='scan'?m.total:m.totalImages||1;progress.value=m.phase==='scan'?m.checked:m.count;
        status.textContent=(m.phase==='scan'?'Scanning images: '+m.checked+'/'+m.total+' files':m.phase==='finish'?'Saving HD pack…':'Building HD pack: '+m.count+'/'+m.totalImages+' images · '+(m.bytes/1048576).toFixed(0)+' MB')+'\n'+m.path;return;
      }
      if(m.type==='pixels'){
        pending.delete(m.key);
        const raw=new Uint8Array(m.raw),old=decoded.get(m.key);if(old){decodedBytes-=old.length;decoded.delete(m.key);}
        // Protect requested views from speculative prefetch eviction.
        if(m.prefetch&&decodedBytes+raw.length>MAX_DECODED)return;
        while(decodedBytes+raw.length>MAX_DECODED&&decoded.size){const [key,bytes]=decoded.entries().next().value;decoded.delete(key);decodedBytes-=bytes.length;}
        decoded.set(m.key,raw);decodedBytes+=raw.length;return;
      }
      if(m.type==='load-error'){pending.delete(m.key);failed.add(m.key);window.RE1Diag?.log('HD image: '+m.message);return;}
      if(m.type==='ready'||m.type==='installed'){
        meta=m.meta;buildClock=null;decoded.clear();decodedBytes=0;pending.clear();failed.clear();files=new Map(m.files.map(f=>[f.key,f.file]));initialized=true;busy=false;error=false;
        if(workerURL){URL.revokeObjectURL(workerURL);workerURL=null;}
        resolveInit();
        if(m.type==='installed'){toggle.checked=true;localStorage.setItem('re1.hd.enabled','true');}
      }else if(m.type==='removed'){meta=null;buildClock=null;files.clear();decoded.clear();decodedBytes=0;pending.clear();failed.clear();busy=false;error=false;}
      else if(m.type==='error'){
        if(game){window.RE1Diag?.log('HD worker: '+m.message);return;}
        busy=false;buildClock=null;updateTime();sharpness.disabled=false;error=true;initialized=true;resolveInit();status.textContent='HD pack: '+m.message+' Your original files and previous pack are unchanged.';
        // Permit retry after a quota/write failure.
        install.disabled=false;cancel.hidden=true;progress.hidden=true;remove.disabled=false;return;
      }
      render();
    };
    worker.onerror=event=>{event.preventDefault();if(workerURL)URL.revokeObjectURL(workerURL);busy=false;buildClock=null;error=true;initialized=false;resolveInit();status.textContent='Could not start HD pack builder: '+event.message;render();};
    worker.postMessage({type:'init',scalerWasm},scalerWasm?[scalerWasm]:[]);
  }
  toggle.checked=localStorage.getItem('re1.hd.enabled')!=='false';
  sharpness.value=['0','0.7','1.1'].includes(localStorage.getItem('re1.hd.sharpness'))?localStorage.getItem('re1.hd.sharpness'):'0.7';
  sharpness.onchange=()=>{localStorage.setItem('re1.hd.sharpness',sharpness.value);render();};
  toggle.onchange=()=>{localStorage.setItem('re1.hd.enabled',String(toggle.checked));render();};
  install.onclick=async()=>{
    if(busy||game)return;
    try{
      error=false;await refreshSource();if(!sourceFiles.length)throw Error('Select your game folder first.');
      busy=true;buildClock=new RE1HDTime.BuildClock(performance.now());render();status.textContent='Preparing HD pack…';progress.removeAttribute('value');
      $('hd-pack').open=true;
      navigator.storage?.persist?.().catch(()=>{});
      worker.postMessage({type:'build',files:sourceFiles,source,sharpness:Number(sharpness.value)});
    }catch(e){busy=false;buildClock=null;error=true;status.textContent=String(e.message||e);render();}
  };
  cancel.onclick=()=>{
    worker.terminate();busy=false;buildClock=null;initialized=false;error=false;initDone=new Promise(resolve=>{resolveInit=resolve;});start();render();
  };
  remove.onclick=()=>{if(!busy&&!game){busy=true;render();worker.postMessage({type:'remove'});}};
  window.addEventListener('re1-storage-changed',()=>{if(!busy&&!game)refreshSource().catch(e=>{status.textContent=e.message;});});
  // Native indexing finishes asynchronously without a DOM event.
  let wasReady=false;
  setInterval(()=>{if(buildClock)updateTime();},1000);
  setInterval(()=>{if(busy||game)return;const now=!!(window.RE1BrowserStorage||window.AndroidLauncher)?.ready();if(now!==wasReady){wasReady=now;refreshSource().catch(e=>{status.textContent=e.message;});}},700);
  window.addEventListener('pagehide',()=>{holdLock?.();worker?.terminate();});
  if(supported){start();refreshSource().catch(()=>{});}else{error=true;status.textContent=directFile?'HD packs need browser storage on localhost or HTTPS. Run the included serve.py to use HD packs. Normal gameplay works when opening index.html directly.':'Update Android System WebView or your browser to build local HD packs.';resolveInit();render();}
})();
