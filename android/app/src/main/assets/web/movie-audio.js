// Port-owned FMV output. A source node plays the already decoded PCM on the
// audio timeline; delayed SDL mixer callbacks must not consume movie samples.
(function(root){
  'use strict';
  class MovieAudio {
    constructor(context=()=>root.Module?.SDL2?.audioContext){this.getContext=context;this.stop();}
    stopSource(){
      if(!this.source)return;
      this.source.onended=null;
      try{this.source.stop();}catch(_){}
      this.source.disconnect();this.source=null;
    }
    stop(){this.stopSource();this.buffer=null;this.context=null;this.key=null;this.rate=0;this.start=0;this.startTime=0;this.clock=false;}
    play(pcm,rate,start,end,key=pcm){
      const context=this.getContext(),frames=pcm.length/2;
      if(!context||context.state==='closed'||!Number.isInteger(frames)||frames<=0||!Number.isFinite(rate)||rate<=0){this.stop();return false;}
      start=Math.max(0,Math.min(frames,Math.floor(start)));
      end=end<=0?frames:Math.max(0,Math.min(frames,Math.floor(end)));
      if(!Number.isFinite(start)||!Number.isFinite(end)||start>=end){this.stop();return false;}
      this.stopSource();this.clock=false;
      try{
        if(!this.buffer||this.key!==key||this.context!==context||this.rate!==rate||this.buffer.length!==frames){
          const buffer=context.createBuffer(2,frames,rate);
          const left=buffer.getChannelData(0),right=buffer.getChannelData(1);
          for(let i=0;i<frames;i++){left[i]=pcm[i*2]/32768;right[i]=pcm[i*2+1]/32768;}
          this.buffer=buffer;this.key=key;
        }
        this.context=context;this.rate=rate;this.start=start;
        const source=context.createBufferSource();source.buffer=this.buffer;source.connect(context.destination);
        this.source=source;
        source.onended=()=>{source.disconnect();if(this.source===source)this.source=null;};
        // Compute the onset after copying PCM; a long allocation/copy must
        // not make the scheduled start precede the first presented picture.
        this.startTime=context.currentTime+0.05;
        source.start(this.startTime,start/rate,(end-start)/rate);
        this.clock=true;return true;
      }catch(error){this.stop();root.RE1Diag?.log('Movie audio: '+error.message);return false;}
    }
    position(){
      if(!this.clock)return -1;
      // Continue the timeline through a short silent tail until video EOF.
      // AudioContext time freezes during suspension, including Settings.
      return this.start+Math.floor(Math.max(0,this.context.currentTime-this.startTime)*this.rate);
    }
  }
  root.RE1MovieAudio=new MovieAudio();
  if(typeof module!=='undefined')module.exports={MovieAudio};
})(globalThis);
