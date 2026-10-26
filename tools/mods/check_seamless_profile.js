#!/usr/bin/env node
// Optional developer validation against a privately supplied GOG folder.
// The output profile contains hashes and resource paths, never game pixels.
'use strict';
const fs=require('node:fs'),path=require('node:path');
const core=require('../../android/app/src/main/assets/web/hd-core.js');
const [folder,input,output]=process.argv.slice(2);
if(!folder||!input)throw Error('Usage: node tools/mods/check_seamless_profile.js GAME_FOLDER PROFILE.json [OUTPUT.json]');
const profile=JSON.parse(fs.readFileSync(input,'utf8')),records=new Map(profile.records.map(r=>[r.key,r]));
const files=[];
function scan(root,prefix=''){
  for(const entry of fs.readdirSync(root,{withFileTypes:true})){
    if(entry.isDirectory())scan(path.join(root,entry.name),prefix+entry.name+'/');
    else if(/\.(pak|tim|ivm|emd|emw|rdt|dor|etm|ptc|pix)$/i.test(entry.name)&&fs.statSync(path.join(root,entry.name)).size<=core.MAX_BYTES)files.push({file:path.join(root,entry.name),name:prefix+entry.name});
  }
}
scan(folder);files.sort((a,b)=>a.name.localeCompare(b.name));
const seen=new Set(),blocks=new Set(),counts={};let matched=0,images=0,failed=0;
for(const entry of files){
  try{
    for(const image of core.images(new Uint8Array(fs.readFileSync(entry.file)),entry.name,blocks)){
      if(image.w<4||image.h<4)continue;
      const key=core.key(image.rgba,image.w,image.h);if(seen.has(key))continue;seen.add(key);images++;
      const record=records.get(key);
      if(record){record.path=entry.name;matched++;const group=record.file.split('/')[1];counts[group]=(counts[group]||0)+1;}
    }
  }catch(error){failed++;}
}
if(output)fs.writeFileSync(output,JSON.stringify(profile));
console.log(JSON.stringify({files:files.length,uniqueOriginalImages:images,matchedOriginalImages:matched,matchedByGroup:counts,unsupportedContainers:failed},null,2));
