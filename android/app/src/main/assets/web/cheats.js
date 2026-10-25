'use strict';
(() => {
  const el=id=>document.getElementById(id);
  let agreed=false, timer=null;
  const call=(action,value=0)=>typeof window.Module?._re1_web_cheat==='function'
    ? Module._re1_web_cheat(action,value) : 0;
  const message=text=>{el('cheats-status').textContent=text;};
  function refresh(){
    const state=call(0), usable=!!(state&256);
    for(const [id,bit] of [['cheat-health',1],['cheat-ammo',2]]){
      el(id).checked=!!(state&bit);el(id).disabled=!usable&&!(state&bit);
    }
    for(const id of ['cheat-heal','cheat-refill','cheat-save','cheat-itembox'])el(id).disabled=!usable;
    el('cheat-disable').disabled=!(state&3);
  }
  function tab(cheats){
    el('controls-panel').hidden=cheats;el('cheats-panel').hidden=!cheats;
    el('controls-tab').setAttribute('aria-selected',String(!cheats));
    el('cheats-tab').setAttribute('aria-selected',String(cheats));
    clearInterval(timer);timer=null;
    if(cheats){
      el('cheats-warning').hidden=agreed;el('cheats-content').hidden=!agreed;
      refresh();
      if(agreed)timer=setInterval(()=>{if(!el('settings').open){clearInterval(timer);timer=null;}else refresh();},500);
    }
  }
  el('controls-tab').onclick=()=>tab(false);
  el('cheats-tab').onclick=()=>tab(true);
  el('cheats-decline').onclick=()=>tab(false);
  el('cheats-agree').onclick=()=>{
    agreed=true;call(1);tab(true);
    message('Start normal gameplay to use cheats. All cheats start off and reset when you return to the launcher.');
  };
  function command(action,value=0){
    // The engine may not have loaded when the warning was first accepted.
    if(!agreed)return false;
    call(1);
    const success=!!call(action,value);
    message(success?'Applied for this session.':'Unavailable here. Resume normal gameplay, then reopen Cheats.');
    refresh();return success;
  }
  el('cheat-health').onchange=()=>command(2,el('cheat-health').checked?1:0);
  el('cheat-ammo').onchange=()=>command(3,el('cheat-ammo').checked?1:0);
  el('cheat-heal').onclick=()=>command(4);
  el('cheat-refill').onclick=()=>command(5);
  for(const [id,action] of [['cheat-save',6],['cheat-itembox',7]])el(id).onclick=()=>{
    if(command(action)){tab(false);el('close-settings').click();}
  };
  el('cheat-disable').onclick=()=>command(8);
  el('settings').addEventListener('close',()=>{clearInterval(timer);timer=null;tab(false);});
})();
