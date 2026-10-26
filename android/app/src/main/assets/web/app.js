'use strict';
const $ = id => document.getElementById(id);
// Native dialog autofocus can scroll long help text straight to its buttons.
// Start at the heading, while retaining an explicit field target for forms.
window.re1ShowDialog=(dialog,field)=>{
  if(!dialog.open)dialog.showModal();
  const target=field||dialog.querySelector('h2,h3')||dialog;
  if(!field)target.tabIndex=-1;
  target.focus({preventScroll:true});
  dialog.scrollTop=0;
};
let saved = {};
try { saved = JSON.parse(localStorage.getItem('re1.controls') || '{}'); } catch (_) {}
const input = globalThis.RE1Input = new RE1InputModel.Input(saved);
let showLoading=localStorage.getItem('re1.showLoadingFiles')!=='false';
function refreshLoadingSetting(){
  const native=window.AndroidLauncher;
  if(typeof native?.loadingFilenamesVisible==='function')showLoading=native.loadingFilenamesVisible();
  $('show-loading-files').checked=showLoading;
}
refreshLoadingSetting();
$('show-loading-files').onchange=()=>{
  showLoading=$('show-loading-files').checked;localStorage.setItem('re1.showLoadingFiles',String(showLoading));
  window.AndroidLauncher?.showLoadingFilenames?.(showLoading);
  window.RE1BrowserStorage?.showLoadingFilenames(showLoading);
};
$('open-logs').onclick=()=>{
  if(typeof window.AndroidLauncher?.openLogs==='function'){
    window.AndroidLauncher.setReport?.(RE1Diag.report());window.AndroidLauncher.openLogs();
  }
  else{$('diagnostics').open=true;$('diagnostics').scrollIntoView({block:'nearest'});}
};
$('aspect').value=RE1Presentation.aspect;
$('aspect').onchange=()=>RE1Presentation.setAspect($('aspect').value);
let launched=false, launching=false, ready=false, editing=false, background=false;
window.re1SaveDataAllowed=()=>!launched&&!launching;
window.re1ModsAllowed=()=>!launched&&!launching;
let menuContext=true, editorMovement=input.settings.movement, selectedControl='action';
let graphicsOnly=false, restarting=false, failed=false;
let renderer=localStorage.getItem('re1.renderer')||'auto';
$('smooth-frames').checked=localStorage.getItem('re1.smoothFrames')==='true';
$('smooth-frames').onchange=()=>{
  const enabled=$('smooth-frames').checked;
  localStorage.setItem('re1.smoothFrames',String(enabled));
  if(ready)Module._re1_web_set_smooth(enabled?1:0);
  RE1Diag.log('Experimental interpolated rendering: '+(enabled?'on':'off'));
};
if(!['auto','2','3'].includes(renderer))renderer='auto';
$('renderer').value=renderer;
$('renderer').onchange=()=>{renderer=$('renderer').value;localStorage.setItem('re1.renderer',renderer);};
$('skip-movies').checked=localStorage.getItem('re1.skipMovies')==='true';
$('skip-movies').onchange=()=>localStorage.setItem('re1.skipMovies',String($('skip-movies').checked));
window.re1Native = (buttons,x,y,connected=true) => {
  input.updateNative(buttons,x,y,connected);
  const status=connected?'Android gamepad connected. Left stick and D-pad: movement.':
    'Connect a Bluetooth or USB controller, then press a controller button.';
  if($('controller-status').textContent!==status)$('controller-status').textContent=status;
};
const layout = {up:[13,49,'▲'],down:[13,79,'▼'],left:[6,64,'◀'],right:[20,64,'▶'],
  action:[91,63,'ACTION'],run:[82,80,'RUN'],aim:[82,46,'AIM'],inventory:[68,81,'ITEMS'],options:[50,15,'PAUSE'],quickturn:[68,63,'TURN']};
