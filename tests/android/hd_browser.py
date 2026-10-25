"""Asset-free HD installer checks in Chromium: OPFS, cancellation, rollback,
source binding, local reads, reloads, and actual engine uploads.
This hosts the Android installer in test-only file/bridge adapters; the
production browser package intentionally excludes the installer."""
import base64,http.server,json,pathlib,struct,threading,zlib
from playwright.sync_api import sync_playwright
ROOT=pathlib.Path(__file__).resolve().parents[2]
WEB=ROOT/'android/app/src/main/assets/web'
BOOT=['fontus.tim','font03t.tim','optkey03.tim','status.tim','statface.tim','blue.tim','staitem.tim','kage.tim']
palette=struct.pack('<256H',*([0,31,992,31744]+[0]*252))
pixels=bytes(1+(x//128+y//128)%3 for y in range(256) for x in range(256))
TIM=struct.pack('<II',16,9)+struct.pack('<I4H',524,0,0,256,1)+palette+struct.pack('<I4H',65548,0,0,128,256)+pixels
FILES={'Data/'+name:TIM for name in BOOT}|{'Data/item_all.pix':bytes(91200),'Stage1/ROOM1000.rdt':bytes(128)}
class Server(http.server.SimpleHTTPRequestHandler):
 def __init__(self,*a,**kw):super().__init__(*a,directory=str(WEB),**kw)
 def log_message(self,*a):pass
 def do_GET(self):
  if self.path=='/manifest':
   data=json.dumps([{'path':k,'size':len(v)} for k,v in FILES.items()]).encode();kind='application/json'
  elif self.path=='/native.html':
   data=(WEB/'index.html').read_bytes();kind='text/html'
  elif self.path in {'/','/index.html'}:
   # Test-only local-file adapter for exercising the unchanged Android builder.
   html=(WEB/'index.html').read_text()
   controls='<div hidden><input id="browser-remember" type="checkbox"><input id="browser-folder-input" type="file" webkitdirectory multiple><button id="browser-forget"></button><input id="browser-auto-fullscreen" type="checkbox"><button id="browser-fullscreen"></button><output id="browser-loading-file"></output></div>'
   html=html.replace('<body>','<body>'+controls).replace('<script src="presentation.js">','<script src="browser-storage.js"></script><script src="browser.js"></script><script src="presentation.js">')
   data=html.encode();kind='text/html'
  else:return super().do_GET()
  self.send_response(200);self.send_header('Content-Type',kind)
  if self.path=='/native.html':self.send_header('Content-Security-Policy',"default-src 'self'; script-src 'self' 'wasm-unsafe-eval'; worker-src 'self' blob:; style-src 'self'; connect-src 'self' blob:; img-src 'self' data:; object-src 'none'; base-uri 'none'; frame-src 'none'")
  self.end_headers();self.wfile.write(data)
server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Server)
threading.Thread(target=server.serve_forever,daemon=True).start()
try:
 with sync_playwright() as p:
  browser=p.chromium.launch(executable_path='/usr/bin/chromium',headless=True,args=['--no-sandbox','--enable-unsafe-swiftshader'])
  page=browser.new_page(viewport={'width':1000,'height':650});errors=[]
  page.add_init_script('''window.hdProgress=[];const NativeHDWorker=Worker;
   window.Worker=class extends NativeHDWorker{constructor(...args){super(...args);this.addEventListener('message',e=>{if(e.data.type==='progress')hdProgress.push(e.data);});}};''')
  page.on('pageerror',lambda e:errors.append(str(e)))
  page.goto(f'http://127.0.0.1:{server.server_port}/')
  page.wait_for_function('()=>!!RE1BrowserStorage&&!document.querySelector("#folder").disabled')
  page.evaluate('''entries=>{
   window.hdFiles=entries.map(([path,b64])=>{const s=atob(b64),a=Uint8Array.from(s,c=>c.charCodeAt(0)),file=new File([a],path.split('/').pop());return {path,size:file.size,file};});
   RE1BrowserStorage.ready=()=>true;RE1BrowserStorage.status=()=>"Synthetic HD fixture";
   RE1BrowserStorage.hdFiles=()=>window.hdFiles;
   RE1BrowserStorage.manifest=()=>window.hdFiles.map(({path,size})=>({path,size}));
   RE1BrowserStorage.read=(path,offset,length)=>RE1BrowserFiles.readFile(window.hdFiles.find(f=>f.path===path).file,offset,length);
   window.dispatchEvent(new Event('re1-storage-changed'));
  }''',[[k,base64.b64encode(v).decode()] for k,v in FILES.items()])
  page.wait_for_function('()=>!document.querySelector("#hd-install").disabled')
  page.locator('#hd-pack summary').click()
  page.locator('#hd-sharpness').select_option('1.1')
  page.locator('#hd-install').click()
  assert page.locator('#hd-time').is_visible()
  assert 'Elapsed' in page.locator('#hd-time').inner_text()
  page.wait_for_function('()=>!RE1HDPack.busy()&&document.querySelector("#hd-enabled").disabled===false',timeout=60000)
  assert 'prepared images' in page.locator('#hd-status').inner_text(),page.locator('#hd-status').inner_text()
  # Read the installed compressed image from its immutable archive; validate actual pixels and dimensions
  # independently, before testing the asynchronous renderer path.
  record=page.evaluate('''async()=>{
   const dir=await (await navigator.storage.getDirectory()).getDirectoryHandle('re1-hd-v1');
   for await(const [,child] of dir.entries())if(child.kind==='directory'){
    const manifest=JSON.parse(await (await (await child.getFileHandle('manifest.json')).getFile()).text());
    const key=manifest.records[0].key;return {key,sharpness:manifest.sharpness,buildMs:manifest.buildMs,bytes:Array.from(await RE1HDPack.read(key,0,Math.min(262144,RE1HDPack.size(key))))};
   }
  }''')
  data=bytes(record['bytes']);header=struct.unpack('<4s7I',data[:32]);assert header[:2]==(b'R1HD',2)
  assert record['sharpness']==1.1 and record['buildMs']>0,record
  assert 'Built in' in page.locator('#hd-time').inner_text()
  assert page.evaluate('''()=>hdProgress.some(p=>p.phase==='scan')&&hdProgress.some(p=>p.phase==='build'&&p.doneWork>0&&p.doneWork===p.totalWork&&p.count===p.totalImages)''')
  assert header[2:6]==(256,256,1152,1152),header
  pixels=zlib.decompress(data[32:]);assert len(pixels)==1152*1152*4
  assert pixels[3]==255 and pixels[0]>240 and pixels[1]<15 and pixels[2]<15
  assert page.locator('#hd-install').inner_text()=='Reinstall HD pack'
  page.locator('#hd-pack summary').click()
  assert page.locator('#hd-remove').is_visible()
  page.locator('#hd-pack summary').click()
  page.locator('#hd-sharpness').select_option('0')
  assert 'Reinstall to apply Natural' in page.locator('#hd-status').inner_text()
  print('PASS: one-click worker builds a compressed AI 4.5x image, original proportions and RGBA preserved',flush=True)
  print('PASS: measured workload scan, visible elapsed timer, completed duration and persisted install-only sharpness',flush=True)
  # A corrupt replacement source fails atomically and preserves installed data.
  old=page.locator('#hd-status').inner_text()
  page.evaluate('''()=>{const file=new File([new Uint8Array([1,2,3])],'bad.pak');hdFiles.push({path:'Stage1/bad.pak',size:3,file});window.dispatchEvent(new Event('re1-storage-changed'));}''')
  page.wait_for_timeout(100);page.locator('#hd-install').click()
  page.wait_for_function('()=>!RE1HDPack.busy()&&document.querySelector("#hd-status").textContent.includes("unchanged")',timeout=60000)
  assert page.evaluate('(key)=>RE1HDPack.size(key)',record['key'])==len(data)
  page.evaluate('''()=>{hdFiles.pop();window.dispatchEvent(new Event('re1-storage-changed'));}''')
  page.wait_for_timeout(100)
  page.locator('#hd-install').click();page.locator('#hd-cancel').click()
  page.wait_for_function('()=>!document.querySelector("#hd-enabled").disabled')
  assert page.evaluate('(key)=>RE1HDPack.size(key)',record['key'])==len(data)
  print('PASS: failed/cancelled rebuild preserves the existing pack; folder changes disable mismatched packs',flush=True)
  # A concurrent game holds a shared storage lock; rebuilding must leave its
  # existing image snapshots intact and report why the build cannot start.
  page.evaluate('''()=>{navigator.locks.request('re1-hd-library',{mode:'shared'},()=>new Promise(resolve=>{window.releaseHDTestLock=resolve;}));}''')
  page.wait_for_function('()=>!!window.releaseHDTestLock')
  page.locator('#hd-install').click()
  page.wait_for_function('()=>!RE1HDPack.busy()&&document.querySelector("#hd-status").textContent.includes("other")')
  page.evaluate('()=>window.releaseHDTestLock()')
  assert page.evaluate('(key)=>RE1HDPack.size(key)',record['key'])==len(data)
  page.locator('#hd-enabled').check()
  # Enable and boot the real runtime with synthetic textures, then read back
  # upload diagnostics. Streaming movies are covered separately by the engine.
  page.evaluate('''()=>{window.re1Ready=()=>{window.hdRuntimeReady=true;};}''')
  page.locator('#launch').click();page.wait_for_function('()=>window.hdRuntimeReady',timeout=90000)
  page.evaluate("()=>Module.callMain(['--capture','/persist/hd-test.cap','4'])")
  page.wait_for_function("()=>Module.FS.analyzePath('/persist/hd-test.cap').exists",timeout=60000)
  result=page.evaluate('()=>({active:RE1HDPack.active,hits:RE1HDPack.hits,misses:RE1HDPack.misses,w:document.querySelector("#canvas").width,h:document.querySelector("#canvas").height})')
  assert result['active'],result
  page.wait_for_function('()=>RE1HDPack.hits>0',timeout=30000)
  assert (result['w'],result['h'])==(1440,1080),result
  assert not errors,errors
  print('PASS: actual wasm renderer loads cached replacements at 1440x1080:',result,flush=True)
  # Reload restores the immutable pack plus composed-image cache. Removing it
  # reclaims HD storage without touching the separate save database.
  page.reload()
  assert page.locator('#hd-sharpness').input_value()=='0'
  page.wait_for_function('()=>!document.querySelector("#hd-remove").hidden&&!document.querySelector("#hd-remove").disabled')
  assert page.evaluate('(key)=>RE1HDPack.size(key)',record['key'])==len(data)
  # An app upgrade must retain a removable old pack and clearly request a
  # rebuild, rather than silently applying the incompatible 720p dimensions.
  page.evaluate('''async()=>{
   const root=await (await navigator.storage.getDirectory()).getDirectoryHandle('re1-hd-v1');
   for await(const [,dir] of root.entries())if(dir.kind==='directory'){
    const file=await dir.getFileHandle('manifest.json'),meta=JSON.parse(await (await file.getFile()).text());
    meta.version=1;const writer=await file.createWritable();await writer.write(JSON.stringify(meta));await writer.close();
   }
  }''')
  page.reload();page.wait_for_function('()=>document.querySelector("#hd-status").textContent.includes("previous 720p")')
  assert page.locator('#hd-install').inner_text()=='Reinstall HD pack'
  assert page.locator('#hd-enabled').is_disabled()
  assert page.locator('#hd-remove').is_visible()
  page.locator('#hd-remove').click()
  page.wait_for_function('()=>document.querySelector("#hd-remove").hidden')
  assert page.evaluate('(key)=>RE1HDPack.size(key)',record['key'])==0
  page.reload();page.wait_for_function('()=>document.querySelector("#hd-status").textContent.includes("Build a local")')
  print('PASS: concurrent-game lock, reload persistence, old-pack upgrade state and visible removal',flush=True)
  native=browser.new_page(viewport={'width':1000,'height':650})
  native.add_init_script('''window.hdEncoded='''+json.dumps({k:base64.b64encode(v).decode() for k,v in FILES.items()})+''';
   window.hdNativeReads=[];
   window.AndroidLauncher={ready:()=>true,status:()=>"Synthetic Android game files",
    readFileChunk:(path,offset,length)=>{hdNativeReads.push({path,offset,length});return 'ok:'+btoa(atob(hdEncoded[path]).slice(offset,offset+length));}};''')
  native.goto(f'http://127.0.0.1:{server.server_port}/native.html')
  native.wait_for_function('()=>!document.querySelector("#hd-install").disabled')
  native.locator('#hd-install').click()
  native.wait_for_function('()=>!RE1HDPack.busy()&&!document.querySelector("#hd-enabled").disabled',timeout=60000)
  assert 'prepared images' in native.locator('#hd-status').inner_text()
  assert native.evaluate('()=>hdNativeReads.length>=8&&hdNativeReads.every(r=>r.length>0&&r.length<=262144)')
  print('PASS: Android-style synchronous file bridge builds a local pack through bounded worker requests',flush=True)
  native.close()
  browser.close()
finally:server.shutdown()
