// Port-owned save backups. Uses the existing Emscripten IDBFS database;
// no engine, game-data, configuration or HD-pack changes are required.
(function(root){
  'use strict';
  const DB='/persist',STORE='FILE_DATA',PREFIX='/persist/SAVE/';
  const MAX=131072,SIZES=[0x800,0xa82],NAME=/^savedat([1-8])\.dat$/;
  const fail=message=>{throw Error(message);};
  function checksum(bytes){
    let crc=0xffffffff;
    for(const byte of bytes){crc^=byte;for(let n=0;n<8;n++)crc=(crc>>>1)^((crc&1)?0xedb88320:0);}
    return ((crc^0xffffffff)>>>0).toString(16).padStart(8,'0');
  }
  function raw(files){
    if(!files.length)fail('No saved games found. Save in the game first, then return to the launcher.');
    if(files.length>8)fail('Choose at most eight GOG save slots.');
    const names=new Set();
    return files.map(file=>{
      const name=typeof file?.name==='string'?file.name.toLowerCase():'';
      const source=file?.contents;
      // C/WASI writes use HEAP8, so IDBFS legitimately stores Int8Array as
      // well as Uint8Array. View the bytes; never sign-convert, pad or truncate.
      const contents=ArrayBuffer.isView(source)?new Uint8Array(source.buffer,source.byteOffset,source.byteLength):source instanceof ArrayBuffer?new Uint8Array(source):null;
      if(!NAME.test(name)||names.has(name)||!contents||!SIZES.includes(contents.length))fail('Cannot back up or import save slot '+(file?.name||'')+'. Expected savedat1.dat through savedat8.dat (2048 or 2690 bytes).');
      names.add(name);return {name,contents};
    });
  }
  function zip(files){
    files=raw(files);const local=[],central=[];let offset=0;
    const header=(size,signature)=>{const bytes=new Uint8Array(size);new DataView(bytes.buffer).setUint32(0,signature,true);return bytes;};
    for(const file of files){
      const name=new TextEncoder().encode(file.name),length=file.contents.length,crc=parseInt(checksum(file.contents),16);
      const entry=header(30+name.length,0x04034b50),view=new DataView(entry.buffer);
      view.setUint16(4,20,true);view.setUint16(12,0x21,true);view.setUint32(14,crc,true);
      view.setUint32(18,length,true);view.setUint32(22,length,true);view.setUint16(26,name.length,true);entry.set(name,30);
      local.push(entry,file.contents);
      const record=header(46+name.length,0x02014b50),index=new DataView(record.buffer);
      index.setUint16(4,20,true);index.setUint16(6,20,true);index.setUint16(14,0x21,true);
      index.setUint32(16,crc,true);index.setUint32(20,length,true);index.setUint32(24,length,true);
      index.setUint16(28,name.length,true);index.setUint32(42,offset,true);record.set(name,46);
      central.push(record);offset+=entry.length+length;
    }
    const size=central.reduce((n,entry)=>n+entry.length,0),end=header(22,0x06054b50),view=new DataView(end.buffer);
    view.setUint16(8,files.length,true);view.setUint16(10,files.length,true);view.setUint32(12,size,true);view.setUint32(16,offset,true);
    const bytes=new Uint8Array(offset+size+end.length);let position=0;
    for(const chunk of [...local,...central,end]){bytes.set(chunk,position);position+=chunk.length;}
    return bytes;
  }
  async function unzip(bytes){
    if(!(bytes instanceof Uint8Array)||bytes.length>MAX||bytes.length<22)fail('Choose a save ZIP smaller than 128 KiB.');
    const view=new DataView(bytes.buffer,bytes.byteOffset,bytes.byteLength),text=new TextDecoder('utf-8',{fatal:true});
    const first=Math.max(0,bytes.length-65557);let end=bytes.length-22;
    for(;end>=first;end--)if(view.getUint32(end,true)===0x06054b50&&end+22+view.getUint16(end+20,true)===bytes.length)break;
    if(end<first)fail('Invalid or incomplete save ZIP.');
    const count=view.getUint16(end+10,true),start=view.getUint32(end+16,true),size=view.getUint32(end+12,true);
    if(view.getUint16(end+4,true)||view.getUint16(end+6,true)||count!==view.getUint16(end+8,true)||!count||count>16||start+size!==end)fail('Unsupported save ZIP. Use a normal ZIP of savedat1.dat through savedat8.dat.');
    const files=[];let pos=start;
    for(let n=0;n<count;n++){
      if(pos+46>end||view.getUint32(pos,true)!==0x02014b50)fail('Damaged ZIP directory.');
      const flags=view.getUint16(pos+8,true),method=view.getUint16(pos+10,true),crc=view.getUint32(pos+16,true);
      const packed=view.getUint32(pos+20,true),length=view.getUint32(pos+24,true),nameLength=view.getUint16(pos+28,true);
      const next=pos+46+nameLength+view.getUint16(pos+30,true)+view.getUint16(pos+32,true),local=view.getUint32(pos+42,true);
      if(next>end||view.getUint16(pos+34,true)||flags&1)fail('Encrypted, split or damaged ZIPs are unsupported.');
      const path=text.decode(bytes.subarray(pos+46,pos+46+nameLength));pos=next;
      if(/^save\/$/i.test(path)&&length===0)continue;
      if(!/^(?:save\/)?savedat[1-8]\.dat$/i.test(path)||!SIZES.includes(length)||![0,8].includes(method))fail('ZIP must contain only savedat1.dat through savedat8.dat (2048 or 2690 bytes each), optionally inside SAVE/.');
      if(local+30>start||view.getUint32(local,true)!==0x04034b50||view.getUint16(local+6,true)!==flags||view.getUint16(local+8,true)!==method)fail('Damaged ZIP file header.');
      const localName=view.getUint16(local+26,true),dataStart=local+30+localName+view.getUint16(local+28,true);
      if(dataStart+packed>start||text.decode(bytes.subarray(local+30,local+30+localName))!==path)fail('Damaged ZIP file bounds.');
      const compressed=bytes.subarray(dataStart,dataStart+packed);let contents;
      if(method===0){if(packed!==length)fail('Damaged stored save file.');contents=compressed.slice();}
      else{
        if(typeof DecompressionStream!=='function')fail('Update your browser or Android System WebView to import compressed ZIPs.');
        const reader=new Blob([compressed]).stream().pipeThrough(new DecompressionStream('deflate-raw')).getReader();
        contents=new Uint8Array(length);let written=0;
        try{
          for(;;){const {value,done}=await reader.read();if(done)break;
            if(written+value.length>length){await reader.cancel();fail('ZIP expands beyond the declared save size.');}
            contents.set(value,written);written+=value.length;
          }
          if(written!==length)fail('Incomplete compressed save file.');
        }finally{reader.releaseLock();}
      }
      if(checksum(contents)!==crc.toString(16).padStart(8,'0'))fail('Damaged save data in '+path+'. Nothing was imported.');
      files.push({name:path.split('/').pop(),contents});
    }
    if(pos!==end)fail('Damaged ZIP directory size.');
    return raw(files);
  }
  async function database(mode,operation){
    const db=await new Promise((resolve,reject)=>{
      // Keep in sync with Emscripten 3.1.69 IDBFS. Existing saves are used as-is.
      const request=indexedDB.open(DB,21);let blocked=false;
      request.onupgradeneeded=()=>{
        const db=request.result;
        const store=db.objectStoreNames.contains(STORE)?request.transaction.objectStore(STORE):db.createObjectStore(STORE);
        if(!store.indexNames.contains('timestamp'))store.createIndex('timestamp','timestamp',{unique:false});
      };
      request.onerror=()=>reject(request.error);
      request.onblocked=()=>{blocked=true;reject(Error('Close other game tabs and try again.'));};
      request.onsuccess=()=>{if(blocked)request.result.close();else resolve(request.result);};
    });
    try{return await new Promise((resolve,reject)=>{
      const tx=db.transaction(STORE,mode);let value;
      tx.oncomplete=()=>resolve(value);
      tx.onabort=()=>reject(tx.error||Error('Save storage failed. No slots were changed.'));
      try{operation(tx.objectStore(STORE),result=>{value=result;});}
      catch(error){try{tx.abort();}catch{}reject(error);}
    });}finally{db.close();}
  }
  async function read(){
    return database('readonly',(store,done)=>{
      const files=[];const request=store.openCursor();
      request.onsuccess=()=>{
        const cursor=request.result;
        if(!cursor){done(files.sort((a,b)=>a.name.localeCompare(b.name)));return;}
        if(typeof cursor.key==='string'&&cursor.key.startsWith(PREFIX)){
          const name=cursor.key.slice(PREFIX.length);
          if(NAME.test(name))files.push({name,contents:cursor.value.contents});
        }
        cursor.continue();
      };
    });
  }
  async function write(files){
    // One transaction: quota/provider/transaction failure cannot leave half an import.
    return database('readwrite',store=>{
      store.put({timestamp:new Date(),mode:16895},'/persist/SAVE');
      for(const file of files)store.put({timestamp:new Date(),mode:33206,contents:file.contents},PREFIX+file.name);
    });
  }
  const locked=operation=>root.navigator?.locks?root.navigator.locks.request('re1-save-storage',{ifAvailable:true},lock=>{
    if(!lock)fail('Close the game in other tabs before managing saves.');return operation();
  }):operation();
  // The game holds the same lock until its page closes. Import never edits a
  // database while another cooperating tab has a live in-memory save mount.
  function startGame(){
    if(!root.navigator?.locks)return Promise.resolve();
    return new Promise((resolve,reject)=>{
      root.navigator.locks.request('re1-save-storage',{ifAvailable:true},lock=>{
        if(!lock)throw Error('Close the game or save manager in other tabs, then relaunch.');
        resolve();return new Promise(()=>{});
      }).catch(reject);
    });
  }
  const api=root.RE1SaveData={raw,zip,unzip,read,write,checksum,startGame,busy:()=>false};
  if(typeof module!=='undefined')module.exports=api;
  if(!root.document)return;
  const $=id=>document.getElementById(id),dialog=$('saves'),picker=$('saves-file');
  let busy=false,pending=null,requestId=0,incoming=null;
  const allowed=()=>root.re1SaveDataAllowed?.()!==false;
  function state(value){
    busy=value;
    for(const id of ['saves-export','saves-import','saves-confirm','saves-cancel','saves-close'])$(id).disabled=busy;
    dialog.setAttribute('aria-busy',String(busy));
    root.dispatchEvent(new Event('re1-storage-changed'));
  }
  function status(text){$('saves-status').textContent=text;}
  function reset(){incoming=null;$('saves-confirmation').hidden=true;$('saves-actions').hidden=false;}
  function error(error){status(String(error.message||error));state(false);}
  async function perform(operation){
    if(busy||!allowed())return;
    state(true);try{await operation();}catch(e){error(e);}finally{state(false);}
  }
  function native(method,...args){
    return new Promise((resolve,reject)=>{
      const id=String(++requestId);pending={id,resolve,reject};
      try{root.AndroidLauncher[method](id,...args);}catch(error){pending=null;reject(error);}
    });
  }
  async function chosen(files){
    incoming=files;
    $('saves-confirm-text').textContent='Import '+incoming.length+' save slot(s): '+incoming.map(f=>NAME.exec(f.name)[1]).join(', ')+'? Existing saves in those slots will be overwritten. Other slots and settings will be kept. Export your current saves first if you want a backup.';
    $('saves-actions').hidden=true;$('saves-confirmation').hidden=false;
    status('Backup checked. Confirm below to import.');$('saves-cancel').focus();
  }
  api.busy=()=>busy;
  api.nativeResult=(id,result)=>{
    if(!pending||pending.id!==id)return;
    const task=pending;pending=null;
    if(result.error)task.reject(Error(result.error));else task.resolve(result);
  };
  $('launcher-saves').onclick=()=>{
    if(!allowed())return;reset();status('Export a backup before uninstalling or clearing app/browser data. Backups work in both editions.');dialog.showModal();root.re1SaveDialogChanged?.();
  };
  $('saves-close').onclick=()=>{if(!busy){dialog.close();root.re1SaveDialogChanged?.();}};
  dialog.addEventListener('cancel',event=>{if(busy)event.preventDefault();else setTimeout(()=>root.re1SaveDialogChanged?.(),0);});
  $('saves-export').onclick=()=>perform(async()=>{
    reset();status('Preparing save ZIP…');const data=await locked(async()=>zip(await read()));
    const filename='resident-evil-'+new Date().toISOString().replace(/[:.]/g,'-')+'.saves.zip';
    if(typeof root.AndroidLauncher?.exportSaves==='function'){
      const result=await native('exportSaves',btoa(String.fromCharCode(...data)),filename);status(result.cancelled?'Export cancelled. Your saves are unchanged.':'Save ZIP exported.');
    }else{
      const url=URL.createObjectURL(new Blob([data],{type:'application/zip'}));
      const link=document.createElement('a');link.href=url;link.download=filename;document.body.append(link);link.click();link.remove();setTimeout(()=>URL.revokeObjectURL(url),60000);
      status('Save backup download started. Keep the downloaded file somewhere safe.');
    }
  });
  $('saves-import').onclick=()=>{
    if(busy||!allowed())return;reset();status('Choose a ZIP containing savedat1.dat through savedat8.dat, from this port or your USA GOG game.');
    if(typeof root.AndroidLauncher?.importSaves==='function')perform(async()=>{
      const result=await native('importSaves');
      if(result.cancelled)status('Import cancelled. Your saves are unchanged.');
      else await chosen(await unzip(Uint8Array.from(atob(result.data),c=>c.charCodeAt(0))));
    });
    else{state(true);picker.value='';picker.click();}
  };
  picker.addEventListener('cancel',()=>{status('Import cancelled. Your saves are unchanged.');state(false);});
  picker.onchange=async()=>{
    try{
      const file=picker.files[0];if(!file){status('Import cancelled. Your saves are unchanged.');return;}
      if(file.size>MAX)fail('Save ZIP is too large.');
      await chosen(await unzip(new Uint8Array(await file.arrayBuffer())));
    }catch(e){error(e);}finally{state(false);}
  };
  $('saves-cancel').onclick=()=>{reset();status('Import cancelled. Your saves are unchanged.');};
  $('saves-confirm').onclick=()=>perform(async()=>{
    if(!incoming)return;const files=incoming;
    await locked(()=>write(files));reset();status('Imported '+files.length+' save slot(s). Launch the game and choose Load game.');
  });
})(globalThis);
