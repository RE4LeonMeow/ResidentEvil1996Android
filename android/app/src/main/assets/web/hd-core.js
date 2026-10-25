// Port-owned HD pack format and still-image decoder. No game assets are bundled.
(function(root){
  'use strict';
  const MAX_PIXELS=4*1024*1024, MAX_BYTES=16*1024*1024;
  function dimensions(w,h){
    if(!Number.isInteger(w)||!Number.isInteger(h)||w<1||h<1||w*h>MAX_PIXELS)throw Error('Invalid image dimensions');
    // The original scene is 320x240: 4.5x is 1440x1080. Atlas proportions and
    // UVs remain unchanged; bound unusually large sheets to 2048 per side.
    const scale=Math.min(4.5,2048/w,2048/h,Math.sqrt(MAX_PIXELS/(w*h)));
    return [Math.max(w,Math.floor(w*scale)),Math.max(h,Math.floor(h*scale))];
  }
  function key(pixels,w,h){
    let a=2166136261,b=5381,c=0x9e3779b9,d=0x85ebca6b;
    for(let i=0;i<pixels.length;i++){
      const v=pixels[i];a=Math.imul(a^v,16777619);b=Math.imul(b,33)^v;
      c=Math.imul(c^v,0x5bd1e995);d=Math.imul(d+v,0x27d4eb2d)^(d>>>15);
    }
    return [a,b,c,d].map(v=>(v>>>0).toString(16).padStart(8,'0')).join('')+'_'+w+'x'+h;
  }
  function weights(src,dst){
    const indices=new Int32Array(dst*4),values=new Float32Array(dst*4);
    for(let x=0;x<dst;x++){
      const at=(x+.5)*src/dst-.5,start=Math.floor(at)-1;let sum=0;
      for(let k=0;k<4;k++){
        const distance=Math.abs(at-start-k);
        const v=distance<1e-8?1:distance>=2?0:Math.sin(Math.PI*distance)*Math.sin(Math.PI*distance/2)/(Math.PI*Math.PI*distance*distance/2);
        indices[x*4+k]=Math.max(0,Math.min(src-1,start+k));values[x*4+k]=v;sum+=v;
      }
      for(let k=0;k<4;k++)values[x*4+k]/=sum;
    }
    return {indices,values};
  }
  function upscale(rgba,w,h){
    if(rgba.length!==w*h*4)throw Error('Invalid RGBA image');
    const [ow,oh]=dimensions(w,h),wx=weights(w,ow),wy=weights(h,oh);
    const temp=new Float32Array(ow*h*4),out=new Uint8Array(ow*oh*4);
    // Premultiplied Lanczos2 prevents dark fringes. Preserve original alpha
    // coverage with nearest samples: masks must still occlude the same pixels.
    for(let y=0;y<h;y++)for(let x=0;x<ow;x++){
      const to=(y*ow+x)*4;
      for(let k=0;k<4;k++){
        const from=(y*w+wx.indices[x*4+k])*4,v=wx.values[x*4+k],alpha=rgba[from+3]/255;
        for(let c=0;c<3;c++)temp[to+c]+=rgba[from+c]*alpha*v;
        temp[to+3]+=alpha*v;
      }
    }
    for(let y=0;y<oh;y++)for(let x=0;x<ow;x++){
      const to=(y*ow+x)*4,nearest=(Math.min(h-1,Math.floor((y+.5)*h/oh))*w+Math.min(w-1,Math.floor((x+.5)*w/ow)))*4;
      let r=0,g=0,b=0,a=0;
      for(let k=0;k<4;k++){
        const from=(wy.indices[y*4+k]*ow+x)*4,v=wy.values[y*4+k];
        r+=temp[from]*v;g+=temp[from+1]*v;b+=temp[from+2]*v;a+=temp[from+3]*v;
      }
      out[to+3]=rgba[nearest+3];
      if(a>1e-6&&out[to+3]){
        out[to]=Math.max(0,Math.min(255,Math.round(r/a)));out[to+1]=Math.max(0,Math.min(255,Math.round(g/a)));out[to+2]=Math.max(0,Math.min(255,Math.round(b/a)));
      }
    }
    return {rgba:out,w:ow,h:oh};
  }
  function unpackPak(src){
    let pos=0,bit=0,bits=9,next=259,outAt=0;
    const out=new Uint8Array(MAX_BYTES),prefix=new Uint16Array(65536),suffix=new Uint8Array(65536),stack=new Uint8Array(65537);
    function code(){
      if(pos*8+bit+bits>src.length*8)throw Error('Truncated PAK');
      let result=0;
      for(let n=0;n<bits;n++){result=(result<<1)|((src[pos]>>(7-bit))&1);if(++bit===8){bit=0;pos++;}}
      return result;
    }
    function emit(v){if(outAt>=out.length)throw Error('Oversized PAK');out[outAt++]=v;}
    for(;;){
      bits=9;next=259;let previous=code();if(previous===256)return out.slice(0,outAt);
      if(previous>255)throw Error('Invalid PAK start');emit(previous);
      for(;;){
        const current=code();if(current===256)return out.slice(0,outAt);if(current===258)break;
        if(current===257){if(++bits>16)throw Error('Invalid PAK code width');continue;}
        if(current>next||current>=65536)throw Error('Invalid PAK dictionary');
        let lookup=current===next?previous:current,count=0;
        while(lookup>255){if(lookup>=next||count>=65536)throw Error('Invalid PAK chain');stack[count++]=suffix[lookup];lookup=prefix[lookup];}
        const first=lookup;emit(first);while(count)emit(stack[--count]);if(current===next)emit(first);
        if(next>=65536)throw Error('PAK dictionary overflow');prefix[next]=previous;suffix[next++]=first;previous=current;
      }
    }
  }
  function tim(data,offset){
    const v=new DataView(data.buffer,data.byteOffset,data.byteLength);
    if(offset+20>data.length||v.getUint32(offset,true)!==16)throw Error('Not TIM');
    const flags=v.getUint32(offset+4,true),mode=flags&7;
    if(flags>11||mode>3||((mode<2)!==!!(flags&8)))throw Error('Invalid TIM mode');
    let p=offset+8,palette=null;
    if(mode<2){
      if(p+12>data.length)throw Error('Truncated TIM palette');
      const size=v.getUint32(p,true),pw=v.getUint16(p+8,true),ph=v.getUint16(p+10,true);
      if(size!==12+pw*ph*2||p+size>data.length||pw*ph<(mode?256:16))throw Error('Invalid TIM palette');
      palette={offset:p+12,count:pw*ph};p+=size;
    }
    if(p+12>data.length)throw Error('Truncated TIM pixels');
    const size=v.getUint32(p,true),words=v.getUint16(p+8,true),h=v.getUint16(p+10,true);
    const w=mode===0?words*4:mode===1?words*2:mode===2?words:Math.floor(words*2/3);
    // Retail PC backgrounds store pixel bytes alone in this length field;
    // ordinary PSX TIMs include the 12-byte block header. The engine uses the
    // declared row dimensions for the upload in both cases.
    const pixelBytes=words*h*2;
    if(!w||!h||w*h>MAX_PIXELS||(size!==12+pixelBytes&&size!==pixelBytes)||p+12+pixelBytes>data.length)throw Error('Invalid TIM pixels');
    return {data,v,mode,w,h,rowBytes:words*2,pixels:p+12,palette,end:p+12+pixelBytes};
  }
  function decode(t,row=0,rule='page'){
    const {w,h,v,mode}=t,rgba=new Uint8Array(w*h*4),entries=mode?256:16;
    function colour(at,value,index){
      rgba[at]=Math.floor((value&31)*255/31);rgba[at+1]=Math.floor(((value>>>5)&31)*255/31);rgba[at+2]=Math.floor(((value>>>10)&31)*255/31);
      let transparent=false;
      if(rule!=='opaque'){
        if(rule==='effect')transparent=value===0;
        else if(rule==='index')transparent=index===0;
        else if(mode<2){const first=v.getUint16(t.palette.offset+row*entries*2,true);transparent=(first&32767)===0?index===0:(value&32767)===0;}
        else transparent=(value&32767)===0;
      }
      rgba[at+3]=transparent?0:255;
    }
    if(mode<2&&(!Number.isInteger(row)||row<0||(row+1)*entries>t.palette.count))throw Error('Invalid TIM palette row');
    for(let y=0;y<h;y++)for(let x=0;x<w;x++){
      const at=(y*w+x)*4,src=t.pixels+y*t.rowBytes;
      if(mode===3){rgba[at]=t.data[src+x*3+2];rgba[at+1]=t.data[src+x*3+1];rgba[at+2]=t.data[src+x*3];rgba[at+3]=255;}
      else if(mode===2)colour(at,v.getUint16(src+x*2,true),-1);
      else{
        const byte=t.data[src+(mode?x:x>>>1)],index=mode?byte:(x&1?byte>>>4:byte&15);
        colour(at,v.getUint16(t.palette.offset+(row*entries+index)*2,true),index);
      }
    }
    return rgba;
  }
  function* images(data,path,seenBlocks){
    const isPak=/\.pak$/i.test(path);if(isPak)data=unpackPak(data);
    let found=0;
    for(let p=0;p+20<=data.length;p++){
      if(data[p]!==16||data[p+1]||data[p+2]||data[p+3])continue;
      let t;try{t=tim(data,p);}catch(_){continue;}
      found++;
      if(seenBlocks){
        const signature=(isPak&&t.mode>=2?'base:':'page:')+key(data.subarray(p,t.end),t.w,t.h);
        if(seenBlocks.has(signature)){p=t.end-1;continue;}seenBlocks.add(signature);
      }
      if(t.mode>=2){yield {w:t.w,h:t.h,rgba:decode(t,0,isPak?'opaque':'page')};if(!isPak)yield {w:t.w,h:t.h,rgba:decode(t,0,'opaque')};}
      else{
        const rows=Math.min(256,Math.floor(t.palette.count/(t.mode?256:16)));
        for(let row=0;row<rows;row++){
          for(const rule of row===0?['page','effect','opaque']:['index','effect'])yield {w:t.w,h:t.h,rgba:decode(t,row,rule)};
        }
      }
      p=t.end-1;
    }
    if(isPak&&!found)throw Error('No supported image in PAK');
  }
  const api={dimensions,key,upscale,unpackPak,tim,decode,images,MAX_BYTES,MAX_PIXELS};
  root.RE1HDCore=api;if(typeof module!=='undefined')module.exports=api;
})(globalThis);
