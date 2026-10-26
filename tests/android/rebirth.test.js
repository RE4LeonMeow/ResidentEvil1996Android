'use strict';
const {test}=require('node:test'),assert=require('node:assert/strict');
const r=require('../../android/app/src/main/assets/web/rebirth-core.js');
const core=require('../../android/app/src/main/assets/web/mod-core.js');
const font=()=>r.encoding(r.xml('<Encoding width="8" indent="0"><Entry Encode="0" Char=" "/><Entry Encode="0x1b" Char="?"/><Entry Encode="0x1d" Char="A"/><Entry Encode="0x3d" Char="a"/><Entry Encode="0x5f" Char="ą"/><Entry Encode="5" Char="("/><Entry Encode="0x38" Char="/"/></Encoding>'));
test('translation XML supports BOM, comments, entities and literal CDATA without execution',()=>{
 assert.deepEqual(r.xml('\uFEFF<?xml version="1.0"?><Strings><!-- note --><Text>ą &amp; &#x142; &lt;x&gt;</Text><Text/><Text><![CDATA[<script>&literal]]></Text></Strings>').entries,['ą & ł <x>','','<script>&literal']);
 for(const input of ['<!DOCTYPE Strings [<!ENTITY x SYSTEM "file:///secret">]><Strings><Text>&x;</Text></Strings>','<Strings><Text><script/></Text></Strings>','<Strings><Text>&unknown;</Text></Strings>','<Strings><Text>&#0;</Text></Strings>','<Encoding><Entry Encode="1" Encode="2"/></Encoding>','<Strings>'+ ' '.repeat(r.MAX_XML)+'</Strings>'])assert.throws(()=>r.xml(input));
});
test('reserved font codes cannot accidentally become message commands',()=>{
 const f=font();assert.ok(f.map.get('(')>=12);assert.equal(f.map.get('ą'),95);
 const value=r.compile('Aą(a',f,'item').bytes;
 assert.deepEqual([...value],[29,95,f.map.get('('),61,7,1,0]);
 assert.throws(()=>r.encoding(r.xml('<Encoding><Entry Encode="248" Char="?"/></Encoding>')));
});
test('message conversion retains reveal, page, item and color commands with a bounded native-action placeholder',()=>{
 const f=font();
 assert.deepEqual([...r.compile('{scroll 0}A{scroll 1}{clear 48}a\\ną{timed 53}',f).bytes],[4,0,29,4,1,3,48,61,2,95,1,53]);
 assert.deepEqual([...r.compile('A{timed 8}',f).bytes],[29,1,8]);
 const first=r.compile('{color 1}{string 0}{color 0}?{branch 2 10 0}overlapping next message',f).bytes;
 const second=r.compile('{color 1}{string 0}{color 0}?{branch 0 6 1}',f).bytes;
 assert.deepEqual(first,second);assert.deepEqual([...first.slice(0,8)],[5,1,6,0,5,0,27,8]);
 assert.throws(()=>r.compile('{scroll 0}A',f));assert.throws(()=>r.compile('{timed 20}A',f));
 assert.deepEqual(r.compile('{scroll 0}{scroll 0}A{scroll 1}',f).bytes,r.compile('{scroll 0}A{scroll 1}',f).bytes);
 for(const command of ['{clear 256}','{scroll -1}','{branch 2 999 1}','{unsupported 1}','{string 1 2}'])assert.throws(()=>r.compile(command,f));
});
test('save labels preserve the fixed reveal-cell size and fail rather than overflow',()=>{
 const value=r.compile('A{s}a{s}',font(),'menu',{cells:5}).bytes;
 assert.deepEqual([...value],[29,251,61,251,0,251,0,251,0,251,1,0]);
 assert.throws(()=>r.compile('AAAAAA',font(),'menu',{cells:5}));
 const tab=r.compile('A{pos 40}a',font(),'menu').bytes;assert.deepEqual([...tab],[29,0,0,0,0,61,1,0]);
});
test('compiled cache is versioned, bounded, and rejects duplicate room/scenario keys',()=>{
 const bytes=r.compile('A',font()).bytes,key=r.key(6,3,2,13,1),bin=r.binary([{key,bytes}]),view=new DataView(bin.buffer);
 assert.equal(view.getUint32(0,true),0x544c3152);assert.equal(view.getUint32(4,true),1);assert.equal(view.getUint32(8,true),1);
 assert.equal(view.getUint32(12,true),bin.length);assert.equal(view.getUint32(16,true),key);
 assert.notEqual(key,r.key(6,3,2,13,0));assert.notEqual(key,r.key(6,3,2,13));
 assert.throws(()=>r.binary([{key,bytes},{key,bytes}]));
});
test('document padding preserves RGBA and font conversion reads a separate source atlas',()=>{
 const original={w:256,h:256,rgba:new Uint8Array(256*256*4).fill(7)},atlas={w:256,h:256,rgba:new Uint8Array(256*256*4).fill(90)};
 const f=r.font(atlas,original,font().glyphs),idx=95,offset=((Math.floor(idx/18)+2)*14*256+(idx%18)*8)*4;
 assert.deepEqual([...f.rgba.slice(offset,offset+4)],[90,90,90,90]);assert.equal(f.rgba[0],7);assert.equal(original.rgba[offset],7);
 const padded=r.pad({w:1,h:1,rgba:Uint8Array.of(10,20,30,0)},2,2);assert.deepEqual([...padded.rgba],[10,20,30,0,0,0,0,0,0,0,0,0,0,0,0,0]);
 assert.throws(()=>r.pad({w:3,h:3,rgba:new Uint8Array(36)},2,2));
});
test('nested Rebirth archives fail clearly and OGG voice siblings match existing WAV paths',()=>{
 const e=(path,slot=0)=>({path,slot,size:10,crc:0});
 assert.throws(()=>core.manifest([e('mod_translation.7z'),e('Movie/pu.mp4',1)]),/nested Classic/);
 assert.equal(core.manifest([e('Pack/manifest.txt'),e('Pack/encoding.xml',1)]).format,'rebirth');
 const plan=core.pcPlan([e('USA/VOICE/LINE.ogg'),e('ddraw.dll',1),e('notes.xml',2)],'Dub.zip',[{path:'Voice/Line.wav'}]);
 assert.equal(plan.overrides[0].target,'Voice/Line.ogg');assert.equal(plan.compatibility.skipped.length,2);
 assert.match(plan.compatibility.skipped[0].reason,/not executed/);
});
