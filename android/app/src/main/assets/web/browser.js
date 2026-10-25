// Loaded only by the standalone browser package; Android keeps its native bridge.
(function(){
  'use strict';
  const $=id=>document.getElementById(id),filesAPI=RE1BrowserFiles;
  let selected=null,lookup=new Map(),busy=true,message='Restoring imported game files…';
  let loadingVisible=localStorage.getItem('re1.showLoadingFiles')!=='false',loadingTimer;
  const remember=$('browser-remember'),picker=$('browser-folder-input');
  remember.checked=localStorage.getItem('re1.browser.remember')!=='false';
  const touch=navigator.maxTouchPoints>0||matchMedia('(pointer:coarse)').matches;
  if(!localStorage.getItem('re1.controls'))localStorage.setItem('re1.controls',JSON.stringify({hidden:!touch}));
  $('browser-auto-fullscreen').checked=localStorage.getItem('re1.browser.fullscreen')===null?touch:localStorage.getItem('re1.browser.fullscreen')==='true';
  $('browser-auto-fullscreen').onchange=()=>localStorage.setItem('re1.browser.fullscreen',String($('browser-auto-fullscreen').checked));
  $('controller-status').textContent='Pair a Bluetooth/USB controller with this device. PC: arrows, C/Enter, V, X, Z; Q turns, F2 opens settings.';
  function changed(){
    $('folder').disabled=busy;$('browser-forget').disabled=busy||!selected;
    window.dispatchEvent(new Event('re1-storage-changed'));
  }
  function readyMessage(note=''){
    const mb=(selected.total/1048576).toFixed(0);
    return selected.files.length+' local files · '+mb+' MB ready'+(note?' · '+note:'');
  }
  function use(index){selected=index;lookup=new Map(index.files.map(e=>[e.path,e.file]));}
  async function database(mode,operation){
    const db=await new Promise((resolve,reject)=>{
      const request=indexedDB.open('re1-browser-files',1);
      request.onupgradeneeded=()=>request.result.createObjectStore('library');
      request.onsuccess=()=>resolve(request.result);request.onerror=()=>reject(request.error);
    });
    return new Promise((resolve,reject)=>{
      let value;const tx=db.transaction('library',mode);
      tx.oncomplete=()=>{db.close();resolve(value);};
      tx.onabort=tx.onerror=()=>{db.close();reject(tx.error||Error('Browser storage failed'));};
      try{const request=operation(tx.objectStore('library'));request.onsuccess=()=>{value=request.result;};}
      catch(error){tx.abort();db.close();reject(error);}
    });
  }
  async function persist(){
    if(remember.checked){
      message='Remembering game files in this browser…';changed();
      // A stored File can retain an external OS-file reference in Chromium.
      // Stream each file into a browser-owned Blob before committing it, so
      // moving the original folder cannot invalidate the remembered library.
      // No whole-file ArrayBuffer is allocated on the JavaScript heap.
      const copies=[];
      for(const entry of selected.files)copies.push({path:entry.path,file:await new Response(entry.file.stream()).blob()});
      await database('readwrite',s=>s.put({files:copies,name:selected.name},'game'));
      message=readyMessage('remembered in this browser');
      // A denied persistence request is normal; the browser still provides
      // quota-managed storage. Saves remain in the engine's separate IDBFS DB.
      navigator.storage?.persist?.().catch(()=>{});
    }else{
      await database('readwrite',s=>s.delete('game'));
      message=readyMessage('select again after reloading');
    }
  }
  const storage=window.RE1BrowserStorage={
    engineScript:'residentevil.js',
    busy:()=>busy,
    ready:()=>!!selected&&!busy,
    status:()=>message,
    selectFolder(){if(!busy){picker.value='';picker.click();}},
    manifest(){if(!selected||busy)throw Error('Select a game folder first');return selected.files.map(e=>({path:e.path,size:e.file.size}));},
    read(path,offset,length){return filesAPI.readFile(lookup.get(path),offset,length);},
    showLoadingFilenames(value){loadingVisible=!!value;if(!loadingVisible)$('browser-loading-file').hidden=true;},
    io(path){
      if(!loadingVisible)return;
      $('browser-loading-file').textContent=path;$('browser-loading-file').hidden=false;
      clearTimeout(loadingTimer);loadingTimer=setTimeout(()=>{$('browser-loading-file').hidden=true;},1500);
    },
    requestFullscreen(){
      if(document.fullscreenElement){document.exitFullscreen?.().catch(()=>{});return;}
      const request=document.documentElement.requestFullscreen?.({navigationUI:'hide'});
      request?.then(()=>screen.orientation?.lock?.('landscape').catch(()=>{})).catch(()=>{});
    },
    launch(){if($('browser-auto-fullscreen').checked&&!document.fullscreenElement)storage.requestFullscreen();}
  };
  $('browser-fullscreen').onclick=()=>storage.requestFullscreen();
  picker.onchange=async()=>{
    if(!picker.files.length)return;
    busy=true;message='Checking selected local files…';changed();
    try{
      const index=filesAPI.indexFiles(picker.files);
      // Check the browser's local read capability before starting the engine.
      const font=index.files.find(e=>e.path.toLowerCase()==='data/fontus.tim');filesAPI.readFile(font.file,0,8);
      use(index);
      try{await persist();}catch(error){message=readyMessage('not remembered: '+error.message);}
    }catch(error){selected=null;lookup.clear();message=String(error.message||error);}
    finally{busy=false;changed();}
  };
  remember.onchange=async()=>{
    localStorage.setItem('re1.browser.remember',String(remember.checked));
    if(!selected||busy)return;
    busy=true;changed();
    try{await persist();}catch(error){message=readyMessage('storage failed: '+error.message);}
    finally{busy=false;changed();}
  };
  $('browser-forget').onclick=async()=>{
    busy=true;changed();
    try{await database('readwrite',s=>s.delete('game'));selected=null;lookup.clear();message='Imported files removed. Saves and control settings are kept.';}
    catch(error){message='Could not remove imported files: '+error.message;}
    finally{busy=false;changed();}
  };
  (async()=>{
    try{
      if(remember.checked){
        const saved=await database('readonly',s=>s.get('game'));
        if(saved){use(filesAPI.indexFiles(saved.files));message=readyMessage('restored in this browser');}
      }
      if(!selected)message='Select your installed/extracted USA GOG game folder. Files stay local.';
    }catch(error){message='Select game files again: '+error.message;}
    finally{busy=false;changed();}
  })();
})();
