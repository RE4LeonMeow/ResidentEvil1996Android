#!/usr/bin/env node
// Offline mod authoring; Node's built-in modules only. No bundled game data.
'use strict';
const fs=require('node:fs'),path=require('node:path'),zlib=require('node:zlib'),crypto=require('node:crypto');
const core=require('../../android/app/src/main/assets/web/mod-core.js');
const hd=require('../../android/app/src/main/assets/web/hd-core.js');
const fail=message=>{throw Error(message);};
function args(values){
 const positional=[],options={};
 for(let n=0;n<values.length;n++)if(values[n].startsWith('--')){const name=values[n].slice(2);if(name==='overwrite')options[name]=true;else{if(!values[n+1]||values[n+1].startsWith('--'))fail('Missing value for --'+name);options[name]=values[++n];}}else positional.push(values[n]);
 return {positional,options};
}
function safe(root,name,exists=true){
 name=core.path(name);const file=path.resolve(root,...name.split('/')),base=path.resolve(root)+path.sep;
 if(!file.startsWith(base))fail('Path leaves the mod folder: '+name);
 let at=path.resolve(root);for(const part of name.split('/')){at=path.join(at,part);if(fs.existsSync(at)&&fs.lstatSync(at).isSymbolicLink())fail('Symlinks are not supported: '+name);}
 if(exists&&!fs.statSync(file).isFile())fail('Not a regular file: '+name);return file;
}
function readManifest(folder){return JSON.parse(fs.readFileSync(path.join(folder,'mod.json'),'utf8'));}
function writeManifest(folder,json){fs.writeFileSync(path.join(folder,'mod.json'),JSON.stringify(json,null,2)+'\n');}
function png(w,h,rgba){
 const chunk=(name,body)=>{const tag=Buffer.from(name),head=Buffer.alloc(4),crc=Buffer.alloc(4);head.writeUInt32BE(body.length);crc.writeUInt32BE(core.crc32(Buffer.concat([tag,body])));return Buffer.concat([head,tag,body,crc]);};
 const header=Buffer.alloc(13);header.writeUInt32BE(w);header.writeUInt32BE(h,4);header[8]=8;header[9]=6;
 const raw=Buffer.alloc((w*4+1)*h);for(let y=0;y<h;y++)raw.set(rgba.subarray(y*w*4,(y+1)*w*4),y*(w*4+1)+1);
 return Buffer.concat([Buffer.from([137,80,78,71,13,10,26,10]),chunk('IHDR',header),chunk('IDAT',zlib.deflateSync(raw)),chunk('IEND',Buffer.alloc(0))]);
}
function gameFile(root,name){
 let at=path.resolve(root);
 for(const part of core.gamePath(name).split('/')){const match=fs.readdirSync(at).filter(n=>n.toLowerCase()===part.toLowerCase());if(match.length!==1)fail('Game resource not found or ambiguous: '+name);at=path.join(at,match[0]);}
 if(fs.statSync(at).size>hd.MAX_BYTES)fail('Texture source exceeds 16 MiB');return at;
}
function image(data,name,index){
 if(/\.pak$/i.test(name))data=hd.unpackPak(data);let number=0;
 for(let at=0;at+20<=data.length;at++){
  if(data[at]!==16||data[at+1]||data[at+2]||data[at+3])continue;
  let tim;try{tim=hd.tim(data,at);}catch{continue;}
  if(number++===index)return tim;at=tim.end-1;
 }fail('Image '+index+' was not found in this container');
}
function scan(folder,prefix=''){
 const out=[];for(const entry of fs.readdirSync(path.join(folder,prefix),{withFileTypes:true})){
  const name=prefix+entry.name;if(entry.isSymbolicLink())fail('Remove symlinks before building a mod');
  if(entry.isDirectory()){if(entry.name!=='.git')out.push(...scan(folder,name+'/'));}
  else if(entry.isFile()){const stat=fs.statSync(safe(folder,name));out.push({path:name,slot:out.length,size:stat.size,crc:0});}
 }return out;
}
// ZIP32, stored entries: no dependencies and no recompression/quality changes.
// Stream each file so a video/sound mod need not fit into one Node Buffer.
function zip(folder,names,destination,resolveSource){
 if(path.extname(destination).toLowerCase()!=='.zip')fail('Output must end in .zip');
 fs.mkdirSync(path.dirname(destination),{recursive:true});
 const temporary=destination+'.'+crypto.randomBytes(6).toString('hex')+'.tmp',fd=fs.openSync(temporary,'wx'),central=[];
 let offset=0,closed=false;
 const table=Uint32Array.from({length:256},(_,v)=>{for(let i=0;i<8;i++)v=v&1?(v>>>1)^0xedb88320:v>>>1;return v>>>0;});
 const write=bytes=>{let at=0;while(at<bytes.length){const n=fs.writeSync(fd,bytes,at,bytes.length-at,offset+at);if(!n)fail('Incomplete ZIP write');at+=n;}offset+=bytes.length;};
 try{
  for(const name of names){
   const source=resolveSource?resolveSource(name):safe(folder,name),size=fs.statSync(source).size,encoded=Buffer.from(name),start=offset;
   if(size>1024**3||offset+size+encoded.length+128>0xffffffff)fail('Pack exceeds the supported ZIP32 size');
   const head=Buffer.alloc(30);head.writeUInt32LE(0x04034b50);head.writeUInt16LE(20,4);head.writeUInt16LE(2048,6);head.writeUInt16LE(33,12);head.writeUInt32LE(size,18);head.writeUInt32LE(size,22);head.writeUInt16LE(encoded.length,26);write(head);write(encoded);
   const input=fs.openSync(source,'r'),buffer=Buffer.alloc(262144);let crc=0xffffffff,count=0;
   try{for(let n;(n=fs.readSync(input,buffer,0,buffer.length,null))>0;){for(let i=0;i<n;i++)crc=table[(crc^buffer[i])&255]^(crc>>>8);write(buffer.subarray(0,n));count+=n;}}finally{fs.closeSync(input);}
   if(count!==size)fail('A source file changed while building: '+name);crc=(crc^0xffffffff)>>>0;
   const patch=Buffer.alloc(4);patch.writeUInt32LE(crc);fs.writeSync(fd,patch,0,4,start+14);
   const c=Buffer.alloc(46);c.writeUInt32LE(0x02014b50);c.writeUInt16LE(20,4);c.writeUInt16LE(20,6);c.writeUInt16LE(2048,8);c.writeUInt16LE(33,14);c.writeUInt32LE(crc,16);c.writeUInt32LE(size,20);c.writeUInt32LE(size,24);c.writeUInt16LE(encoded.length,28);c.writeUInt32LE(start,42);central.push(Buffer.concat([c,encoded]));
  }
  const at=offset;for(const item of central)write(item);
  if(offset+22>0xffffffff)fail('Pack exceeds ZIP32 size');
  const end=Buffer.alloc(22);end.writeUInt32LE(0x06054b50);end.writeUInt16LE(names.length,8);end.writeUInt16LE(names.length,10);end.writeUInt32LE(offset-at,12);end.writeUInt32LE(at,16);write(end);
  fs.fsyncSync(fd);fs.closeSync(fd);closed=true;fs.renameSync(temporary,destination);
 }finally{if(!closed)fs.closeSync(fd);if(fs.existsSync(temporary))fs.unlinkSync(temporary);}
}
function main(values){
 const {positional,options}=args(values),[command,dir]=positional;
 if(!dir||!['new','texture','build','rebirth'].includes(command))fail('Usage:\n  node tools/mods/pack.js new FOLDER --name "My pack" --author "Your name"\n  node tools/mods/pack.js texture FOLDER --game "GOG/USA" --source "Stage1/RC1000.pak"\n  node tools/mods/pack.js build FOLDER [--out "my-pack.zip"]\n  node tools/mods/pack.js rebirth EXTRACTED_FOLDER [--movies MOVIE_FOLDER] [--out "mod-android.zip"] [--overwrite]');
 const folder=path.resolve(dir);
 if(command==='rebirth'){if(positional.length!==2)fail('Quote folder paths with spaces; use --movies for a separate Movie folder.');require('./prepare-rebirth.js').prepare(folder,options,{scan,zip,safe});return;}
 if(command==='new'){
  if(fs.existsSync(folder)&&fs.readdirSync(folder).length)fail('Choose a new or empty mod folder');
  const id=options.id||path.basename(folder).toLowerCase().replace(/[^a-z0-9._-]+/g,'-').replace(/^[^a-z0-9]+/,'');
  if(!/^[a-z0-9][a-z0-9._-]{0,79}$/.test(id))fail('Choose --id with 1-80 lowercase letters/numbers/dots/hyphens/underscores');
  fs.mkdirSync(folder,{recursive:true});writeManifest(folder,{format:'re1-android-mod',formatVersion:1,game:'re1-usa',id,name:options.name||path.basename(folder),author:options.author||'Your name',version:'1.0',description:'My texture pack',credits:'',textures:[]});
  console.log('Created '+folder+'\nNext: export a texture with the texture command.');return;
 }
 const json=readManifest(folder);
 if(command==='texture'){
  if(!options.game||!options.source)fail('texture needs --game and --source');
  const source=core.gamePath(options.source.replace(/\\/g,'/')),number=Number(options.image||0),palette=Number(options.palette||0);
  if(!Number.isInteger(number)||number<0||number>4095)fail('Invalid --image index');
  const tim=image(new Uint8Array(fs.readFileSync(gameFile(options.game,source))),source,number),rule=options.rule||(/\.pak$/i.test(source)&&tim.mode>=2?'opaque':'page');
  if(!['page','opaque','index','effect'].includes(rule))fail('Unknown --rule');
  const rgba=hd.decode(tim,palette,rule),file='textures/'+source+(number||palette?'.i'+number+'.p'+palette:'')+'.png',target=safe(folder,file,false);
  if(fs.existsSync(target)&&!options.overwrite)fail('That PNG already exists. Use another folder or --overwrite to replace it');
  fs.mkdirSync(path.dirname(target),{recursive:true});fs.writeFileSync(target,png(tim.w,tim.h,rgba));
  json.textures??=[];json.textures=json.textures.filter(item=>item.file!==file);json.textures.push({source,file,image:number,palette,rule,originalSize:[tim.w,tim.h]});writeManifest(folder,json);
  console.log('Exported '+target+' ('+tim.w+'x'+tim.h+')\nEdit this PNG, keep its proportions and filename, then build the ZIP.');return;
 }
 const entries=scan(folder).map((entry,slot)=>({...entry,slot}));core.manifest(entries);const plan=core.plan(json,entries);
 const selected=new Set(['mod.json']);for(const record of [...plan.textures,...plan.overrides])selected.add(record.file.path);
 for(const definition of json.textures||[]){
  if(!Array.isArray(definition.originalSize))continue;
  const bytes=new Uint8Array(fs.readFileSync(safe(folder,definition.file))),size=core.imageSize(bytes);core.dimensions(...definition.originalSize,size.w,size.h);
 }
 for(const name of ['README.txt','LICENSE.txt'])if(fs.existsSync(path.join(folder,name)))selected.add(name);
 const destination=path.resolve(options.out||folder+'.zip');zip(folder,[...selected].sort(),destination);
 console.log('Built '+destination+'\n'+plan.textures.length+' texture(s), '+plan.overrides.length+' data file(s). Install through Mods > +.');
}
if(require.main===module){try{main(process.argv.slice(2));}catch(error){console.error('Error: '+error.message);process.exitCode=1;}}
module.exports={main,png};
