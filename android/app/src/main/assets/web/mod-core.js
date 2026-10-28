// Port-owned data/texture mod format. Imported text is data, never executable.
(function(root){
  'use strict';
  const MAX_PIXELS=16*1024*1024,MAX_SIDE=4096,MAX_IMAGE_BYTES=64*1024*1024;
  const KEY=/^[0-9a-f]{32}_([1-9][0-9]{0,3})x([1-9][0-9]{0,3})$/;
  const fail=message=>{throw Error(message);};
  const crcTable=Uint32Array.from({length:256},(_,n)=>{
    for(let k=0;k<8;k++)n=n&1?(n>>>1)^0xedb88320:n>>>1;return n>>>0;
  });
  function crc32(bytes){let crc=0xffffffff;for(const byte of bytes)crc=crcTable[(crc^byte)&255]^(crc>>>8);return (crc^0xffffffff)>>>0;}
  function path(value){
    if(typeof value!=='string'||value.length>1024)fail('Invalid mod file path.');
    value=value.replace(/\\/g,'/');
    const parts=value.split('/');
    if(parts.length>20||parts.some(p=>!p||p==='.'||p==='..'||p.length>255||/[\x00-\x1f\x7f:]/.test(p)))fail('Unsafe mod file path: '+value);
    return value;
  }
  function gamePath(value){
    value=path(value);
    // These are read-only game resources. Save/configuration/database paths
    // and Windows plugins are deliberately outside the virtual data overlay.
    if(!/^(?:data|stage[0-9a-f]+|enemy|players|pld|item_m[12]|effspr|objspr|sound|voice|movie)\//i.test(value)||
       !/\.(?:tim|pix|pak|rdt|emd|emw|tmd|etm|ptc|ivm|dor|wav|ogg|mp3|avi|mp4)$/i.test(value))fail('Unsupported game resource: '+value);
    return value;
  }
  function text(value,fallback,max){return typeof value==='string'&&value.trim()?value.trim().slice(0,max):fallback;}
  function index(files){
    if(!Array.isArray(files)||!files.length||files.length>50000)fail('Invalid mod ZIP file list.');
    const found=new Map();
    for(const entry of files){
      const name=path(entry.path),lower=name.toLowerCase();
      if(found.has(lower)||!Number.isInteger(entry.slot)||entry.slot<0||entry.slot>=50000||
        !Number.isSafeInteger(entry.size)||entry.size<0||entry.size>1024*1024*1024||
        !Number.isInteger(entry.crc)||entry.crc<0||entry.crc>0xffffffff)fail('Invalid or duplicate mod ZIP entry.');
      found.set(lower,{...entry,path:name});
    }
    return found;
  }
  function manifest(files){
    const found=index(files),candidates=[...found.values()].filter(e=>/(?:^|\/)mod\.json$/i.test(e.path));
    if(candidates.length>1)fail('Choose a ZIP containing one mod.json.');
    if(candidates.length){if(candidates[0].size>262144)fail('mod.json is too large.');return {entry:candidates[0],prefix:candidates[0].path.slice(0,-8),format:'native'};}
    const shdp=[...found.values()].find(e=>/(?:^|\/)hires\/bgd\/[0-9a-f]{8}(?:_[0-9a-f]+)?\.webp$/i.test(e.path)&&!/(?:^|\/)bio1dc\//i.test(e.path));
    if(shdp)return {format:'seamless',prefix:shdp.path.slice(0,shdp.path.toLowerCase().indexOf('hires/bgd/'))};
    const rebirth=[...found.values()].filter(e=>/(?:^|\/)encoding\.xml$/i.test(e.path));
    if(rebirth.length>1)fail('Choose one Classic REbirth translation at a time.');
    if(rebirth.length){const prefix=rebirth[0].path.slice(0,-12);if(found.has((prefix+'manifest.txt').toLowerCase()))return {format:'rebirth',prefix};}
    if([...found.values()].some(e=>/(?:^|\/)mod_[^/]+\.(?:7z|rar|zip)$/i.test(e.path)))
      fail('This download contains a nested Classic REbirth mod archive. Extract mod_*.7z/RAR/ZIP and create one ZIP of its contents together with any Movie folder. Importing only the outer ZIP would leave the translation unused.');
    if(pcFiles(files).length)return {format:'pc-data'};
    if([...found.values()].some(e=>/\.(?:zip|rar|7z)$/i.test(e.path)))
      fail('This ZIP contains another archive. Extract the inner archive first, then ZIP the game folders themselves (for example Enemy/CHAR11.EMD).');
    fail('No supported mod resources found. Use an Android mod, full SHDP ZIP, or PC resource ZIP with its original game folders.');
  }
  function pcFiles(files){
    const out=[],targets=new Set();
    for(const file of index(files).values()){
      const match=file.path.match(/(?:^|\/)((?:data|stage[0-9a-f]+|enemy|players|pld|item_m[12]|effspr|objspr|sound|voice|movie)\/.+)$/i);
      if(!match)continue;
      let target;try{target=gamePath(match[1]);}catch(_){continue;}
      const key=target.toLowerCase();if(targets.has(key))fail('Duplicate PC game resource: '+target);
      targets.add(key);out.push({target,file});
    }
    return out;
  }
  function pcPlan(files,name,gameFiles){
    const found=new Map(gameFiles.map(file=>[file.path.toLowerCase(),file]));
    const overrides=pcFiles(files).flatMap(item=>{
      const key=item.target.toLowerCase();if(found.has(key))return [item];
      const old=/\.mp4$/i.test(key)?'.avi':/\.ogg$/i.test(key)?'.wav':null;
      const original=old&&found.get(key.slice(0,-4)+old);
      // New MP4/OGG siblings share the selected GOG folder's spelling. This
      // avoids creating separate Movie/movie or Voice/VOICE directories in
      // the case-sensitive virtual filesystem for a Windows mod ZIP.
      return original?[{...item,target:original.path.slice(0,-4)+key.slice(-4)}]:[];
    });
    if(!overrides.length)fail('No PC resources match your GOG folder. Keep the original game folders and filenames in the mod ZIP.');
    return {name:text(name?.replace(/\.zip$/i,''),'PC resource mod',120),author:'PC mod',version:'1.0',
      description:'Compatible game-file replacements. Disable this mod to use your selected game folder again. Windows plugins are ignored.',
      credits:'',textures:[],overrides,compatibility:compatibility(files,overrides.map(item=>item.file.path),'PC resource replacements')};
  }
  function compatibility(files,used,format,warnings=[]){
    const known=new Set(used.map(name=>name.toLowerCase())),skipped=[];
    for(const file of files){
      const name=file.path.toLowerCase();
      if(known.has(name)||/(?:^|\/)(?:readme[^/]*|license[^/]*|credits[^/]*|description\.txt|manifest\.txt|mod-info\.txt|mod\.json)$/.test(name))continue;
      const reason=/\.(?:dll|asi|exe|bat|cmd|ps1)$/i.test(name)?'Windows program/plugin (not executed)':/\.(?:zip|rar|7z)$/i.test(name)?'Nested archive (extract and repackage first)':/\.(?:xml|png|webp|jpe?g)$/i.test(name)?'No supported mapping for this file':'No matching supported GOG resource';
      if(skipped.length<200)skipped.push({path:file.path,reason});
    }
    return {format,used:known.size,total:files.length,skipped,warnings:warnings.slice(0,50)};
  }
  function infoFile(files){
    const candidates=[...index(files).values()].filter(file=>/(?:^|\/)mod-info\.txt$/i.test(file.path));
    if(candidates.length>1)fail('Keep one mod-info.txt in the mod ZIP.');
    if(candidates[0]?.size>16384)fail('mod-info.txt must be at most 16 KiB.');
    return candidates[0]||null;
  }
  function parseInfo(value){
    if(typeof value!=='string'||new TextEncoder().encode(value).length>16384)fail('mod-info.txt must be UTF-8 text of at most 16 KiB.');
    const out={},seen=new Set();let field=null;
    for(const line of value.replace(/^\uFEFF/,'').replace(/\r\n?/g,'\n').split('\n')){
      if(/^\s*#/.test(line))continue;
      const match=line.match(/^\s*(name|description|author|version)\s*[:=]\s*(.*)$/i);
      if(match){field=match[1].toLowerCase();if(seen.has(field))fail('Duplicate '+field+' in mod-info.txt.');seen.add(field);out[field]=match[2];}
      else if(field==='description')out.description+='\n'+line;
    }
    for(const [key,max] of [['name',120],['description',2000],['author',160],['version',40]])if(out[key]!==undefined){
      out[key]=out[key].trim();if(out[key].length>max)fail(key+' in mod-info.txt is too long (maximum '+max+' characters).');
    }
    return out;
  }
  function modDetails(value){
    if(!value||typeof value.name!=='string'||!value.name.trim()||value.name.trim().length>120||typeof value.description!=='string'||value.description.trim().length>2000)
      fail('Enter a mod name (up to 120 characters) and a description of up to 2000 characters.');
    return {name:value.name.trim(),description:value.description.trim()};
  }
  function plan(json,files,prefix=''){
    const found=index(files),get=value=>{
      const name=prefix+path(value),entry=found.get(name.toLowerCase());
      if(!entry)fail('Mod file is missing: '+name);return entry;
    };
    if(!json||json.format!=='re1-android-mod'||![1,2].includes(json.formatVersion)||json.game!=='re1-usa')
      fail('Unsupported mod format. Use a re1-android-mod version 1 or 2 ZIP for re1-usa.');
    let runtime=null;
    if(json.runtime!==undefined){
      if(json.formatVersion!==2||!json.runtime||typeof json.runtime!=='object')fail('Campaign data requires mod format version 2.');
      runtime={campaign:get(json.runtime.campaign),locale:json.runtime.locale===undefined?null:get(json.runtime.locale),originalFont:json.runtime.originalFont===true};
      if(runtime.campaign.size<2020||runtime.campaign.size>262144||runtime.locale&&(runtime.locale.size<16||runtime.locale.size>2097152))fail('Campaign/translation data is oversized or truncated.');
    }else if(json.formatVersion===2)fail('Mod format version 2 needs campaign runtime data.');
    if(typeof json.id!=='string'||!/^[a-z0-9][a-z0-9._-]{0,79}$/.test(json.id))fail('mod.json needs an id using letters, numbers, dots, underscores or hyphens.');
    const meta={slug:json.id,name:text(json.name,json.id,120),author:text(json.author,'Unknown author',160),
      version:text(json.version,'1.0',40),description:text(json.description,'',2000),credits:text(json.credits,'',12000)};
    let definitions=json.textures;
    if(definitions===undefined){
      definitions=[];
      for(const entry of found.values())if(entry.path.toLowerCase().startsWith((prefix+'textures/').toLowerCase())){
        const file=entry.path.slice(prefix.length);
        if(/\.(?:png|webp|jpe?g)$/i.test(file))definitions.push({source:file.slice(9).replace(/\.(?:png|webp|jpe?g)$/i,''),file});
      }
    }
    if(!Array.isArray(definitions)||definitions.length>10000)fail('Too many texture replacements.');
    const textures=definitions.map(item=>{
      if(!item||typeof item!=='object')fail('Invalid texture replacement.');
      const file=get(item.file);
      if(!/\.(?:png|webp|jpe?g)$/i.test(file.path)||file.size>MAX_IMAGE_BYTES)fail('Use PNG, WebP or JPEG textures smaller than 64 MiB.');
      const key=item.key?.toLowerCase();if(key&&!KEY.test(key))fail('Invalid original texture key.');
      const source=item.source===undefined?null:gamePath(item.source);
      if(!key&&!source)fail('Texture replacement needs an original source file or texture key.');
      const image=item.image??0,palette=item.palette??0,rule=item.rule;
      if(!Number.isInteger(image)||image<0||image>4095||!Number.isInteger(palette)||palette<0||palette>255||
        rule!==undefined&&!['page','opaque','index','effect'].includes(rule))fail('Invalid original image/palette selection.');
      return {file,source,key,image,palette,rule,alpha:item.alpha==='image'?'image':'auto'};
    });
    let data=json.files;
    if(data===undefined){
      data=[];for(const entry of found.values())if(entry.path.toLowerCase().startsWith((prefix+'files/').toLowerCase())){
        const file=entry.path.slice(prefix.length);data.push({target:file.slice(6),file});
      }
    }
    if(!Array.isArray(data)||data.length>10000)fail('Too many data replacements.');
    const targets=new Set(),overrides=data.map(item=>{
      const target=gamePath(item.target),key=target.toLowerCase();
      if(targets.has(key))fail('Duplicate game resource: '+target);targets.add(key);
      return {target,file:get(item.file)};
    });
    if(!textures.length&&!overrides.length&&!runtime)fail('This mod does not contain any texture or game-data replacements.');
    return {...meta,textures,overrides,...(runtime?{runtime}:{})};
  }
  // R1CP v1: fixed game-state/inventory/music tables plus bounded sound rows.
  // No executable addresses, imported code or arbitrary filesystem paths.
  function campaign(bytes){
    if(!(bytes instanceof Uint8Array)||bytes.length<2020||bytes.length>262144)fail('Invalid campaign data size.');
    let at=0;
    const u8=()=>{if(at>=bytes.length)fail('Truncated campaign data.');return bytes[at++];};
    const u16=()=>{const lo=u8();return lo|(u8()<<8);};
    const u32=()=>{const lo=u16();return (lo|(u16()<<16))>>>0;};
    if(u32()!==0x50433152||u32()!==1||u32()!==bytes.length)fail('Invalid campaign data header.');
    const character=u8(),skipSelection=u8(),x=(u16()<<16)>>16,z=(u16()<<16)>>16,angle=u16();
    const card=u16(),inventory=u16(),sounds=u16(),groups=u16(),rooms=u16(),seconds=u16();
    if(character>1||skipSelection>1||angle>4095||card!==1052||inventory!==40||sounds>203||groups!==57||rooms!==224||seconds&&(seconds<60||seconds>3600||seconds%60))fail('Unsupported campaign settings.');
    const state=bytes.subarray(at,at+card);at+=card;
    if(state[512]>=7||state[513]>=29||state[514]>=32)fail('Invalid campaign start room.');
    for(let n=0;n<inventory;n+=2)if(bytes[at+n]>128)fail('Invalid campaign inventory.');at+=inventory;
    for(let n=0;n<896;n++){const value=u8();if(value!==255&&value>=57)fail('Invalid campaign music group.');}
    const name=()=>{
      const size=u8();if(size>63)fail('Campaign sound name is too long.');let value='';
      for(let n=0;n<size;n++)value+=String.fromCharCode(u8());
      if(value&&(!/^[A-Za-z0-9_+&.-]+$/.test(value)||value.includes('..')))fail('Unsafe campaign sound name.');
    };
    for(let n=0;n<57*4;n++){name();if(u8()>1)fail('Invalid campaign music loop.');}
    const seen=new Set();
    for(let n=0;n<sounds;n++){const key=u16();if(key>=203||seen.has(key))fail('Invalid or duplicate campaign sound row.');seen.add(key);for(let i=0;i<48;i++)name();}
    if(at!==bytes.length)fail('Unexpected data after campaign profile.');
    return {character,skipSelection:!!skipSelection,x,z,angle,soundRows:sounds,countdownSeconds:seconds||180};
  }
  function dimensions(w,h,ow,oh,page=false){
    if(![w,h,ow,oh].every(Number.isInteger)||w<1||h<1||ow<w||oh<h||ow>MAX_SIDE||oh>MAX_SIDE||
      ow*oh>MAX_PIXELS||!page&&ow*h!==oh*w)fail('Replacement image must preserve the original proportions and be at most 4096 pixels per side.');
  }
  function header(w,h,ow,oh,page=false){
    dimensions(w,h,ow,oh,page);const bytes=new Uint8Array(32),view=new DataView(bytes.buffer);bytes.set([82,49,72,68]);
    [page?4:3,w,h,ow,oh,ow*oh*4,0].forEach((value,n)=>view.setUint32(4+n*4,value,true));return bytes;
  }
  function imageSize(bytes){
    if(!(bytes instanceof Uint8Array)||bytes.length<12||bytes.length>MAX_IMAGE_BYTES)fail('Invalid texture file size.');
    const view=new DataView(bytes.buffer,bytes.byteOffset,bytes.byteLength);let w,h;
    const ascii=(at,length)=>String.fromCharCode(...bytes.subarray(at,at+length));
    if(bytes.length>=24&&ascii(1,3)==='PNG'&&view.getUint32(12,false)===0x49484452){w=view.getUint32(16,false);h=view.getUint32(20,false);}
    else if(ascii(0,4)==='RIFF'&&ascii(8,4)==='WEBP'){
      if(view.getUint32(4,true)+8>bytes.length)fail('Truncated WebP texture.');
      const type=ascii(12,4);
      if(type==='VP8X'&&bytes.length>=30){w=1+bytes[24]+(bytes[25]<<8)+(bytes[26]<<16);h=1+bytes[27]+(bytes[28]<<8)+(bytes[29]<<16);}
      else if(type==='VP8L'&&bytes.length>=25&&bytes[20]===0x2f){const bits=view.getUint32(21,true);w=1+(bits&16383);h=1+((bits>>>14)&16383);}
      else if(type==='VP8 '&&bytes.length>=30&&bytes[23]===0x9d&&bytes[24]===1&&bytes[25]===0x2a){w=view.getUint16(26,true)&16383;h=view.getUint16(28,true)&16383;}
    }else if(bytes[0]===255&&bytes[1]===216){
      let at=2;
      while(at+4<=bytes.length){
        if(bytes[at++]!==255)fail('Invalid JPEG texture.');while(bytes[at]===255)at++;
        const marker=bytes[at++];if(marker===0xd9||marker===0xda)break;
        if(marker===1||marker>=0xd0&&marker<=0xd7)continue;
        if(at+2>bytes.length)break;const size=view.getUint16(at,false);
        if(size<2||at+size>bytes.length)fail('Truncated JPEG texture.');
        if([0xc0,0xc1,0xc2,0xc3,0xc5,0xc6,0xc7,0xc9,0xca,0xcb,0xcd,0xce,0xcf].includes(marker)&&size>=8){h=view.getUint16(at+3,false);w=view.getUint16(at+5,false);break;}
        at+=size;
      }
    }
    if(!Number.isInteger(w)||!Number.isInteger(h)||w<1||h<1||w>MAX_SIDE||h>MAX_SIDE||w*h>MAX_PIXELS)fail('Unsupported texture header or image larger than 4096 pixels per side.');
    return {w,h};
  }
  function active(mods,source){
    const textures=new Map(),files=new Map(),conflicts=new Map();
    // The top row has priority. Lower rows fill gaps, never overwrite it.
    for(const mod of mods)if(mod.enabled&&mod.source===source){
      let count=0;
      for(const record of mod.records||[]){if(textures.has(record.key))count++;else textures.set(record.key,{...record,id:mod.id});}
      for(const file of mod.overrides||[]){const key=file.target.toLowerCase();if(files.has(key))count++;else files.set(key,{...file,id:mod.id});}
      conflicts.set(mod.id,count);
    }
    return {textures,files,conflicts};
  }
  function seamlessMovies(files,prefix,gameFiles){
    const mapping={oj:'ou',pj:'pu',ed4:'eu4',ed5:'eu5',stfc_b:'stfc_r',stfj_b:'stfj_r',stfz_b:'stfz_r',staf_b:'staf_r'};
    const originals=new Set(gameFiles.map(f=>f.path.toLowerCase())),out=[];
    for(const file of files){
      if(!file.path.toLowerCase().startsWith((prefix+'movie/').toLowerCase()))continue;
      const match=file.path.slice(prefix.length).match(/^movie\/([a-z0-9_]+)\.mp4$/i);if(!match)continue;
      const name=match[1].toLowerCase(),target='Movie/'+(mapping[name]||name)+'.mp4';
      if(originals.has(target.toLowerCase())||originals.has(target.toLowerCase().replace(/\.mp4$/,'.avi')))out.push({target,file});
    }
    return out;
  }
  function crop(image,rect){
    if(!Array.isArray(rect)||rect.length!==6||!rect.every(Number.isInteger))fail('Invalid texture-page crop.');
    const [x,y,w,h,rw,rh]=rect,sx=image.w/rw,sy=image.h/rh;
    if(x<0||y<0||w<1||h<1||rw<1||rh<1||x+w>rw||y+h>rh||![x*sx,y*sy,w*sx,h*sy].every(Number.isInteger))fail('Invalid texture-page crop bounds.');
    const ow=w*sx,oh=h*sy,rgba=new Uint8Array(ow*oh*4);
    for(let row=0;row<oh;row++){const at=((y*sy+row)*image.w+x*sx)*4;rgba.set(image.rgba.subarray(at,at+ow*4),row*ow*4);}
    return {w:ow,h:oh,rgba};
  }
  function upgradeSeamless(mod,profile){
    if(mod.slug!=='seamless-hd'||mod.error||(mod.adapterRevision||1)<2||!profile||profile.revision<3)return mod;
    const records=new Map(mod.records.map(record=>[record.key,record]));let changed=false;
    for(const alias of profile.records){
      if(!alias.reuse||records.has(alias.key))continue;
      const old=records.get(alias.reuse),key=KEY.exec(alias.key),reuse=KEY.exec(alias.reuse);
      if(!old||!key||!reuse)continue;
      let record={...old,key:alias.key,path:alias.path||old.path};
      if(alias.crop){
        const [x,y,w,h,rw,rh]=alias.crop;
        if(alias.crop.length!==6||!alias.crop.every(Number.isInteger)||x<0||y<0||w<1||h<1||x+w>rw||y+h>rh||
          +key[1]!==w||+key[2]!==h||+reuse[1]!==rw||+reuse[2]!==rh||!Number.isInteger(old.width*w/rw)||!Number.isInteger(old.height*h/rh))continue;
        record={...record,crop:alias.crop,width:old.width*w/rw,height:old.height*h/rh};
      }else if(key[1]!==reuse[1]||key[2]!==reuse[2])continue;
      records.set(alias.key,record);changed=true;
    }
    return changed?{...mod,records:[...records.values()]}:mod;
  }
  const api={path,gamePath,text,index,manifest,plan,campaign,pcPlan,compatibility,infoFile,parseInfo,modDetails,dimensions,header,imageSize,active,seamlessMovies,upgradeSeamless,crop,crc32,KEY,MAX_PIXELS,MAX_SIDE,MAX_IMAGE_BYTES};
  root.RE1ModCore=api;if(typeof module!=='undefined')module.exports=api;
})(globalThis);
