// Port-owned Classic REbirth data adapter. XML is data; plugins are never run.
(function(root){
  'use strict';
  const MAX_XML=2*1024*1024,MAX_RECORDS=4096,MAX_TEXT=8192;
  const fail=message=>{throw Error('Classic REbirth: '+message);};
  function entities(value){
    if(/&(?!(?:amp|lt|gt|quot|apos|#[0-9]+|#x[0-9a-f]+);)/i.test(value))fail('Malformed XML entity.');
    return value.replace(/&([^;\s]{1,30});/g,(_,name)=>{
      const known={amp:'&',lt:'<',gt:'>',quot:'"',apos:"'"};if(name in known)return known[name];
      if(!/^#(?:[0-9]+|x[0-9a-f]+)$/i.test(name))fail('Unknown XML entity.');
      const n=name[1].toLowerCase()==='x'?parseInt(name.slice(2),16):Number(name.slice(1));
      if(!Number.isInteger(n)||n<32&&![9,10,13].includes(n)||n>0x10ffff||n>=0xd800&&n<=0xdfff)fail('Invalid XML character.');
      return String.fromCodePoint(n);
    });
  }
  function attributes(text){
    const result={};let rest=text.trim();
    while(rest){
      const m=/^([A-Za-z][\w-]*)\s*=\s*(["'])([\s\S]*?)\2(?:\s+|$)/.exec(rest);
      if(!m||Object.hasOwn(result,m[1]))fail('Invalid or duplicate XML attribute.');
      result[m[1]]=entities(m[3]);rest=rest.slice(m[0].length).trimStart();
    }
    return result;
  }
  function xml(input){
    if(typeof input!=='string'||input.length>MAX_XML||/[\x00-\x08\x0b\x0c\x0e-\x1f]/.test(input))fail('Invalid or oversized XML.');
    let text=input.replace(/^\uFEFF/,'').replace(/\r\n?/g,'\n');
    if(/<!\s*(?:DOCTYPE|ENTITY)/i.test(text))fail('XML document types and external entities are unsupported.');
    text=text.replace(/<!--(?:(?!-->)[\s\S])*-->/g,'').replace(/^\s*<\?xml[^?]*\?>/,'').trim();
    const rootMatch=/^<(Strings|Encoding)\b([^>]*)>([\s\S]*)<\/\1>\s*$/.exec(text);
    if(!rootMatch)fail('Use a Strings or Encoding XML document.');
    const [,kind,attrs,body]=rootMatch,result={kind,attributes:attributes(attrs),entries:[]};let at=0;
    while(at<body.length){
      const blank=/^\s*/.exec(body.slice(at))[0];at+=blank.length;if(at===body.length)break;
      const m=kind==='Strings'?/^<Text\s*(?:\/>|>([\s\S]*?)<\/Text\s*>)/.exec(body.slice(at)):/^<Entry\s+([^>]*?)\s*\/>/.exec(body.slice(at));
      if(!m)fail('Unsupported or malformed XML element.');
      if(kind==='Strings'){
        let value=m[1]||'';
        value=value.replace(/<!\[CDATA\[([\s\S]*?)\]\]>/g,(_,v)=>v.replace(/&/g,'&amp;').replace(/</g,'&lt;'));
        if(value.includes('<'))fail('Nested markup in Text is unsupported.');
        result.entries.push(entities(value));
      }else result.entries.push(attributes(m[1]));
      if(result.entries.length>MAX_RECORDS)fail('Too many XML entries.');at+=m[0].length;
    }
    return result;
  }
  function number(value,min=0,max=255){
    if(typeof value!=='string'||! /^(?:0x[0-9a-f]+|\d+)$/i.test(value)||!Number.isInteger(Number(value))||Number(value)<min||Number(value)>max)fail('Invalid numeric value: '+value);
    return Number(value);
  }
  function encoding(document){
    if(document.kind!=='Encoding')fail('encoding.xml needs an Encoding root.');
    const map=new Map(),glyphs=new Map(),used=new Set();let next=176;
    for(const entry of document.entries){
      const source=number(entry.Encode),character=entry.Char;
      if(typeof character!=='string'||[...character].length!==1||source>=248)fail('Invalid character mapping.');
      used.add(source);
    }
    for(const entry of document.entries){
      const source=number(entry.Encode),character=entry.Char;
      if(map.has(character))continue;
      let index=source;
      if(source>0&&source<12){while(used.has(next)&&next<248)next++;if(next>=248)fail('Font has no free character slots.');index=next++;used.add(index);}
      const width=number(entry.width||document.attributes.width||'8',1,14),indent=number(entry.indent||document.attributes.indent||'0',0,14);
      map.set(character,index);glyphs.set(index,{index,source,width,indent});
    }
    // Common punctuation aliases do not replace any author's character map.
    for(const [a,b] of [['\\','/'],['\u201c','"'],['\u201d','"'],['\u2019',"'"],['\u00a0',' ']])if(!map.has(a)&&map.has(b))map.set(a,map.get(b));
    if(!map.has('?')||map.get(' ')!==0)fail('Font must map space to 0 and include a question mark.');
    return {map,glyphs:[...glyphs.values()]};
  }
  function tokens(text){
    if(typeof text!=='string'||text.length>MAX_TEXT)fail('Text is too long.');
    const out=[];let at=0;
    for(const match of text.matchAll(/\{([^{}]+)\}|\\n/g)){
      if(match.index>at)out.push({text:text.slice(at,match.index)});
      out.push(match[0]==='\\n'?{command:'newline',args:[]}:{command:match[1].trim().split(/\s+/)[0],args:match[1].trim().split(/\s+/).slice(1)});at=match.index+match[0].length;
    }
    if(at<text.length)out.push({text:text.slice(at)});
    if(out.some(t=>t.text&&/[{}]/.test(t.text)))fail('Unrecognized text markup.');return out;
  }
  function compile(text,font,mode='message',options={}){
    const bytes=[],missing=new Set();let dismiss=0,ended=false,scrollOpen=false,x=0;
    const append=value=>{bytes.push(value);if(bytes.length>MAX_TEXT)fail('Encoded text is too long.');};
    const chars=value=>{
      for(const ch of value){
        if(ch==='\n'||ch==='\r'||ch==='\t')continue;
        const code=font.map.get(ch);if(code===undefined)missing.add(ch);
        append(code??font.map.get('?'));x+=8;
      }
    };
    for(const token of tokens(text)){
      // REbirth's XML dumps can include text from the next overlapping
      // message after a choice. Native RE1 returns after that choice.
      if(ended){if(ended!=='branch'&&token.text?.trim())fail('Text follows an end marker.');continue;}
      if(token.text!==undefined){chars(token.text);continue;}
      const {command,args}=token;
      if(command==='newline'){if(mode!=='message')fail('Multiline menu text is unsupported.');append(2);x=0;continue;}
      if(command==='center'&&mode==='menu'&&!args.length)continue;
      if((command==='tab'||command==='pos')&&mode==='menu'&&args.length===1){const to=number(args[0],0,320);while(x<to){append(0);x+=8;}continue;}
      if(command==='s'&&mode==='menu'&&!args.length){append(251);continue;}
      if(command==='color'&&mode==='menu'&&args.length===1){number(args[0]);continue;}
      if(command==='timed'&&mode==='message'&&args.length===1){dismiss=number(args[0]);if(!dismiss)fail('A timed end needs a positive delay.');ended=true;continue;}
      if(command==='branch'&&mode==='message'&&args.length===3){args.forEach(a=>number(a));append(8);ended='branch';continue;}
      const codes={clear:3,scroll:4,color:5,string:6};
      if(Object.hasOwn(codes,command)&&args.length===1&&mode==='message'){
        const value=number(args[0]);
        if(command==='scroll'){if(value===0){if(scrollOpen)continue;scrollOpen=true;}else scrollOpen=false;}
        append(codes[command]);append(value);continue;
      }
      fail('Unsupported text command: '+command);
    }
    if(scrollOpen)fail('Unterminated zero-speed text section.');
    if(mode==='item'){append(7);append(1);append(0);}
    else {append(1);append(dismiss);}
    // Branch destinations come from the original game, never the other
    // region's XML. The native adapter fills this bounded placeholder.
    if(ended==='branch')for(let n=0;n<6;n++)append(0);
    if(options.cells){
      const visible=bytes.slice(0,bytes.indexOf(1)).filter(v=>v!==251);
      if(visible.length>options.cells)fail('Save-screen text exceeds its fixed cell count.');
      bytes.length=0;for(let i=0;i<options.cells;i++)bytes.push(visible[i]||0,251);bytes.push(1,0);
    }
    return {bytes:Uint8Array.from(bytes),missing:[...missing]};
  }
  function key(kind,index,stage=0,room=0,variant=2){return ((kind<<28)|(stage<<24)|(room<<16)|(variant<<14)|index)>>>0;}
  function binary(records){
    if(!Array.isArray(records)||records.length>MAX_RECORDS)fail('Too many translated strings.');
    let size=16;const keys=new Set();for(const r of records){if(keys.has(r.key)||!(r.bytes instanceof Uint8Array)||r.bytes.length>MAX_TEXT)fail('Invalid or duplicate translated string.');keys.add(r.key);size+=8+r.bytes.length;}
    if(size>2*1024*1024)fail('Translation cache exceeds 2 MiB.');
    const bytes=new Uint8Array(size),v=new DataView(bytes.buffer);bytes.set([82,49,76,84]);v.setUint32(4,1,true);v.setUint32(8,records.length,true);v.setUint32(12,size,true);
    let at=16;for(const r of records){v.setUint32(at,r.key,true);v.setUint32(at+4,r.bytes.length,true);bytes.set(r.bytes,at+8);at+=8+r.bytes.length;}return bytes;
  }
  function pad(image,w,h){
    if(image.w>w||image.h>h||w*h>4*1024*1024)fail('Document image is larger than its original page.');
    const rgba=new Uint8Array(w*h*4);for(let y=0;y<image.h;y++)rgba.set(image.rgba.subarray(y*image.w*4,(y+1)*image.w*4),y*w*4);return {w,h,rgba};
  }
  function font(atlas,original,glyphs){
    if(atlas.w!==256||atlas.h!==256||original.w!==256||original.h!==256)fail('This font layout is unsupported (expected 256×256 PBFONT0 and GOG font).');
    const rgba=original.rgba.slice();
    for(const g of glyphs){
      if(g.index===0||g.index<12||g.source>=18*16)continue;
      const sx=g.source%18*14,sy=(Math.floor(g.source/18)+2)*14,dx=g.index%18*8,dy=(Math.floor(g.index/18)+2)*14;
      if(sy+14>256||dy+14>256)fail('Font glyph lies outside the atlas.');
      for(let y=0;y<14;y++)for(let x=0;x<8;x++){
        const from=((sy+y)*256+sx+x)*4,to=((dy+y)*256+dx+x)*4;rgba.set(atlas.rgba.subarray(from,from+4),to);
      }
    }
    return {w:256,h:256,rgba};
  }
  const api={xml,encoding,compile,key,binary,pad,font,MAX_XML,MAX_RECORDS};
  root.RE1Rebirth=api;if(typeof module!=='undefined')module.exports=api;
})(globalThis);
