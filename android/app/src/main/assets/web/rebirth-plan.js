// Install-time translation conversion. No XML parsing on the gameplay thread.
async function rebirthPlan(message,info,sourceFiles){
  const all=message.pack.files,found=core.index(all),prefix=info.prefix,used=new Set(),warnings=[],records=[],textures=[];
  const get=name=>found.get((prefix+name).toLowerCase());
  const read=async name=>{
    const file=get(name);if(!file)return null;
    if(file.size>RE1Rebirth.MAX_XML)throw Error('Classic REbirth XML/text is too large: '+file.path);
    used.add(file.path);return new TextDecoder('utf-8',{fatal:true}).decode(await bytes(file,message.pack.id));
  };
  const manifest=await read('manifest.txt');
  if(!/^\s*\[MOD\]\s*$/im.test(manifest||''))throw Error('Invalid Classic REbirth manifest.txt.');
  const name=/^\s*Name\s*=\s*(.+)$/im.exec(manifest)?.[1]?.trim()||message.pack.name.replace(/\.zip$/i,'');
  const encodingText=await read('encoding.xml'),font=RE1Rebirth.encoding(RE1Rebirth.xml(encodingText));
  const missing=new Set();
  function add(kind,index,text,stage=0,room=0,variant=2,options={}){
    if(kind===6&&!text.replace(/\{scroll\s+\d+\}/g,'').trim())return;
    let compiled;
    try{compiled=RE1Rebirth.compile(text,font,kind===2?'item':kind>=3&&kind<=5?'menu':'message',options);}
    catch(error){throw Error(error.message+' (table '+kind+', entry '+index+(stage?', room '+stage+room.toString(16):'')+')');}
    compiled.missing.forEach(ch=>missing.add(ch));records.push({key:RE1Rebirth.key(kind,index,stage,room,variant),bytes:compiled.bytes});
  }
  const groups=[['misc.xml',0,63],['desc.xml',1,79],['item_simple.xml',2,129],['save.xml',3,21],['ascii.xml',4,8]];
  for(const [file,kind,max] of groups){
    const text=await read(file);if(text===null)continue;
    const document=RE1Rebirth.xml(text);if(document.kind!=='Strings')throw Error('Expected Strings in '+file);
    document.entries.slice(0,max).forEach((text,index)=>{
      // Timers use printf fields and the port's existing timer rendering.
      if(kind===4&&index!==4)return;
      const cells=kind===3&&(index===5||index===6)?5:kind===3&&index>=10&&index<=16?19:0;
      add(kind,index,text,0,0,2,{cells});
    });
    if(document.entries.length>max)warnings.push(file+': '+(document.entries.length-max)+' entries outside the USA table are unused.');
  }
  for(const file of all){
    const relative=file.path.slice(prefix.length),match=/^text\/room_([1-7])([0-9a-f]{2})(?:_([01]))?\.xml$/i.exec(relative);
    if(!file.path.toLowerCase().startsWith(prefix.toLowerCase())||!match)continue;
    const document=RE1Rebirth.xml(await read(relative));if(document.kind!=='Strings')throw Error('Expected room Strings in '+relative);
    document.entries.slice(0,64).forEach((text,index)=>add(6,index,text,Number(match[1]),parseInt(match[2],16),match[3]===undefined?2:Number(match[3])));
  }
  if(!records.length)throw Error('No supported translated strings were found.');
  if(missing.size)warnings.push('Missing font characters use ?: '+[...missing].join(' '));
  let raw=[];try{raw=core.pcPlan(all,message.pack.name,sourceFiles).overrides;}catch(error){if(!/No PC resources match/.test(error.message))throw error;}
  raw.forEach(item=>used.add(item.file.path));
  const atlas=get('Data/PBFONT0.tim');if(!atlas)throw Error('This translation needs Data/PBFONT0.tim.');
  used.add(atlas.path);textures.push({file:atlas,source:'Data/fontus.tim',image:0,palette:0,rule:'page',alpha:'image',rebirthFont:true,fontMap:font.glyphs});
  const gameFiles=new Map(sourceFiles.map(file=>[file.path.toLowerCase(),file]));
  let lastPath='',container;
  for(const file of all){
    if(!file.path.toLowerCase().startsWith(prefix.toLowerCase()))continue;
    const match=/^file\/filem_([0a-z])([0-9]{2})\.png$/i.exec(file.path.slice(prefix.length));if(!match)continue;
    const target='Item_m2/filem_'+match[1].toLowerCase()+'0.pix',entry=gameFiles.get(target.toLowerCase());
    if(!entry){warnings.push('No GOG document container for '+file.path);continue;}
    if(lastPath!==target){container=await bytes(entry);lastPath=target;}
    const page=Number(match[2]),view=new DataView(container.buffer,container.byteOffset,container.byteLength),first=view.getUint32(0,true);
    if(first<8||first>container.length||first%4||page*4+4>first){warnings.push('No original document page for '+file.path);continue;}
    const start=view.getUint32(page*4,true);if(start<first||start>=container.length){warnings.push('No original document page for '+file.path);continue;}
    const unpacked=images.unpackPak(container.subarray(start)),tim=images.tim(unpacked,0);
    used.add(file.path);
    for(const rule of ['page','opaque']){
      const original=images.decode(tim,0,rule),key=images.key(original,tim.w,tim.h);
      const previous=textures.find(t=>t.key===key);
      if(previous&&(previous.file.crc!==file.crc||previous.file.size!==file.size)){
        const [a,b]=await Promise.all([decode(await bytes(previous.file,message.pack.id)),decode(await bytes(file,message.pack.id))]);
        if(a.w!==b.w||a.h!==b.h||a.rgba.some((value,n)=>value!==b.rgba[n]))throw Error('Two different translated document pages share the same original texture. This pack needs a page-specific adapter: '+file.path);
      }
      if(!previous)textures.push({file,key,alpha:'image',pad:[tim.w,tim.h],path:target,layout:'page'});
    }
  }
  const credits=await read('description.txt')||'';
  if(get('save2.xml'))warnings.push('save2.xml is for Classic REbirth’s alternate save UI; this port uses save.xml.');
  if(get('config.xml'))warnings.push('Classic REbirth’s desktop control-options UI is unused. Configure touch/controllers in the launcher.');
  if(get('ascii.xml'))warnings.push('ascii.xml supplies Yes/No labels. Desktop quit, timer and difficulty UI replacements are unused.');
  warnings.push('Keep this translation above SHDP and other font/UI packs. Original GOG prompt actions are preserved.');
  const digest=await crypto.subtle.digest('SHA-256',new TextEncoder().encode(name.toLowerCase()));
  return {slug:'rebirth.'+Array.from(new Uint8Array(digest),v=>v.toString(16).padStart(2,'0')).join(''),name,author:'Translation authors (see Credits)',version:'1.0',description:'Classic REbirth translation adapted for the USA GOG engine.',credits,
    textures,overrides:raw,locale:RE1Rebirth.binary(records),localeCount:records.length,
    compatibility:core.compatibility(all,[...used],'Classic REbirth translation',warnings)};
}
