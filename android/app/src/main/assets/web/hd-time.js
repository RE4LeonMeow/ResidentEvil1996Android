// Measured, workload-weighted HD build estimates. No fixed device-speed claim.
(function(root){
  'use strict';
  function duration(ms){
    if(!Number.isFinite(ms))return '—';
    const seconds=Math.max(0,Math.ceil(ms/1000)),h=Math.floor(seconds/3600),m=Math.floor(seconds/60)%60,s=seconds%60;
    return (h?h+':':'')+String(m).padStart(2,'0')+':'+String(s).padStart(2,'0');
  }
  class BuildClock{
    constructor(now){this.started=now;this.phase='scan';this.phaseStarted=now;this.updated=now;this.work=0;this.total=0;}
    update(message,now){
      if(message.phase!==this.phase){this.phase=message.phase;this.phaseStarted=now;}
      this.updated=now;this.work=Number(message.doneWork)||0;this.total=Number(message.totalWork)||0;
    }
    text(now){
      const elapsed='Elapsed '+duration(now-this.started);
      if(this.phase==='finish')return elapsed+' · Saving pack…';
      if(this.phase!=='build'||this.work<=0||this.total<=this.work||this.updated-this.phaseStarted<3000)return elapsed+' · Estimating time remaining…';
      const rate=this.work/(this.updated-this.phaseStarted),remaining=(this.total-this.work)/rate-(now-this.updated);
      return elapsed+(remaining>0?' · About '+duration(remaining)+' remaining':' · Updating estimate…');
    }
  }
  const api={BuildClock,duration};root.RE1HDTime=api;
  if(typeof module!=='undefined')module.exports=api;
})(typeof self==='object'?self:globalThis);
