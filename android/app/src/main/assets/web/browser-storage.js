// Browser-only local file indexing and bounded reads. No game data is sent to a server.
(function(root){
  'use strict';
  const required=['fontus.tim','font03t.tim','item_all.pix','optkey03.tim','status.tim','statface.tim','blue.tim','staitem.tim','kage.tim'];
  function indexFiles(input){
    const entries=Array.from(input,item=>{
      const file=item.file||item,path=String(item.path||file.webkitRelativePath||file.name).replace(/\\/g,'/');
      if(!path||path.startsWith('/')||path.includes('\0')||path.split('/').some(p=>!p||p==='.'||p==='..'))throw Error('Invalid game file path');
      if(!Number.isSafeInteger(file.size)||file.size<0||typeof file.slice!=='function')throw Error('Invalid local file: '+path);
      return {path,file};
    });
    const roots=new Set(entries.filter(e=>/(^|\/)data\/fontus\.tim$/i.test(e.path)).map(e=>e.path.slice(0,-15)));
    if(roots.size!==1)throw Error(roots.size?'Multiple installations found. Choose the specific USA game folder.':'Data/fontus.tim is missing. Choose the installed/extracted USA GOG folder.');
    const prefix=[...roots][0],files=[],seen=new Set();let total=0;
    for(const e of entries){
      if(!e.path.startsWith(prefix))continue;
      const path=e.path.slice(prefix.length),key=path.toLowerCase();
      if(seen.has(key))throw Error('Duplicate game file: '+path);
      seen.add(key);files.push({path,file:e.file});total+=e.file.size;
    }
    for(const name of required)if(!files.some(e=>e.path.toLowerCase()==='data/'+name&&e.file.size>0))throw Error('Required file is missing or empty: Data/'+name);
    if(!files.some(e=>/^stage[^/]*\/.*\.rdt$/i.test(e.path)&&e.file.size>0))throw Error('Stage room files (.rdt) are missing. Choose the complete game folder.');
    return {files,total,name:prefix.replace(/\/$/,'').split('/').pop()||'USA'};
  }
  function readFile(file,offset,length){
    if(!file||!Number.isSafeInteger(offset)||!Number.isSafeInteger(length)||offset<0||length<=0||length>262144||offset+length>file.size)throw Error('Invalid local file read');
    const url=URL.createObjectURL(file.slice(offset,offset+length));
    try{
      const xhr=new XMLHttpRequest();xhr.open('GET',url,false);
      xhr.overrideMimeType('text/plain; charset=x-user-defined');xhr.send();
      if(xhr.status!==200||xhr.responseText.length!==length)throw Error('Local file is unavailable or truncated. Select the game folder again.');
      const bytes=new Uint8Array(length);
      for(let i=0;i<length;i++)bytes[i]=xhr.responseText.charCodeAt(i)&255;
      return bytes;
    }finally{URL.revokeObjectURL(url);}
  }
  root.RE1BrowserFiles={indexFiles,readFile};
  if(typeof module!=='undefined')module.exports=root.RE1BrowserFiles;
})(globalThis);