function persist() {localStorage.setItem('re1.controls',JSON.stringify(input.settings));}
const directionActions=['up','down','left','right'];
function movementStyle(){return editing?editorMovement:input.movementStyle(menuContext);}
function selectLayoutControl(action){
  selectedControl=action;
  if(directionActions.includes(action))editorMovement='arrows';
  else if(action==='joystick')editorMovement='joystick';
  $('layout-control').value=action;
  const size=input.settings.sizes[action]||1;
  $('layout-size').value=Math.round(size*100);$('layout-size-value').textContent=Math.round(size*100)+'%';
  paint();
}
function paint() {
  $('touch').style.setProperty('--scale',Math.max(.7,Math.min(1.5,Number(input.settings.scale)||1)));
  $('touch').style.setProperty('--opacity',Math.max(.15,Math.min(.95,Number(input.settings.opacity)||.55)));
  $('touch').hidden=(!launched && !editing)||(input.settings.hidden&&!editing);
  for (const [action,def] of Object.entries(layout)) {
    const pos=input.settings.positions?.[action]||def;
    const button=$('touch-'+action);
    button.classList.toggle('latched',input.isTouchLatched(action));
    button.style.left=Math.max(4,Math.min(96,Number(pos[0])||def[0]))+'%';
    button.style.top=Math.max(8,Math.min(92,Number(pos[1])||def[1]))+'%';
    button.style.setProperty('--control-size',input.settings.sizes[action]||1);
    button.classList.toggle('selected',editing&&selectedControl===action);
    if(directionActions.includes(action))button.hidden=movementStyle()==='joystick';
  }
  const stick=$('joystick'),pos=input.settings.positions?.joystick||[13,64];
  stick.hidden=movementStyle()!=='joystick';
  stick.style.setProperty('--control-size',input.settings.sizes.joystick||1);
  stick.classList.toggle('selected',editing&&selectedControl==='joystick');
  stick.style.left=Math.max(10,Math.min(90,Number(pos[0])||13))+'%';
  stick.style.top=Math.max(18,Math.min(82,Number(pos[1])||64))+'%';
}
for (const [action,def] of Object.entries(layout)) {
  const button=document.createElement('button');button.id='touch-'+action;button.textContent=def[2];button.dataset.action=action;
  $('touch').append(button);
  const release=e=>{input.release(e.pointerId);button.classList.remove('held');};
  button.addEventListener('pointerdown',e=>{
    e.preventDefault();button.setPointerCapture(e.pointerId);
    if(editing)selectLayoutControl(action);
    if(!editing&&!input.paused){
      if(action==='quickturn'){
        if(ready&&!Module._re1_web_quick_turn())RE1Diag.log('Quick turn ignored: player movement is currently blocked');
      }
      else input.touchPress(e.pointerId,action,menuContext);
      button.classList.toggle('latched',input.isTouchLatched(action));
      if(input.settings.toggles[action])button.setAttribute('aria-pressed',String(input.isTouchLatched(action)));
      for(const direction of directionActions){
        const other=$('touch-'+direction);other.classList.toggle('latched',input.isTouchLatched(direction));
        if(input.settings.toggles[direction])other.setAttribute('aria-pressed',String(input.isTouchLatched(direction)));
      }
      button.classList.add('held');
    }
  },{passive:false});
  button.addEventListener('pointermove',e=>{
    if(!button.hasPointerCapture(e.pointerId))return;
    if(editing){const r=$('touch').getBoundingClientRect();input.settings.positions={...input.settings.positions,[action]:[Math.max(4,Math.min(96,(e.clientX-r.left)/r.width*100)),Math.max(8,Math.min(92,(e.clientY-r.top)/r.height*100))]};paint();}
  });
  for(const name of ['pointerup','pointercancel','lostpointercapture'])button.addEventListener(name,release);
}
const stick=$('joystick');let stickPointer=null;
function centerStick(){stick.style.setProperty('--stick-x','0px');stick.style.setProperty('--stick-y','0px');}
function moveStick(e){
  if(editing){
    const r=$('touch').getBoundingClientRect();
    input.settings.positions={...input.settings.positions,joystick:[(e.clientX-r.left)/r.width*100,(e.clientY-r.top)/r.height*100]};paint();return;
  }
  const r=stick.getBoundingClientRect(),radius=r.width*.34;
  let x=(e.clientX-r.left-r.width/2)/radius,y=(e.clientY-r.top-r.height/2)/radius;
  const distance=Math.hypot(x,y);if(distance>1){x/=distance;y/=distance;}
  input.stick(e.pointerId,x,y);
  stick.style.setProperty('--stick-x',(x*radius)+'px');stick.style.setProperty('--stick-y',(y*radius)+'px');
}
stick.addEventListener('pointerdown',e=>{
  e.preventDefault();if(stickPointer!==null)return;stickPointer=e.pointerId;stick.setPointerCapture(e.pointerId);
  if(editing)selectLayoutControl('joystick');else moveStick(e);
},{passive:false});
stick.addEventListener('pointermove',e=>{if(e.pointerId===stickPointer){e.preventDefault();moveStick(e);}},{passive:false});
for(const name of ['pointerup','pointercancel','lostpointercapture'])stick.addEventListener(name,e=>{
  if(e.pointerId!==stickPointer)return;input.release(e.pointerId);stickPointer=null;centerStick();
});
$('touch').addEventListener('contextmenu',e=>e.preventDefault());
function clear(){input.clear();stickPointer=null;centerStick();document.querySelectorAll('.held,.latched').forEach(x=>{x.classList.remove('held','latched');if(x.hasAttribute('aria-pressed'))x.setAttribute('aria-pressed','false');});}
window.re1TouchContext=menu=>{
  menu=!!menu;if(menu===menuContext)return;
  menuContext=menu;clear();paint();
};
function pause(){const paused=background||$('settings').open||$('credits').open||$('donation').open||$('saves').open||$('mods')?.open||editing;input.paused=paused;window.RE1Diag?.paused(paused);window.AndroidLauncher?.paused?.(paused);clear();if(ready)Module._re1_web_suspend(paused?1:0);}
window.re1SaveDialogChanged=pause;
window.re1ModsDialogChanged=pause;
window.re1Lifecycle=hidden=>{background=hidden;pause();if(hidden&&ready){Module._re1_web_settings_save();window.re1Sync?.();}};
window.addEventListener('blur',clear);
document.addEventListener('visibilitychange',()=>window.re1Lifecycle(document.hidden));
function finishLayout(){editing=false;$('touch').classList.remove('editing');$('layout-bar').hidden=true;persist();paint();}
window.re1OpenSettings=()=>{if(window.RE1Mods?.close())return;if($('saves').open){if(!window.RE1SaveData?.busy()){$('saves').close();pause();}return;}if($('donation').open){$('donation').close();pause();return;}if($('credits').open){$('credits').close();pause();return;}if(editing)finishLayout();refreshLoadingSetting();$('restore-game-actions').disabled=!ready;if(!$('settings').open)window.re1ShowDialog($('settings'));pause();};
let restoringGameActions=false;
$('restore-game-actions').onclick=()=>{
  if(!ready||restoringGameActions)return;
  const status=$('game-actions-status');
  if(typeof Module._re1_web_restore_game_actions!=='function'){
    status.textContent='This engine does not have game-action recovery. Install the matching APK.';return;
  }
  restoringGameActions=true;
  try{
    clear();
    if(Module._re1_web_restore_game_actions()!==1)throw Error('Open Controls & settings to pause the game first.');
    status.textContent='Game actions restored: AIM readies the weapon, ITEMS opens inventory and PAUSE opens game options. Save again to keep these bindings in that slot.';
    RE1Diag.log('Game action bindings restored. Save again to keep the recovered layout in that slot.');
  }catch(error){status.textContent='Could not restore game actions: '+error.message;}
  finally{restoringGameActions=false;}
};
$('gear').onclick=$('launcher-settings').onclick=window.re1OpenSettings;
$('launcher-credits').onclick=async()=>{
  $('credits-text').textContent='Loading credits…';window.re1ShowDialog($('credits'));pause();
  if(location.protocol==='file:'){
    const source=$('bundled-credits');
    $('credits-text').textContent=source?source.content.textContent:'Credits unavailable in this package.';
    return;
  }
  try{
    const response=await fetch('plain.txt',{cache:'no-store'});
    if(!response.ok)throw Error('Credits file unavailable');
    $('credits-text').textContent=await response.text();
  }catch(error){$('credits-text').textContent='Could not load plain.txt.';RE1Diag.log(error.message);}
};
$('close-credits').onclick=()=>{$('credits').close();pause();};
$('credits').addEventListener('cancel',()=>setTimeout(pause,0));
$('launcher-donation').onclick=()=>{
  $('donation-warning').hidden=false;$('donation-details').hidden=true;
  window.re1ShowDialog($('donation'));pause();
};
$('donation-ok').onclick=()=>{
  $('donation-warning').hidden=true;$('donation-details').hidden=false;window.re1ShowDialog($('donation'));
};
const closeDonation=()=>{$('donation').close();pause();};
$('donation-cancel').onclick=$('donation-close').onclick=closeDonation;
$('donation').addEventListener('cancel',()=>setTimeout(pause,0));
for(const link of document.querySelectorAll('.social-link'))link.addEventListener('click',event=>{
  if(typeof window.AndroidLauncher?.openExternal==='function'){
    event.preventDefault();window.AndroidLauncher.openExternal(link.href);
  }
});
$('close-settings').onclick=()=>{$('settings').close();pause();persist();};
$('settings').addEventListener('cancel',()=>setTimeout(pause,0));
for (const id of ['opacity','scale','deadzone']) {
  $(id).value=input.settings[id]*100;
  $(id).oninput=()=>{input.settings[id]=Number($(id).value)/100;paint();persist();};
}
$('hide-touch').checked=input.settings.hidden;
$('hide-touch').onchange=()=>{clear();input.settings.hidden=$('hide-touch').checked;paint();persist();};
$('movement').value=input.settings.movement;
$('movement').onchange=()=>{clear();input.settings.movement=$('movement').value;paint();persist();};
$('menu-arrows').checked=input.settings.menuArrows;
$('menu-arrows').onchange=()=>{clear();input.settings.menuArrows=$('menu-arrows').checked;paint();persist();};
$('show-fps').checked=!!input.settings.showFPS;
RE1Diag.showFPS(!!input.settings.showFPS);
$('show-fps').onchange=()=>{input.settings.showFPS=$('show-fps').checked;RE1Diag.showFPS(input.settings.showFPS);persist();};
const names=['A / Cross','B / Circle','X / Square','Y / Triangle','L1','R1','L2','R2','Select','Start','L3','R3','D-pad up','D-pad down','D-pad left','D-pad right'];
input.onQuickTurn=()=>{if(ready&&!input.paused)Module._re1_web_quick_turn();};
const quickTurnLabel=document.createElement('label'),quickTurnSelect=document.createElement('select');
quickTurnLabel.textContent='Quick turn shortcut';quickTurnSelect.id='controller-quick-turn';quickTurnSelect.add(new Option('Disabled',-1));
names.forEach((name,index)=>quickTurnSelect.add(new Option(name,index)));quickTurnSelect.value=input.settings.quickTurnButton;
quickTurnSelect.onchange=()=>{clear();input.settings.quickTurnButton=Number(quickTurnSelect.value);persist();};
quickTurnLabel.append(quickTurnSelect);$('bindings').before(quickTurnLabel);
const quickTurnHint=document.createElement('p');quickTurnHint.textContent='Default: R3 (press the right stick). Tap once for a 180° turn during player control. This shortcut replaces the chosen button’s ordinary binding.';quickTurnLabel.after(quickTurnHint);
for(const [action,def] of Object.entries(layout)){
  if(!RE1InputModel.bits[action])continue; // TURN is an immediate one-shot action
  const label=document.createElement('label');label.textContent=def[2];
  const select=document.createElement('select');select.id='touch-mode-'+action;
  select.add(new Option('Hold','hold'));select.add(new Option('Toggle','toggle'));
  select.value=input.settings.toggles[action]?'toggle':'hold';
  select.onchange=()=>{clear();input.settings.toggles[action]=select.value==='toggle';const button=$('touch-'+action);if(input.settings.toggles[action])button.setAttribute('aria-pressed','false');else button.removeAttribute('aria-pressed');persist();};
  label.append(select);$('touch-button-modes').append(label);
}
$('release-touch').onclick=clear;
for(let i=0;i<16;i++){
  const label=document.createElement('label');label.textContent=names[i];const select=document.createElement('select');
  for(const [name,bit] of Object.entries(RE1InputModel.bits)){const option=new Option(name,bit);select.add(option);}
  select.add(new Option('Unused',0));
  // Retain Marni buttons not assigned a touch label.
  if(!Array.from(select.options).some(x=>Number(x.value)===input.settings.mapping[i])) select.add(new Option('Game default',input.settings.mapping[i]));
  select.value=input.settings.mapping[i];select.onchange=()=>{input.settings.mapping[i]=Number(select.value);persist();clear();};label.append(select);$('bindings').append(label);
}
$('reset').onclick=()=>{
  input.settings={...RE1InputModel.defaults,positions:{},sizes:{},toggles:{},mapping:[...RE1InputModel.defaults.mapping]};
  document.querySelectorAll('#touch-button-modes select').forEach(select=>{select.value='hold';});
  for(const id of ['opacity','scale','deadzone'])$(id).value=input.settings[id]*100;
  $('hide-touch').checked=false;
  $('movement').value='arrows';$('show-fps').checked=false;RE1Diag.showFPS(false);
  $('menu-arrows').checked=true;
  quickTurnSelect.value=input.settings.quickTurnButton;
  document.querySelectorAll('#bindings select').forEach((select,i)=>{
    const value=input.settings.mapping[i];
    if(!Array.from(select.options).some(x=>Number(x.value)===value))select.add(new Option('Game default',value));
    select.value=value;
  });
  clear();paint();persist();
};
for(const [action,def] of Object.entries(layout)){
  const name=directionActions.includes(action)?action[0].toUpperCase()+action.slice(1)+' arrow':def[2];
  $('layout-control').add(new Option(name,action));
}
$('layout-control').add(new Option('Joystick','joystick'));
$('layout-control').onchange=()=>selectLayoutControl($('layout-control').value);
$('layout-size').oninput=()=>{
  input.settings.sizes[selectedControl]=Number($('layout-size').value)/100;
  $('layout-size-value').textContent=$('layout-size').value+'%';paint();
};
$('edit-layout').onclick=()=>{
  editing=true;editorMovement=input.settings.movement;$('settings').close();
  $('touch').classList.add('editing');$('layout-bar').hidden=false;pause();
  selectLayoutControl(editorMovement==='joystick'?'joystick':'action');
};
$('save-layout').onclick=()=>window.re1OpenSettings();
$('folder').onclick=()=>{const bridge=window.RE1BrowserStorage||window.AndroidLauncher;bridge?.selectFolder();};
function refreshStorage(){if(launched)return;const bridge=window.RE1BrowserStorage||window.AndroidLauncher;$('status').textContent=bridge?bridge.status():'Use the Android app to select local game files.';const busy=!!window.RE1HDPack?.busy();const saveBusy=!!window.RE1SaveData?.busy()||!!window.RE1Mods?.busy();$('launch').disabled=launching||!bridge?.ready()||busy||saveBusy;$('folder').disabled=busy||saveBusy||!!bridge?.busy?.();$('graphics-test').disabled=launching||busy||saveBusy;$('launcher-saves').disabled=launching;$('launcher-mods').disabled=launching||busy||saveBusy;}
setInterval(refreshStorage,600);window.addEventListener('re1-storage-changed',refreshStorage);window.addEventListener('re1-hd-changed',refreshStorage);window.addEventListener('re1-mods-changed',refreshStorage);refreshStorage();
window.re1Failure=message=>{
  window.RE1ModVideo?.close();
  failed=true;ready=false;window.RE1Diag?.stop();$('error-text').textContent=message;
  if(!$('error').open)window.re1ShowDialog($('error'));input.paused=true;clear();
  Module['SDL2']?.audioContext?.suspend().catch(()=>{});
};
function returnToLauncher() {
  persist();
  // Native recreation also works after an engine exception. A page reload
  // leaves recovery to the failed renderer and was unreliable on the phone.
  if(typeof window.AndroidLauncher?.returnToLauncher==='function')window.AndroidLauncher.returnToLauncher();
  else location.reload();
}
window.re1Exit=code=>{
  if(restarting){returnToLauncher();return;}
  if(failed){RE1Diag.stop();return;}
  ready=false;RE1Diag.stop();
  const success=graphicsOnly&&RE1Diag.graphicsPassed&&code===0;
  $('error').querySelector('h2').textContent=success?'Graphics check passed':'Game closed';
  re1Failure(success?'The actual game renderer drew and read back the test pattern correctly. Did you see four colored squares and hear the test tone? Return to the launcher to load your game.':
    'The game stopped (code '+code+'). Open startup diagnostics in settings for the last file and error.');
};
$('reload').onclick=returnToLauncher;
$('restart-launcher').onclick=()=>{
  persist();
  if(ready){restarting=true;RE1Diag.stage('Saving and returning to launcher');Module._re1_web_request_quit();}
  else returnToLauncher();
};
window.Module={};
window.re1Ready=()=>{
  ready=true;$('launcher').hidden=true;$('gear').hidden=false;paint();
  Module._re1_web_set_smooth($('smooth-frames').checked?1:0);
  $('canvas').focus({preventScroll:true});
  RE1Diag.stage('Starting game engine');
  if(window.RE1Mods?.localization()&&(typeof Module._re1_web_localization_load!=='function'||Module._re1_web_localization_load()!==1)){re1Failure('The installed translation cache could not be loaded. Disable or reinstall that translation in Mods, and use a matching engine/APK build.');return;}
  Module.callMain(graphicsOnly?['--graphics-test']:[]);pause();
};
async function launch(testOnly) {
  if(launched||launching||window.RE1SaveData?.busy()||window.RE1Mods?.busy()||$('saves').open||$('mods')?.open)return;
  if(window.RE1HDPack?.busy())return;
  launching=true;
  $('launch').disabled=true;
  // Capture user activation now, before WebAssembly/file initialization.
  const Audio=window.AudioContext||window.webkitAudioContext;
  if(Audio){
    const audio=new Audio();audio.resume().catch(error=>RE1Diag.log('Audio unlock: '+error));
    Module['SDL2']={audioContext:audio};
    if(testOnly){const tone=audio.createOscillator(),gain=audio.createGain();tone.frequency.value=440;gain.gain.value=.08;tone.connect(gain).connect(audio.destination);tone.start();tone.stop(audio.currentTime+.5);}
  }
  window.RE1BrowserStorage?.launch();
  $('status').textContent='Preparing game and installed mods…';
  try{if(!testOnly)await window.RE1Mods?.prepare();Module['re1HDActive']=!testOnly&&await window.RE1HDPack?.prepare();}
  catch(error){launching=false;re1Failure(String(error.message||error));return;}
  try{await window.RE1SaveData?.startGame();}
  catch(error){launching=false;re1Failure(String(error.message||error));return;}
  graphicsOnly=testOnly;
  let selected=renderer;
  const probeCanvas=document.createElement('canvas');
  let probe=selected==='2'?probeCanvas.getContext('webgl'):probeCanvas.getContext('webgl2');
  if(!probe&&selected==='auto'){probe=probeCanvas.getContext('webgl');selected='2';}
  else if(selected==='auto')selected='3';
  if(!probe){re1Failure('OpenGL ES '+selected+' is unavailable. Try the other renderer in settings and update '+(window.RE1BrowserStorage?'your browser.':'Android System WebView.'));return;}
  try{window.RE1Mods?.graphicsLimit(probe.getParameter(probe.MAX_TEXTURE_SIZE));}
  catch(error){launching=false;re1Failure(error.message);return;}
  RE1Diag.begin();RE1Diag.log('Selected OpenGL ES '+selected);
  Module['re1GLES']=selected;Module['re1DiagnosticsOnly']=testOnly;
  Module['re1SkipMovies']=$('skip-movies').checked;
  launched=true;$('launch').disabled=true;$('graphics-test').disabled=true;$('folder').disabled=true;$('status').textContent='Loading game engine…';
  const script=document.createElement('script');script.src=window.RE1BrowserStorage?.engineScript||'residentevil.js';script.onerror=()=>re1Failure(window.RE1BrowserStorage?'Engine files are missing. Extract the complete package into one folder, then reopen index.html.':'Engine files are missing. Reinstall the complete APK.');document.body.append(script);
}
$('launch').onclick=()=>launch(false);
$('graphics-test').onclick=()=>launch(true);
// A tap after a lifecycle transition must also resume WebAudio.
document.addEventListener('pointerdown',()=>{
  const audio=Module['SDL2']?.audioContext;
  if(!input.paused&&audio?.state==='suspended')audio.resume().catch(error=>RE1Diag.log('Audio: '+error));
},{capture:true});
window.addEventListener('gamepadconnected',()=>{$('controller-status').textContent='Controller connected.';});
window.addEventListener('gamepaddisconnected',clear);
if(window.RE1BrowserStorage){
  // The browser shortcuts use the same buffered input as touch/gamepads.
  // Keep SDL keyboard listeners on the canvas so settings controls keep their
  // normal keyboard behavior without queuing stuck keys in the paused game.
  Module['keyboardListeningElement']=$('canvas');
  const keys={ArrowUp:'up',ArrowDown:'down',ArrowLeft:'left',ArrowRight:'right',KeyC:'action',Enter:'action',KeyV:'run',ControlLeft:'run',ControlRight:'run',Backspace:'run',Escape:'run',KeyX:'aim',KeyZ:'inventory',KeyP:'options'};
  window.addEventListener('keydown',event=>{
    if(!launched||event.metaKey||event.altKey)return;
    if(event.code==='F2'){
      event.preventDefault();event.stopImmediatePropagation();
      if(!event.repeat){if($('settings').open)$('close-settings').click();else window.re1OpenSettings();}return;
    }
    if(!ready||input.paused||document.activeElement!==$('canvas'))return;
    if(event.code==='KeyQ'){
      event.preventDefault();event.stopImmediatePropagation();if(!event.repeat)Module._re1_web_quick_turn();return;
    }
    const action=keys[event.code];if(!action)return;
    event.preventDefault();event.stopImmediatePropagation();if(!event.repeat)input.press('key:'+event.code,action);
  },true);
  window.addEventListener('keyup',event=>{
    if(!keys[event.code])return;input.release('key:'+event.code);
    if(document.activeElement===$('canvas')){event.preventDefault();event.stopImmediatePropagation();}
  },true);
  const close=$('close-settings').onclick;
  $('close-settings').onclick=()=>{close();if(ready)$('canvas').focus({preventScroll:true});};
}
paint();
