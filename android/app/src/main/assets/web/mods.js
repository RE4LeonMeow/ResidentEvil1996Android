// Android-only launcher mod manager. All archive/image work stays on a worker.
(function(){
  'use strict';
  const $=id=>document.getElementById(id),native=()=>window.AndroidLauncher;
  const supported=typeof native()?.pickMod==='function'&&!!navigator.storage?.getDirectory&&!!window.Worker;
  let worker,starting,library=[],busy=false,game=false,initialized=false,source='',selected=false,job=null,committing=false;
  let serial=0,nativeSerial=0,workerURL,libraryLock;
  let detailsRequest=null;
  const tasks=new Map(),nativeTasks=new Map(),decoded=new Map(),pending=new Set(),failed=new Set();
  let decodedBytes=0,activeTextures=new Map(),activeFiles=new Map();
  let localeBytes=null;
  const hintedRooms=new Set();
  const budget=64*1024*1024;
  const api=window.RE1Mods={
    active:false,outputHD:false,hits:0,misses:0,iconUploads:0,effectUploads:0,busy:()=>busy,
    localization:()=>localeBytes,
    allowed:()=>window.re1ModsAllowed?.()!==false,
    nativeResult(id,result){
      const task=nativeTasks.get(id);if(!task)return;nativeTasks.delete(id);
      result.error?task.reject(Error(result.error)):task.resolve(result);
    },
    nativeProgress(done,total,phase){
      if(!busy)return;
      $('mods-status').textContent=phase+' · '+(done/1048576).toFixed(1)+(total>0?' / '+(total/1048576).toFixed(1):'')+' MB';
      const progress=$('mods-progress');if(total>0){progress.max=total;progress.value=done;}else progress.removeAttribute('value');
    },
    close(){
      if($('mod-details').open){finishDetails(null);return true;}
      if($('mods-help').open){$('mods-help').close();return true;}
      if(!$('mods').open)return false;
      if(busy){api.cancel();return true;}
      $('mods').close();window.re1ModsDialogChanged?.();return true;
    },
    cancel(){
      if(!busy||committing||!job||job.cancelled)return;
      job.cancelled=true;
      if($('mod-details').open)finishDetails(null);
      // Cancellation must reach the native ZIP copy immediately, including
      // while pickMod is still waiting for a multi-gigabyte document provider.
      job.cancelWait=callNative('cancelMod').catch(()=>{});
      worker?.terminate();worker=null;
      for(const task of tasks.values())task.reject(Error('Import cancelled.'));tasks.clear();
      $('mods-status').textContent='Cancelling import…';$('mods-cancel').disabled=true;
    },
    report(){return 'Mods: '+library.filter(m=>m.enabled&&m.source===source&&!m.error).length+' enabled | Textures: '+activeTextures.size+' | Data overrides: '+activeFiles.size+'\nHD mod uploads: item icons '+api.iconUploads+' | effect pages '+api.effectUploads;},
    graphicsLimit(size){
      if([...activeTextures.values()].some(record=>record.width>size||record.height>size))
        throw Error('An enabled mod uses textures larger than this renderer supports ('+size+' pixels). Disable that mod or try the other renderer.');
    },
    roomHint(path){
      if(!game||!api.active)return;
      const match=path.match(/^Stage([1-7])\/ROOM([1-7])([0-9a-f]{2})[01]\.rdt$/i);if(!match)return;
      const stage=Number(match[1])-1,room=parseInt(match[3],16),tag=match[1]+match[3].toLowerCase();
      if(hintedRooms.has(tag))return;hintedRooms.add(tag);
      const background=('stage'+match[1]+'/rc'+match[2]+match[3]).toLowerCase();
      const mask='objspr/osp0'+(stage>5?String.fromCharCode(stage+0x2b):String(stage))+String(room).padStart(2,'0');
      for(const [key,record] of activeTextures){const source=(record.path||'').toLowerCase();if(source.startsWith(background)||source.startsWith(mask))window.RE1HDPack?.request(key);}
    },
    manifest(base){
      if(!activeFiles.size)return base;
      const files=base.map(file=>{const mod=activeFiles.get(file.path.toLowerCase());return mod?{path:file.path,size:mod.size}:file;});
      const known=new Set(files.map(f=>f.path.toLowerCase()));
      for(const [key,mod] of activeFiles)if(!known.has(key))files.push({path:mod.target,size:mod.size});
      return files;
    },
    readFile(path,offset,length){
      const file=activeFiles.get(path.toLowerCase());if(!file)return null;
      return chunk(native().readModChunk(file.id,file.slot,offset,length),length);
    },
    movie(path){
      const key=path.replace(/\\/g,'/').replace(/^\/game\/usa\//i,'').toLowerCase(),file=activeFiles.get(key);
      return file&&/^movie\/[^/]+\.mp4$/.test(key)?'/mod-video/'+file.id+'/'+file.slot+'.mp4':null;
    },
    movieBlob(path){
      const key=path.replace(/\\/g,'/').replace(/^\/game\/usa\//i,'').toLowerCase(),file=activeFiles.get(key);
      return file&&/^movie\/[^/]+\.mp4$/.test(key)?workerCall('movie',{pack:file.id,slot:file.slot}):null;
    },
    cancelMovie(){worker?.postMessage({type:'movie-cancel'});},
    async prepare(){
      if(!supported)return false;
      if(busy)throw Error('Wait for the mod installation to finish.');
      await starting;await refreshSource();
      const active=RE1ModCore.active(library,source);activeTextures=active.textures;activeFiles=active.files;
      // Native-size translated font/document pages do not require the much
      // larger HD framebuffer. SHDP and the existing HD packs keep theirs.
      api.outputHD=[...activeTextures.values()].some(record=>!record.nativeSize);
      localeBytes=null;
      const translation=library.find(mod=>mod.enabled&&mod.source===source&&!mod.error&&mod.localeSize);
      if(translation){
        if(activeTextures.get(translation.localeFontKey)?.id!==translation.id)throw Error('Move '+translation.name+' above other font/UI packs in Mods, so its custom font matches its translated text.');
        localeBytes=new Uint8Array(await workerCall('locale',{pack:translation.id}));
        window.RE1Diag?.log('Translation: '+translation.name+' | '+translation.localeCount+' strings');
      }
      if(window.RE1HDPack)window.RE1HDPack.regionTiles=[...activeTextures.keys()].some(key=>key.endsWith('_40x30'));
      if(activeTextures.size||activeFiles.size){
        await new Promise((resolve,reject)=>navigator.locks.request('re1-mod-library',{mode:'shared',ifAvailable:true},lock=>{
          if(!lock){reject(Error('Finish the mod installation in the other window first.'));return;}
          return new Promise(done=>{libraryLock=done;resolve();});
        }));
        for(const file of activeFiles.values())if(file.size)api.readFile(file.target,0,1);
      }
      game=true;api.active=activeTextures.size>0;render();
      const boot=[...activeTextures].filter(([,record])=>/^Data\/(?:fontus|font03t|optkey03)\.tim$/i.test(record.path||'')).slice(0,8);
      for(const [key] of boot)window.RE1HDPack?.request(key);
      const deadline=performance.now()+5000;
      while(boot.some(([key])=>pending.has(key))&&performance.now()<deadline)await new Promise(resolve=>setTimeout(resolve,20));
      return api.active;
    }
  };
  function chunk(reply,length){
    if(typeof reply!=='string'||!reply.startsWith('ok:'))throw Error(typeof reply==='string'?reply.slice(6):'Could not read mod/game data.');
    const raw=atob(reply.slice(3));if(raw.length!==length)throw Error('Incomplete mod/game data.');
    return Uint8Array.from(raw,c=>c.charCodeAt(0));
  }
  function callNative(method,...args){
    return new Promise((resolve,reject)=>{
      const id='mods-'+(++nativeSerial);nativeTasks.set(id,{resolve,reject});
      try{native()[method](id,...args);}catch(error){nativeTasks.delete(id);reject(error);}
    });
  }
  function workerCall(type,data={}){
    return new Promise((resolve,reject)=>{
      if(!worker){reject(Error('Mod worker is unavailable.'));return;}
      const id=++serial;tasks.set(id,{resolve,reject});worker.postMessage({type,id,...data});
    });
  }
  async function bridge(message,currentWorker){
    let result;
    try{
      if(message.type==='movie-chunk'){
        result=chunk(native().readModChunk(message.pack,message.slot,message.offset,message.length),message.length).buffer;
      }else if(message.type==='game-manifest'){
        if(!native()?.ready())throw Error('Select your GOG game folder before installing mods.');
        const response=await fetch('/manifest');if(!response.ok)throw Error('Could not read the selected game folder.');result=await response.json();
      }else if(message.type==='native-commit')result=await callNative('finishMod',message.pack,JSON.stringify(message.slots));
      else if(message.type==='native-remove')result=await callNative('removeMod',message.pack);
      else if(message.type==='native-cleanup')result=await callNative('cleanMods',JSON.stringify(message.keep));
      else{
        const {entry}=message;if(entry.size>RE1ModCore.MAX_IMAGE_BYTES)throw Error('This texture/source is too large.');
        const bytes=new Uint8Array(entry.size);
        for(let offset=0;offset<entry.size;offset+=262144){
          if(currentWorker!==worker)return;
          const count=Math.min(262144,entry.size-offset);
          const reply=message.type==='archive-read'?native().readModChunk(message.pack,entry.slot,offset,count):native().readFileChunk(entry.path,offset,count);
          bytes.set(chunk(reply,count),offset);await new Promise(resolve=>setTimeout(resolve,0));
        }
        result=bytes.buffer;
      }
      if(currentWorker===worker)currentWorker.postMessage({type:'reply',id:message.id,result},result instanceof ArrayBuffer?[result]:[]);
    }catch(error){if(currentWorker===worker)currentWorker.postMessage({type:'reply',id:message.id,error:String(error.message||error)});}
  }
  async function start(){
    const names=['hd-core.js','mod-core.js','hd-resize.js','mods-worker.js','hd-resize.wasm','shdp-profile.json','rebirth-core.js','rebirth-plan.js'];
    const assets=await Promise.all(names.map(async name=>{
      const response=await fetch(name);if(!response.ok)throw Error('Mod manager file missing: '+name);
      return name.endsWith('.wasm')?response.arrayBuffer():response.text();
    }));
    const source=assets[0]+'\n'+assets[1]+'\n'+assets[2]+'\n'+assets[6]+'\n'+assets[7]+'\n'+assets[3].replace("importScripts('hd-core.js','mod-core.js','rebirth-core.js','rebirth-plan.js');",'');
    workerURL=URL.createObjectURL(new Blob([source],{type:'application/javascript'}));
    worker=new Worker(workerURL);const currentWorker=worker;
    worker.onmessage=event=>{
      if(currentWorker!==worker)return;const message=event.data;
      if(message.type==='result'){
        const task=tasks.get(message.id);if(!task)return;tasks.delete(message.id);
        message.error?task.reject(Error(message.error)):task.resolve(message.result);
      }else if(message.type==='movie-progress'){
        globalThis.RE1ModVideo?.progress(message.done,message.total);
      }else if(message.type==='progress'){
        if(message.phase==='Finishing installation')committing=true;
        $('mods-status').textContent=message.phase+(message.total?' · '+message.done+' / '+message.total:'')+
          (message.path?'\n'+message.path:'');
        $('mods-progress').max=Math.max(1,message.total);$('mods-progress').value=message.done;renderButtons();
      }else if(message.type==='pixels'){
        pending.delete(message.key);
        const raw=new Uint8Array(message.raw);if(message.prefetch&&decodedBytes+raw.length>budget)return;
        if(decoded.has(message.key)){decodedBytes-=decoded.get(message.key).length;decoded.delete(message.key);}
        while(decodedBytes+raw.length>budget&&decoded.size){const [key,bytes]=decoded.entries().next().value;decoded.delete(key);decodedBytes-=bytes.length;}
        decoded.set(message.key,raw);decodedBytes+=raw.length;
      }else if(message.type==='load-error'){
        pending.delete(message.key);if(message.key)failed.add(message.key);window.RE1Diag?.log('Mod texture: '+message.message);
      }else bridge(message,currentWorker);
    };
    worker.onerror=event=>{
      event.preventDefault();const error=Error('Mod worker: '+event.message);
      for(const task of tasks.values())task.reject(error);tasks.clear();window.RE1Diag?.log(error.message);
    };
    library=await workerCall('init',{scalerWasm:assets[4],profile:JSON.parse(assets[5])});initialized=true;
    URL.revokeObjectURL(workerURL);workerURL=null;await refreshSource();render();
  }
  async function refreshSource(){
    selected=!!native()?.ready();
    if(!selected){source='';renderButtons();return;}
    if($('mods-status').textContent==='Select your GOG game folder before installing a mod ZIP.')$('mods-status').textContent='Tap + to install a mod ZIP.';
    const response=await fetch('/manifest');if(!response.ok)throw Error('Select your game folder again.');
    const files=await response.json(),signature=JSON.stringify(files.map(f=>[f.path.toLowerCase(),f.size]).sort((a,b)=>a[0].localeCompare(b[0])));
    source=Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',new TextEncoder().encode(signature))),v=>v.toString(16).padStart(2,'0')).join('');render();
  }
  function notify(){window.dispatchEvent(new Event('re1-mods-changed'));}
  function renderButtons(){
    $('mods-add').disabled=!supported||!initialized||busy||game||!selected;
    $('mods-add').title=selected?'Install a mod ZIP':'Select your game folder first';
    $('mods-close').disabled=busy;$('mods-cancel').hidden=!busy;$('mods-cancel').disabled=committing||!!job?.cancelled;
    $('mods-progress').hidden=!busy;$('mods').setAttribute('aria-busy',String(busy));
  }
  function render(){
    renderButtons();const list=$('mods-list');list.replaceChildren();
    $('mods-empty').hidden=library.length>0;
    const active=RE1ModCore.active(library,source);
    for(const [index,mod] of library.entries()){
      const row=document.createElement('article');row.className='mod-row';
      const label=document.createElement('label'),toggle=document.createElement('input');toggle.type='checkbox';toggle.checked=mod.enabled;
      toggle.disabled=busy||game||!!mod.error;toggle.setAttribute('aria-label','Enable '+mod.name);
      toggle.onchange=()=>edit({pack:mod.id,action:'enabled',enabled:toggle.checked});
      const name=document.createElement('strong');name.textContent=mod.name;label.append(toggle,name);row.append(label);
      const info=document.createElement('p');info.textContent=mod.author+' · '+mod.version+' · '+(mod.imageCount??mod.records.length)+' textures · '+mod.overrides.length+' data files';row.append(info);
      if(mod.description){const description=document.createElement('p');description.textContent=mod.description;row.append(description);}
      if(mod.compatibility){
        const details=document.createElement('details'),summary=document.createElement('summary'),report=document.createElement('pre');
        summary.textContent='Compatibility report';report.textContent=compatibilityText(mod.compatibility);details.append(summary,report);row.append(details);
      }
      const status=document.createElement('p');status.className='mod-note';
      const conflicts=active.conflicts.get(mod.id)||0;
      status.textContent=mod.error||(mod.slug==='seamless-hd'&&(mod.adapterRevision||1)<2?'Reimport the full Seamless ZIP with + to add stair/mask/typewriter mappings and HD movies. Your current pack stays installed until the new import succeeds.':!source?'Select your game folder.':mod.source!==source?'Installed for a different game folder.':conflicts?conflicts+' replacement(s) are covered by a mod above this one.':mod.enabled?'Enabled for the next launch.':'Disabled.');row.append(status);
      if(mod.credits){const details=document.createElement('details'),summary=document.createElement('summary'),credits=document.createElement('pre');summary.textContent='Credits';credits.textContent=mod.credits;details.append(summary,credits);row.append(details);}
      const actions=document.createElement('div');actions.className='actions';
      const details=document.createElement('button');details.className='secondary';details.textContent='Edit details';details.disabled=busy||game;
      details.setAttribute('aria-label','Edit details for '+mod.name);
      details.onclick=async()=>{if(detailsRequest)return;const result=await showDetails(mod,'Save');if(result)await edit({pack:mod.id,action:'details',details:result});};actions.append(details);
      for(const [text,title,action] of [['↑','Move '+mod.name+' up',{action:'move',direction:-1}],['↓','Move '+mod.name+' down',{action:'move',direction:1}],['Remove','Remove '+mod.name,{action:'remove'}]]){
        const button=document.createElement('button');button.className='secondary';button.textContent=text;button.setAttribute('aria-label',title);
        button.disabled=busy||game||action.direction===-1&&index===0||action.direction===1&&index===library.length-1;
        button.onclick=()=>edit({pack:mod.id,...action});actions.append(button);
      }
      row.append(actions);list.append(row);
    }
  }
  async function edit(change){
    if(busy||game)return;busy=true;render();notify();
    try{library=await workerCall('update',change);$('mods-status').textContent=change.action==='remove'?'Mod removed.':change.action==='move'?'Mod order saved. The top mod has priority.':change.action==='details'?'Mod details saved.':'Mod preference saved.';}
    catch(error){$('mods-status').textContent=String(error.message||error);}
    finally{busy=false;render();notify();}
  }
  $('launcher-mods').onclick=()=>{
    if(!api.allowed())return;window.re1ShowDialog($('mods'));window.re1ModsDialogChanged?.();
    if(!supported)$('mods-status').textContent='Mod installation needs Android and a recent Android System WebView.';
    else if(initialized&&!selected)$('mods-status').textContent='Select your GOG game folder before installing a mod ZIP.';
    render();
  };
  function showDetails(initial,action){
    $('mod-name').value=initial.name||'';$('mod-description').value=initial.description||'';$('mod-details-save').textContent=action;
    $('mod-compatibility').textContent=initial.compatibility?compatibilityText(initial.compatibility):'';
    $('mod-name').setCustomValidity('');window.re1ShowDialog($('mod-details'),initial.compatibility?null:$('mod-name'));
    return new Promise(resolve=>{detailsRequest=resolve;});
  }
  function compatibilityText(report){
    return report.format+'\n'+report.used+' of '+report.total+' source files used.\n'+(report.warnings||[]).join('\n')+
      ((report.skipped||[]).length?'\nSkipped files:\n'+report.skipped.map(item=>item.path+' — '+item.reason).join('\n'):'');
  }
  function finishDetails(value){
    const resolve=detailsRequest;detailsRequest=null;$('mod-details').close();resolve?.(value);
  }
  $('mod-details-form').onsubmit=event=>{
    event.preventDefault();
    try{finishDetails(RE1ModCore.modDetails({name:$('mod-name').value,description:$('mod-description').value}));}
    catch(error){$('mod-name').setCustomValidity(error.message);$('mod-name').reportValidity();}
  };
  $('mod-name').oninput=()=>$('mod-name').setCustomValidity('');
  $('mod-details-cancel').onclick=()=>finishDetails(null);
  $('mod-details').addEventListener('cancel',event=>{event.preventDefault();finishDetails(null);});
  $('mods-close').onclick=()=>api.close();
  $('mods').addEventListener('cancel',event=>{event.preventDefault();api.close();});
  $('mods-help-button').onclick=()=>window.re1ShowDialog($('mods-help'));
  $('mods-help-close').onclick=()=>$('mods-help').close();
  $('mods-guide-button').onclick=()=>native()?.openExternal?.('https://github.com/RE4LeonMeow/ResidentEvil1996Android#mods-and-making-texture-packs');
  $('mods-discord').onclick=()=>native()?.openExternal?.('https://discord.gg/5PVsXAgvM2');
  $('mods-cancel').onclick=()=>api.cancel();
  $('mods-add').onclick=async()=>{
    if(busy||game||!selected||!api.allowed())return;
    if(window.RE1HDPack?.busy()){$('mods-status').textContent='Finish or cancel the HD pack build before installing a mod.';return;}
    busy=true;committing=false;job={cancelled:false};const current=job;render();notify();
    $('mods-status').textContent='Choose a mod ZIP…';$('mods-progress').removeAttribute('value');
    let pack;
    try{
      pack=await callNative('pickMod');
      if(pack.cancelled){
        if(current.cancelled){await current.cancelWait;starting=start();await starting;$('mods-status').textContent='Import cancelled. Your installed mods are unchanged.';}
        return;
      }
      if(current.cancelled)throw Error('Import cancelled.');
      const format=RE1ModCore.manifest(pack.files);let profile;
      if(format.format==='seamless'){
        const response=await fetch('shdp-profile.json');if(!response.ok)throw Error('Seamless HD compatibility data is missing.');profile=await response.json();
      }
      const info=await workerCall('inspect',{pack,profile});let details;
      if(current.cancelled)throw Error('Import cancelled.');
      if(info.needsDetails){
        details=await showDetails(info,'Install');
        if(!details){api.cancel();throw Error('Import cancelled.');}
      }
      if(current.cancelled)throw Error('Import cancelled.');
      library=await workerCall('install',{pack,profile,details});$('mods-status').textContent='Mod installed and enabled. You can launch the game now.';
    }catch(error){
      await (current.cancelWait||callNative('cancelMod').catch(()=>{}));
      worker?.terminate();worker=null;starting=start();await starting.catch(()=>{});
      $('mods-status').textContent=current.cancelled?'Import cancelled. Your installed mods are unchanged.':String(error.message||error);
    }finally{busy=false;committing=false;job=null;render();notify();}
  };
  // Merge mod replacements with the existing AI pack without enabling an AI
  // pack the player has disabled. Imported mod images have priority by key.
  const hd=window.RE1HDPack;
  if(hd&&supported){
    const fallback={prepare:hd.prepare.bind(hd),size:hd.size.bind(hd),pixels:hd.pixels.bind(hd),take:hd.take.bind(hd),request:hd.request.bind(hd),read:hd.read.bind(hd)};
    let builtinActive=false;
    hd.prepare=async()=>{builtinActive=await fallback.prepare();hd.active=builtinActive||api.active;return builtinActive||api.outputHD;};
    hd.size=key=>activeTextures.has(key)?activeTextures.get(key).size:builtinActive?fallback.size(key):0;
    hd.pixels=key=>activeTextures.has(key)?decoded.get(key)?.length||0:builtinActive?fallback.pixels(key):0;
    hd.take=(key,dest)=>{
      if(!activeTextures.has(key))return builtinActive&&fallback.take(key,dest);
      const bytes=decoded.get(key);if(!bytes||bytes.length!==dest.length)return false;
      if(key.endsWith('_40x30'))api.iconUploads++;
      if(/^effspr\//i.test(activeTextures.get(key).path||''))api.effectUploads++;
      dest.set(bytes);decoded.delete(key);decodedBytes-=bytes.length;api.hits++;return true;
    };
    hd.request=key=>{
      const texture=activeTextures.get(key);if(!texture){if(builtinActive)fallback.request(key);return;}
      if(!worker||pending.has(key)||decoded.has(key)||failed.has(key))return;
      pending.add(key);worker.postMessage({type:'load',keys:[{id:texture.id,key}]});
    };
    // The read API is used by installer diagnostics/tests before gameplay.
    // Keep the original AI-library reader available independently of activation.
    hd.read=fallback.read;
  }
  window.addEventListener('re1-storage-changed',()=>refreshSource().catch(error=>{$('mods-status').textContent=error.message;}));
  starting=supported?start().catch(error=>{$('mods-status').textContent=error.message;initialized=false;renderButtons();throw error;}):Promise.resolve();
  starting.catch(()=>{});render();
})();
