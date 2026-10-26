// Offline preparation of an extracted Classic REbirth translation. No game
// installation is read, no archive program/plugin is executed, no npm needed.
'use strict';
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto');
const core=require('../../android/app/src/main/assets/web/mod-core.js');
const hd=require('../../android/app/src/main/assets/web/hd-core.js');
const rebirth=require('../../android/app/src/main/assets/web/rebirth-core.js');
const fail=message=>{throw Error(message);};
function inside(parent,file){const relative=path.relative(parent,file);return relative===''||!relative.startsWith('..'+path.sep)&&relative!=='..'&&!path.isAbsolute(relative);}
function realDestination(file){let parent=path.dirname(file),tail=[path.basename(file)];while(!fs.existsSync(parent)){tail.unshift(path.basename(parent));const next=path.dirname(parent);if(next===parent)fail('The output drive/root does not exist');parent=next;}return path.join(fs.realpathSync(parent),...tail);}
function prepare(folder,options,helpers){
 for(const key of Object.keys(options))if(!['movies','out','overwrite','description-encoding'].includes(key))fail('Unknown rebirth option --'+key);
 if(!fs.existsSync(folder)||!fs.statSync(folder).isDirectory())fail('Select the extracted mod folder, not a ZIP/RAR/7z file. Extract the outer archive and its inner mod archive first.');
 if(fs.lstatSync(folder).isSymbolicLink())fail('Select a real folder, not a symlink');
 folder=fs.realpathSync(folder);
 const files=helpers.scan(folder).map((entry,slot)=>({...entry,slot}));
 let info;try{info=core.manifest(files);}catch(error){fail(error.message+'\nExtract all archives first, then choose the folder containing manifest.txt and encoding.xml.');}
 if(info.format!=='rebirth')fail('Mod Helper checks and packages supported Classic REbirth XML translations with manifest.txt and encoding.xml. It does not convert incompatible mods. Compatible model/sound resource ZIPs and the full SHDP ZIP can already be imported directly. See docs/PORTING_PC_MODS.md.');
 const selected=new Map(),lower=new Map(),generated=new Map(),omitted=[],warnings=[];
 const notice=/^(?:readme|license|licence|copying|authors|credits|notice|copyright)(?:[^/]*\.(?:txt|md)|[^/.]*)$/i;
 const prefix=info.prefix,parents=[''];let at=prefix;
 while(at){parents.push(at);at=at.replace(/[^/]+\/$/,'');}
 const add=(target,source)=>{
  target=core.path(target);const key=target.toLowerCase(),previous=lower.get(key);
  if(previous){if(fs.realpathSync(selected.get(previous))===fs.realpathSync(source))return;fail('Two source files map to '+target+'. Keep one copy or select a more specific folder.');}
  lower.set(key,target);selected.set(target,source);
 };
 for(const file of files){
  const name=file.path,under=name.toLowerCase().startsWith(prefix.toLowerCase());let relative=under?name.slice(prefix.length):null,retainedNotice=false;
  if(relative===null){for(const parent of parents){const part=name.slice(parent.length);if(!name.toLowerCase().startsWith(parent.toLowerCase()))continue;if(/^movie\//i.test(part)){relative=part;break;}if(notice.test(part)){add('notices/'+name,helpers.safe(folder,name));retainedNotice=true;break;}}}
  if(retainedNotice)continue;
  if(relative===null){omitted.push({path:name,reason:'Outside the selected translation and its Movie folder'});continue;}
  let keep=/^(?:manifest\.txt|encoding\.xml|misc\.xml|desc\.xml|item_simple\.xml|save\.xml|ascii\.xml|description\.txt|mod-info\.txt)$/i.test(relative)||notice.test(relative)||/^text\/room_[1-7][0-9a-f]{2}(?:_[01])?\.xml$/i.test(relative)||/^file\/filem_[0a-z][0-9]{2}\.png$/i.test(relative);
  try{core.gamePath(relative);keep=true;}catch{}
  if(keep)add(relative,helpers.safe(folder,name));
  else omitted.push({path:name,reason:/\.(?:dll|asi|exe|bat|cmd|ps1)$/i.test(name)?'Windows program/plugin (not included or executed)':/\.(?:zip|rar|7z)$/i.test(name)?'Archive (not unpacked by this tool; check that needed contents were extracted)':/\.backup$/i.test(name)?'Backup copy':/^(?:config|save2)\.xml$/i.test(relative)?'Classic REbirth desktop UI unused by this port':'No supported mapping in the current translation adapter'});
 }
 if(options.movies){
  const movies=path.resolve(options.movies);if(!fs.existsSync(movies)||!fs.statSync(movies).isDirectory()||fs.lstatSync(movies).isSymbolicLink())fail('--movies must point to the extracted Movie folder');
  for(const entry of helpers.scan(movies)){
   if(/^[^/]+\.(?:mp4|avi)$/i.test(entry.path))add('Movie/'+entry.path,helpers.safe(movies,entry.path));
   else omitted.push({path:'[extra movies]/'+entry.path,reason:'Only direct MP4/AVI movie replacements are included from --movies'});
  }
 }
 const lookup=name=>{const match=lower.get(name.toLowerCase());return match&&selected.get(match);};
 const read=name=>{
  const file=lookup(name);if(!file)return null;
  if(fs.statSync(file).size>rebirth.MAX_XML)fail(name+' exceeds the supported XML/text size');
  try{return new TextDecoder('utf-8',{fatal:true}).decode(fs.readFileSync(file));}catch{fail(name+' must be UTF-8 text. Convert its text encoding before preparing the pack.');}
 };
 const manifest=read('manifest.txt');if(!/^\s*\[MOD\]\s*$/im.test(manifest||''))fail('manifest.txt needs an [MOD] section');
 // Description/credits are metadata, not the game's XML text. Older packs
 // commonly use Windows ANSI or Notepad's UTF-16. Normalize only the ZIP copy
 // for the released importer's UTF-8 reader, retaining the exact original.
 if(options['description-encoding']&&!lookup('description.txt'))fail('--description-encoding needs a description.txt in the mod');
 if(lookup('description.txt')){
  const file=lookup('description.txt');
  if(fs.statSync(file).size>rebirth.MAX_XML)fail('description.txt exceeds the supported XML/text size');
  const original=fs.readFileSync(file);let encoding=options['description-encoding'],text;
  if(!encoding){
   if(original[0]===255&&original[1]===254)encoding='utf-16le';
   else if(original[0]===254&&original[1]===255)encoding='utf-16be';
   else {try{text=new TextDecoder('utf-8',{fatal:true}).decode(original);}catch{encoding='windows-1252';warnings.push('description.txt is not UTF-8; assuming Windows-1252 for its credits. Check accented characters. If needed, rerun with --description-encoding windows-1250 (Polish), windows-1251 (Cyrillic), or the original encoding.');}}
  }
  if(encoding){
   try{text=new TextDecoder(encoding,{fatal:true}).decode(original);}catch{fail('Cannot read description.txt as '+encoding+'. Use --description-encoding with its original encoding (for example windows-1252 or windows-1250).');}
   if(text.includes('\0'))fail('description.txt contains NUL characters. Use --description-encoding utf-16le or utf-16be if it is UTF-16 without a BOM.');
   let backup='notices/original-description.txt',number=2;
   while(lower.has(backup.toLowerCase()))backup='notices/original-description-'+(number++)+'.txt';
   const normalized=Buffer.from(text,'utf8');
   if(normalized.length>rebirth.MAX_XML)fail('description.txt exceeds the supported text size after UTF-8 conversion');
   add(backup,file);generated.set(lower.get('description.txt'),normalized);
   warnings.push('Converted description.txt from '+encoding+' to UTF-8 in the ZIP only. Original credits retained byte-for-byte in '+backup+'.');
  }else if(text.includes('\0'))fail('description.txt contains NUL characters. Use --description-encoding utf-16le or utf-16be if it is UTF-16 without a BOM.');
 }
 const font=rebirth.encoding(rebirth.xml(read('encoding.xml'))),fontFile=lookup('Data/PBFONT0.tim');
 if(!fontFile)fail('This translation does not include Data/PBFONT0.tim. The current Android adapter needs the author\'s matching 256x256 font atlas; some PC packs rely on a font supplied by Classic REbirth. Obtain the matching font from the mod author. Changing the description encoding cannot supply this missing font.');
 if(fs.statSync(fontFile).size>core.MAX_IMAGE_BYTES)fail('PBFONT0.tim is too large');
 const atlas=hd.tim(new Uint8Array(fs.readFileSync(fontFile)),0);
 if(atlas.w!==256||atlas.h!==256)fail('This font layout needs an adapter: PBFONT0.tim must be 256x256 for build 38.');
 const missing=new Set();let strings=0,documents=0,movies=0;
 const validate=(name,mode,limit,kind)=>{
  const text=read(name);if(text===null)return;
  const doc=rebirth.xml(text);if(doc.kind!=='Strings')fail(name+' needs a Strings XML root');
  for(const [index,text] of doc.entries.slice(0,limit).entries()){
   if(kind==='ascii'&&index!==4)continue;
   if(kind==='room'&&!text.replace(/\{scroll\s+\d+\}/g,'').trim())continue;
   const cells=kind==='save'&&(index===5||index===6)?5:kind==='save'&&index>=10&&index<=16?19:0;
   try{const compiled=rebirth.compile(text,font,mode,{cells});compiled.missing.forEach(ch=>missing.add(ch));strings++;}
   catch(error){fail(name+', entry '+index+': '+error.message);}
  }
  if(doc.entries.length>limit)warnings.push(name+': '+(doc.entries.length-limit)+' entries outside the current USA table are unused.');
 };
 for(const [file,mode,limit,kind] of [['misc.xml','message',63],['desc.xml','message',79],['item_simple.xml','item',129],['save.xml','menu',21,'save'],['ascii.xml','menu',8,'ascii']])validate(file,mode,limit,kind);
 for(const [name,source] of selected){
  if(/^text\//i.test(name))validate(name,'message',64,'room');
  if(/^file\//i.test(name)){
   if(fs.statSync(source).size>core.MAX_IMAGE_BYTES)fail(name+' is too large');
   const image=core.imageSize(new Uint8Array(fs.readFileSync(source)));
   if(image.w>256||image.h>256)fail(name+' exceeds the current GOG document-page surface; it needs an additional adapter.');documents++;
  }
  if(/^movie\//i.test(name))movies++;
 }
 if(!strings)fail('No supported translated strings were found');
 if(missing.size)warnings.push('Missing font characters use ? if encountered: '+[...missing].join(' '));
 warnings.push('Packaging and supported XML syntax checked; filenames, document mappings, codecs and full gameplay still need launcher/phone testing.');
 warnings.push('Keep the translation above SHDP and other font/UI packs. GOG prompt actions are preserved by the app.');
 if(movies===0)warnings.push('No movie replacements included. Original game videos remain in use.');
 const destination=path.resolve(options.out||folder+'-android.zip'),reportFile=destination.replace(/\.zip$/i,'.preparation.txt');
 if(path.extname(destination).toLowerCase()!=='.zip')fail('Output must end in .zip');
 for(const root of [folder,...(options.movies?[fs.realpathSync(options.movies)]:[])])if(inside(root,realDestination(destination))||inside(root,realDestination(reportFile)))fail('Write the output ZIP outside the input folders');
 for(const output of [destination,reportFile])if(fs.lstatSync(output,{throwIfNoEntry:false})?.isSymbolicLink())fail('Output ZIP/report must not be symlinks');
 if(!options.overwrite&&(fs.existsSync(destination)||fs.existsSync(reportFile)))fail('Output already exists. Choose another --out filename or add --overwrite to replace the generated ZIP/report.');
 if(selected.size>50000)fail('Too many mod files');
 const name=(/^\s*Name\s*=\s*(.+)$/im.exec(manifest)?.[1]||path.basename(folder)).replace(/[\x00-\x1f\x7f]/g,' ');
 const report=['RE1 Mod Helper - translation preparation report','Mod: '+name,'Files included: '+selected.size,'Supported text entries checked: '+strings,'Document PNGs: '+documents,'Movie replacements: '+movies,'','Notes:',...warnings.map(w=>'- '+w),'','Files left out:',...omitted.map(e=>'- '+e.path+' — '+e.reason),'','Source files were not rewritten. Game XML, images and media are copied unchanged. Description metadata may be normalized to UTF-8 in the ZIP, with the original retained in notices/.','Mod Helper checks and packages supported data. It does not repair incompatible mods or convert Windows DLL/ASI behaviour.','Original author credits and included notices are retained.'];
 fs.mkdirSync(path.dirname(reportFile),{recursive:true});
 const temporaryReport=reportFile+'.'+crypto.randomBytes(6).toString('hex')+'.tmp';
 try{
  fs.writeFileSync(temporaryReport,report.join('\n')+'\n',{encoding:'utf8',flag:'wx'});
  helpers.zip(folder,[...selected.keys()].sort(),destination,name=>generated.get(name)||selected.get(name));
  fs.renameSync(temporaryReport,reportFile);
 }finally{if(fs.existsSync(temporaryReport))fs.unlinkSync(temporaryReport);}
 console.log('Mod Helper packaged '+destination+'\nReport: '+reportFile+'\n'+strings+' text entries checked; '+documents+' document image(s); '+movies+' movie(s).\nInstall through Mods > +. Keep translations above conflicting font/UI packs. Test in game before sharing.');
 return {destination,reportFile,files:selected.size,strings,documents,movies};
}
module.exports={prepare};
