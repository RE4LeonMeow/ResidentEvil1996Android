'use strict';
// Visible diagnostics for both recoverable startup failures and renderer crashes.
(function () {
  // Keep the slow aiming interval after input release or opening Settings.
  // Eight real frames reject isolated timer noise without diluting a short dip.
  class AimTiming {
    constructor(){this.gaps=new Float64Array(8);this.ticks=new Float64Array(8);this.reset();}
    reset(){this.best=[null,null];this.breakWindow();}
    breakWindow(){this.previous=0;this.key=null;this.samples=0;this.sumGap=0;this.sumTick=0;}
    frame(now,tick,aiming,camera,enemies,generated=false){
      if(generated)return;
      if(!aiming||!Number.isFinite(now)||!Number.isFinite(tick)){this.breakWindow();return;}
      const category=enemies>0?1:0,key=camera+':'+category;
      if(this.key!==key){this.breakWindow();this.key=key;}
      if(this.previous){
        const gap=now-this.previous;
        if(gap>0){
          const at=this.samples%8;
          if(this.samples>=8){this.sumGap-=this.gaps[at];this.sumTick-=this.ticks[at];}
          this.gaps[at]=gap;this.ticks[at]=tick;this.sumGap+=gap;this.sumTick+=tick;this.samples++;
          if(this.samples>=8&&(!this.best[category]||this.sumGap/8>this.best[category].gap)){
            this.best[category]={gap:this.sumGap/8,tick:this.sumTick/8,peak:Math.max(...this.gaps),camera,enemies,time:now};
          }
        }
      }
      this.previous=now;
    }
  }
  if(typeof module!=='undefined')module.exports={AimTiming};
  if(typeof document==='undefined')return;
  const aimTiming=new AimTiming();
  const lines=[];
  const start=performance.now();
  let frames=0, generatedFrames=0, lastProgress=performance.now(), active=false, warned=false, stage='Launcher ready', file='';
  let fpsEnabled=false,fpsPaused=false,sampleTime=0,sampleFrames=0;
  const gaps=new Float64Array(64),ticks=new Float64Array(64);let samples=0,previousFrame=0;
  const fps=document.getElementById('fps-counter');
  function resetFPS(){sampleTime=performance.now();sampleFrames=frames;fps.hidden=!fpsEnabled||!active;fps.textContent=fpsPaused?'FPS: paused':'FPS: —';}
  const native=()=>globalThis.AndroidLauncher;
  const text=()=>document.getElementById('diagnostic-report');
  function log(value) {
    const line=((performance.now()-start)/1000).toFixed(1)+'s '+String(value).slice(0,1200);
    lines.push(line);if(lines.length>160)lines.shift();
    native()?.log?.(line);
    if(text()&&document.getElementById('diagnostics').open)text().textContent=report();
  }
  function report() {
    const n=Math.min(samples,64),recent=Array.from(gaps.slice(0,n)).sort((a,b)=>a-b);
    const mean=values=>n?(values.slice(0,n).reduce((a,b)=>a+b,0)/n).toFixed(1):'—';
    let gpu='Unavailable';
    try{const gl=globalThis.GL?.currentContext?.GLctx;if(gl){const ext=gl.getExtension('WEBGL_debug_renderer_info');gpu=gl.getParameter(ext?ext.UNMASKED_RENDERER_WEBGL:gl.RENDERER);}}catch(_){}
    const canvas=document.getElementById('canvas');
    let gameBindings='';
    const binding=globalThis.Module?._re1_web_game_action_binding;
    if(typeof binding==='function')try{
      gameBindings='Game action bindings: '+['ACTION','RUN','AIM','ITEMS','PAUSE'].map((name,index)=>name+'=0x'+binding(index).toString(16)).join(' ')+
        ' | Preset: '+binding(5)+'\n';
    }catch(_){}
    const aimReport=(label,value)=>label+': '+(!value?'not captured':
      (1000/value.gap).toFixed(1)+' FPS | Frame gap: '+value.gap.toFixed(1)+' ms mean / '+value.peak.toFixed(1)+' ms peak | Tick elapsed: '+value.tick.toFixed(1)+' ms mean | Room: '+(1+(value.camera>>>16))+'/'+((value.camera>>>8)&255).toString(16).padStart(2,'0')+'/'+(value.camera&255)+' | Living enemies: '+value.enemies+' | At: '+((value.time-start)/1000).toFixed(1)+'s')+'\n';
    return 'Resident Evil '+(globalThis.RE1BrowserStorage?'Browser':'Android')+' 1.1 (build 42)\n'+navigator.userAgent+'\n'+
      'GPU: '+gpu+'\nOutput: '+canvas.width+'×'+canvas.height+' | HD: '+!!globalThis.RE1HDPack?.active+' | More FPS: '+!!document.getElementById('smooth-frames')?.checked+'\n'+
      'Recent frame gap: '+mean(gaps)+' ms mean / '+(n?recent[Math.floor((n-1)*.95)].toFixed(1):'—')+' ms p95 | Tick elapsed: '+mean(ticks)+' ms mean\n'+
      aimReport('Worst aiming with enemies',aimTiming.best[1])+aimReport('Worst aiming without enemies',aimTiming.best[0])+
      (globalThis.RE1Mods?.report?.()||'Mods: unavailable')+'\n'+
      gameBindings+
      'Phase: '+stage+'\nPresented frames: '+frames+'\nInterpolated frames: '+generatedFrames+'\nLast file: '+file+'\n'+lines.join('\n');
  }
  function notify() {
    const label=document.getElementById('boot-progress');
    if(label){label.textContent=stage+(file?' · '+file:'');label.hidden=!active||frames>30||!!globalThis.RE1ModVideo?.isActive();}
    native()?.progress?.(stage+' | frames='+frames+(file?' | '+file:''));
  }
  const diag=globalThis.RE1Diag={
    log,report,
    get frames() {return frames;},
    get generatedFrames() {return generatedFrames;},
    showFPS(value){fpsEnabled=!!value;resetFPS();},
    paused(value){fpsPaused=!!value;previousFrame=0;aimTiming.breakWindow();resetFPS();},
    begin() {native()?.begin?.();aimTiming.reset();active=true;frames=0;generatedFrames=0;samples=0;previousFrame=0;warned=false;lastProgress=performance.now();resetFPS();this.stage('Loading engine');},
    stage(value) {stage=String(value);lastProgress=performance.now();log(stage);notify();},
    io(path) {file=path;lastProgress=performance.now();globalThis.RE1BrowserStorage?.io(path);notify();},
    frame(generated=false,tickMs=0,aiming=false,camera=0,enemies=0,mode=0) {
      if(generated)generatedFrames++;
      frames++;lastProgress=performance.now();
      if(mode>0&&stage.startsWith('/game/'))stage='Game loop running';
      if(!fpsPaused)aimTiming.frame(lastProgress,tickMs,aiming,camera,enemies,generated);
      if(!fpsPaused){if(previousFrame){const at=samples++%64;gaps[at]=lastProgress-previousFrame;ticks[at]=Number.isFinite(tickMs)?tickMs:0;}previousFrame=lastProgress;}
      if(fpsEnabled&&!fpsPaused&&active&&lastProgress-sampleTime>=500){
        fps.textContent='FPS: '+Math.round((frames-sampleFrames)*1000/(lastProgress-sampleTime));sampleTime=lastProgress;sampleFrames=frames;
      }
      if(frames===1)log('First frame presented');if(frames===30||frames%60===0)notify();
    },
    graphicsPassed:false,
    graphicsResult(ok) {this.graphicsPassed=!!ok;log(ok?'Textured graphics check passed':'Textured graphics check failed');},
    stop() {active=false;resetFPS();native()?.finished?.();notify();},
    failure(message) {log('ERROR: '+message);active=false;notify();globalThis.re1Failure?.(String(message));}
  };
  document.getElementById('diagnostics').addEventListener('toggle',()=>{if(text())text().textContent=report();});
  window.addEventListener('error',event=>{
    const detail=event.error?.stack||event.message||('Could not load '+event.target?.src);
    diag.failure(detail);
  });
  window.addEventListener('unhandledrejection',event=>diag.failure(event.reason?.stack||event.reason));
  document.getElementById('canvas').addEventListener('webglcontextlost',event=>{
    event.preventDefault();diag.failure('The graphics context was lost. Return to the launcher and try OpenGL ES 2 compatibility mode.');
  });
  setInterval(()=>{
    if(active&&!fpsPaused&&!document.hidden&&performance.now()-lastProgress>30000&&!warned){
      warned=true;log('No startup progress for 30 seconds');
      const label=document.getElementById('boot-progress');label.hidden=false;
      label.textContent='No progress: '+stage+'. Open Logs and copy the report.';
      native()?.progress?.('No progress: '+stage+'. Open Logs.');
    }
  },1000);
  document.getElementById('copy-report').onclick=()=>{
    if(native()?.copyReport)native().copyReport(report());
    else navigator.clipboard?.writeText(report());
  };
  log('Launcher script loaded');
})();
