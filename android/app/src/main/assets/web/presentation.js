/* SPDX-License-Identifier: GPL-3.0-or-later
 * Pan/viewport math adapted from FusionFix v1.1.0, ClassicPresentation.hxx
 * (blob bf5a1c5c0d3b7f7e40a8c2b7aa8a52812f59364a), FusionFix contributors.
 * https://github.com/Fusion-Fix/ResidentEvilClassicTrilogy.FusionFix
 * Android adaptation crops the completed frame so backgrounds, masks and
 * geometry share exactly one transform. Inventory is rearranged by the renderer;
 * other menus, movies and scripted scenes remain complete. Door views stay
 * centered; interaction text is reflowed while its camera crop stays fixed.
 * Transition hints hold the preceding framing.
 */
(function(root){
  'use strict';
  const clamp=(v,lo,hi)=>Math.max(lo,Math.min(hi,v));
  class Pan {
    constructor(){this.camera=null;this.position=30;}
    center(camera,visible=180){this.camera=camera;return this.position=Math.max(240-visible,0)/2;}
    bottom(camera,visible=180){this.camera=camera;return this.position=Math.max(240-visible,0);}
    freeze(playerY,camera,visible=180){
      const limit=Math.max(240-visible,0);
      if(this.camera!==camera)this.position=clamp((Number.isFinite(playerY)?playerY:120)-visible/2,0,limit);
      this.camera=camera;return this.position=clamp(this.position,0,limit);
    }
    update(playerY,camera,seconds,visible=180){
      const limit=Math.max(240-visible,0);
      const target=clamp((Number.isFinite(playerY)?playerY:120)-visible/2,0,limit);
      if(this.camera!==camera)this.position=target;
      else{
        const diff=target-this.position;
        const travel=Math.sign(diff)*Math.max(Math.abs(diff)-8,0);
        this.position+=travel*(1-Math.exp(-6*clamp(seconds,0,.1)));
      }
      this.camera=camera;return this.position=clamp(this.position,0,limit);
    }
  }
  function viewport(width,height,wide){
    const aspect=wide?clamp(width/height,4/3,16/9):4/3;
    const w=Math.min(width,height*aspect),h=w/aspect;
    return {x:(width-w)/2,y:(height-h)/2,width:w,height:h,visible:Math.min(240,320*h/w)};
  }
  function imageRect(view,crop,pan=0){
    const scale=crop?view.width/320:Math.min(view.width/320,view.height/240);
    return {x:(view.width-320*scale)/2,y:crop?-clamp(pan,0,240-view.visible)*scale:(view.height-240*scale)/2,width:320*scale,height:240*scale};
  }
  function menuExtra(view){return Math.max(0,240*view.width/view.height-320);}
  const api={Pan,viewport,imageRect,menuExtra};
  if(typeof module!=='undefined')module.exports=api;
  if(!root.document)return;
  const frame=document.getElementById('game-frame'),canvas=document.getElementById('canvas');
  let wide=localStorage.getItem('re1.aspect')==='16:9',mode=0,playerY=120,camera=0;
  let view,previousTime=performance.now(),previousStyle='',firstFrame=true;const pan=new Pan();
  function draw(seconds=0){
    const crop=wide&&mode>=1&&mode<=3;
    const offset=!crop?0:mode===1?pan.update(playerY,camera,seconds,view.visible):
      mode===3?pan.freeze(playerY,camera,view.visible):pan.center(camera,view.visible);
    const wideMenu=mode>=5&&mode<=7;
    if((!crop&&!wideMenu)||mode===2)pan.camera=null;
    const rect=wide&&wideMenu?{x:0,y:0,width:view.width,height:view.height}:imageRect(view,crop,offset);
    const style=[rect.x,rect.y,rect.width,rect.height].map(v=>Math.round(v*100)/100).join(',');
    if(style===previousStyle)return;previousStyle=style;
    canvas.style.left=rect.x+'px';canvas.style.top=rect.y+'px';
    canvas.style.width=rect.width+'px';canvas.style.height=rect.height+'px';
  }
  function resize(){
    view=viewport(innerWidth,innerHeight,wide);
    frame.style.left=view.x+'px';frame.style.top=view.y+'px';
    frame.style.width=view.width+'px';frame.style.height=view.height+'px';draw();
  }
  root.RE1Presentation={
    ...api,
    setAspect(value){wide=value==='16:9';localStorage.setItem('re1.aspect',wide?'16:9':'4:3');pan.camera=null;resize();},
    get aspect(){return wide?'16:9':'4:3';},
    get croppedRows(){return wide?240-view.visible:0;},
    get inventoryExtra(){return wide?menuExtra(view):0;},
    promptOffset(y,key){return wide?Math.max(0,240-view.visible-pan.freeze(y,key,view.visible)):0;},
    get mode(){return mode;},
    frame(nextMode,nextY,nextCamera){
      if(firstFrame){previousStyle='';firstFrame=false;}
      const now=performance.now(),seconds=(now-previousTime)/1000;previousTime=now;
      if(nextMode!==4){mode=nextMode;playerY=nextY;camera=nextCamera;}
      draw(nextMode===4?0:seconds);
    }
  };
  addEventListener('resize',resize);resize();
})(globalThis);
