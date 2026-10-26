// Imported MP4 movies use WebView's media decoder and one A/V clock.
// The original AVI/FFmpeg path remains available for unmodified game files.
(function(){
  'use strict';
  const layer=document.createElement('div');layer.id='mod-video-layer';layer.hidden=true;
  const video=document.createElement('video');video.id='mod-video';video.hidden=true;
  const loading=document.createElement('span');loading.id='mod-video-loading';loading.setAttribute('role','status');
  video.playsInline=true;video.preload='auto';video.disableRemotePlayback=true;video.controls=false;
  layer.append(video,loading);document.getElementById('game-frame').append(layer);
  let generation=0,active=false,ended=false,suspended=false,from=0,until=0,openedAt=0,frameTicket=null,frameShown=false,blobURL=null;
  const log=message=>globalThis.RE1Diag?.log('Mod movie: '+message);
  function cover(){frameShown=false;video.removeAttribute('data-ready');loading.hidden=false;loading.textContent='Loading HD video…';}
  function reveal(token){
    if(token!==generation||!active||video.seeking||video.readyState<2)return;
    frameShown=true;video.setAttribute('data-ready','');loading.hidden=true;
  }
  function finish(){if(!active)return;active=false;ended=true;globalThis.RE1Mods?.cancelMovie?.();video.pause();}
  function play(token){
    if(!active||suspended)return;
    video.play().catch(error=>{if(token===generation&&active&&error.name!=='AbortError'){log(error.message);finish();}});
  }
  function seek(token){
    if(token!==generation||!active)return;
    cover();
    try{video.currentTime=from/1000;}catch(error){log(error.message);finish();return;}
    play(token);
  }
  globalThis.RE1ModVideo={
    open(path,to){
      const url=globalThis.RE1Mods?.movie(path);if(!url)return false;
      this.close();const token=++generation;active=true;ended=false;from=0;until=Math.max(0,to);openedAt=performance.now();
      video.onloadedmetadata=()=>seek(token);
      // Older WebViews lack video-frame callbacks. loadeddata/seeked still
      // guarantee that a decoded frame exists; metadata alone does not.
      if(!video.requestVideoFrameCallback)video.onloadeddata=video.onseeked=video.onplaying=()=>reveal(token);
      video.onended=()=>{if(token===generation)finish();};
      video.onerror=()=>{if(token===generation){log('Could not play '+path+' (media error '+(video.error?.code||'unknown')+').');finish();}};
      globalThis.re1TouchContext?.(true);globalThis.RE1Presentation?.frame(0,120,0);
      if(video.requestVideoFrameCallback){
        const frame=()=>{if(token!==generation)return;if(active&&!suspended){reveal(token);globalThis.RE1Diag?.frame();}frameTicket=video.requestVideoFrameCallback(frame);};
        frameTicket=video.requestVideoFrameCallback(frame);
      }
      cover();video.hidden=false;layer.hidden=false;document.getElementById('boot-progress').hidden=true;
      const local=globalThis.RE1Mods?.movieBlob?.(path);
      if(local){
        log('Preparing local MP4: '+path);
        Promise.resolve(local).then(file=>{
          if(token!==generation)return;
          blobURL=URL.createObjectURL(file);video.src=blobURL;openedAt=performance.now();video.load();log('Local file MP4 playback: '+path);
        }).catch(error=>{if(token===generation&&active){log('Could not prepare '+path+': '+error.message);finish();}});
      }else{video.src=url;video.load();log('WebView MP4 playback: '+path);}
      return true;
    },
    playFrom(ms){from=Math.max(0,ms);until=0;active=true;ended=false;openedAt=performance.now();if(video.readyState>=1)seek(generation);},
    stop(){active=false;video.pause();},
    progress(done,total){if(active&&!frameShown){openedAt=performance.now();loading.textContent='Loading HD video… '+Math.floor(done*100/Math.max(1,total))+'%';}},
    close(){++generation;active=false;ended=false;globalThis.RE1Mods?.cancelMovie?.();if(frameTicket!==null)video.cancelVideoFrameCallback?.(frameTicket);frameTicket=null;video.onloadedmetadata=video.onloadeddata=video.onseeked=video.onplaying=video.onended=video.onerror=null;video.pause();video.removeAttribute('src');video.load();if(blobURL)URL.revokeObjectURL(blobURL);blobURL=null;video.hidden=true;layer.hidden=true;cover();},
    isActive(){return active;},
    takeEnd(){const value=ended;ended=false;return value;},
    tick(){
      if(!active)return;
      if(until>0&&video.currentTime*1000>=until)finish();
      if(!suspended&&!frameShown&&performance.now()-openedAt>30000){log('Timed out waiting for the installed MP4 to start.');finish();}
    },
    suspend(value){suspended=!!value;if(suspended)video.pause();else{openedAt=performance.now();if(video.readyState>=1)play(generation);}}
  };
})();
