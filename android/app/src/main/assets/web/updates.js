// Optional Android launcher check. Game files/saves never enter this request.
(function(){
  'use strict';
  const native=window.AndroidLauncher,core=window.RE1UpdateCore;
  if(!core||!native?.hasUpdateChecker?.())return;
  const launcher=document.querySelector('#launcher'),widget=document.querySelector('#updates');
  const status=document.querySelector('#update-status'),open=document.querySelector('#update-open');
  const option=document.querySelector('#check-updates'),key='re1.updates.enabled';
  let serial=0,request=null,active=false;
  try{option.checked=localStorage.getItem(key)!=='false';}catch(_){option.checked=true;}
  widget.hidden=false;option.closest('label').hidden=false;
  function online(){
    try{return native.updateNetworkAvailable();}catch(_){return navigator.onLine;}
  }
  function paint(state,text){
    widget.dataset.state=state;status.textContent=text;
    status.disabled=state==='checking';
    status.title=state==='checking'?'Checking GitHub releases':'Check for updates';
    open.hidden=state!=='available';open.disabled=state!=='available';
  }
  function stop(){serial++;request?.abort();request=null;active=false;}
  async function check(force=false){
    if(launcher.hidden)return;
    if(!online()){stop();paint('offline','Offline mode');return;}
    if(!force&&!option.checked){stop();paint('disabled','Update checks off');return;}
    stop();const id=serial;
    const controller=new AbortController();request=controller;active=true;paint('checking','Checking for updates…');
    const timeout=setTimeout(()=>controller.abort(),15000);
    try{
      const response=await fetch('/updates'+(force?'?force=1':''),{signal:request.signal,cache:'no-store'});
      if(!response.ok)throw Error('Release check unavailable');
      const reply=await response.json();
      if(id!==serial||launcher.hidden)return;
      const value=online()?core.result(reply):core.result({state:'offline'});
      paint(value.state,value.text);
    }catch(_){
      if(id===serial&&!launcher.hidden)paint(online()?'unavailable':'offline',online()?'Couldn’t check for updates':'Offline mode');
    }finally{clearTimeout(timeout);if(id===serial){request=null;active=false;}}
  }
  status.onclick=()=>check(true);
  open.onclick=()=>native.openExternal('https://github.com/RE4LeonMeow/ResidentEvil1996Android/releases');
  option.onchange=()=>{
    try{localStorage.setItem(key,String(option.checked));}catch(_){}
    check();
  };
  function networkChanged(){
    if(launcher.hidden)return;
    if(!online()){stop();paint('offline','Offline mode');}
    else if(!active)check();
  }
  window.RE1Updates={networkChanged};
  window.addEventListener('online',networkChanged);window.addEventListener('offline',networkChanged);
  document.addEventListener('visibilitychange',()=>{if(!document.hidden)networkChanged();});
  new MutationObserver(()=>{if(launcher.hidden)stop();}).observe(launcher,{attributes:true,attributeFilter:['hidden']});
  check();
})();
