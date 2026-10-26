"""Optional private SHDP/GOG integration test. No game/pack assets are bundled.

Run with --game /path/to/GOG/USA --pack /path/to/RE_SHDP_1.1.zip --profile
/private/test-profile --output /private/captures. Native I/O is simulated over
loopback HTTP; real Android archive transport has its separate JVM tests.
"""
import argparse
import base64
import functools
import hashlib
import io
import http.server
import json
import pathlib
import re
import shutil
import threading
import time
import urllib.parse
import uuid
import zipfile
from PIL import Image

from playwright.sync_api import sync_playwright

ROOT = pathlib.Path(__file__).resolve().parents[2]
WEB = ROOT / 'android/app/src/main/assets/web'
parser = argparse.ArgumentParser()
parser.add_argument('--game', required=True, type=pathlib.Path)
parser.add_argument('--pack', required=True, type=pathlib.Path)
parser.add_argument('--profile', required=True, type=pathlib.Path)
parser.add_argument('--output', required=True, type=pathlib.Path)
parser.add_argument('--port', default=18087, type=int)
parser.add_argument('--reuse', action='store_true')
parser.add_argument('--rebirth', action='store_true', help='Exercise a privately supplied Classic REbirth translation instead of SHDP')
parser.add_argument('--movie-check', action='store_true')
parser.add_argument('--jill', action='store_true')
parser.add_argument('--textures-only', action='store_true', help='Check installed texture payloads and optional movie playback without advancing the story')
parser.add_argument('--inventory-check',action='store_true',help='Verify HD item sprites in the actual game inventory, then stop')
parser.add_argument('--movie-suite',action='store_true',help='With --textures-only, also test imported Dm1/PJ/Ed4 playback and seeks')
args = parser.parse_args()
if args.movie_suite and not args.textures_only:parser.error('--movie-suite requires --textures-only')
if args.rebirth and (args.textures_only or args.inventory_check or args.movie_check or args.movie_suite):parser.error('--rebirth runs its own gameplay/inventory/save checks')
args.movie_check=args.movie_check or args.movie_suite
args.output.mkdir(parents=True, exist_ok=True)
files = {p.relative_to(args.game).as_posix(): p for p in args.game.rglob('*') if p.is_file()}
game_manifest = [{'path': name, 'size': p.stat().st_size} for name, p in files.items()]
pack = zipfile.ZipFile(args.pack)
entries = [item for item in pack.infolist() if not item.is_dir()]
packet = {'id': 'pack-' + uuid.uuid4().hex, 'name': args.pack.name,
          'files': [{'path': entry.filename, 'slot': n, 'size': entry.file_size, 'crc': entry.CRC} for n, entry in enumerate(entries)]}
media_lock=threading.Lock()


def verify_textures(page,pack):
    aliases={record['key']:record for record in json.loads((WEB / 'shdp-profile.json').read_text())['records']}
    probes=[]
    # A rapid startup-input test can skip the main hall. Independently
    # verify its actual camera masks, including both GOG palette variants,
    # resolve to the official HD pixels after an existing-pack migration.
    definitions=[{'source':source,'rule':rule} for source,rule in [('Objspr/OSP00061.pak','page'),('Objspr/OSP00065.pak','page'),('Objspr/OSP00066.pak','page'),('Item_m1/KAI03.DOR','page'),('Data/TYPE00.TIM','opaque')]]
    effects=sorted(name for name in files if name.lower().startswith('effspr/') and name.lower().endswith('.tim'))
    definitions.extend({'source':name,'rule':'page'} for name in effects)
    item_path=next(name for name in files if name.lower()=='data/item_all.pix')
    definitions.extend({'source':item_path,'row':row} for row in range(files[item_path].stat().st_size//1200))
    icon_count=effect_count=0
    for definition in definitions:
        source=definition['source']
        key=page.evaluate('''({source,rule,row})=>{
            const bytes=Module.FS.readFile('/game/USA/'+source);let image;
            if(row!==undefined){
              const status=RE1HDCore.tim(Module.FS.readFile('/game/USA/Data/STATUS.TIM'),0),rgba=new Uint8Array(40*30*4);
              for(let i=0;i<1200;i++){const index=bytes[row*1200+i],v=status.v.getUint16(status.palette.offset+(512+index)*2,true);
                rgba[i*4]=Math.floor((v&31)*255/31);rgba[i*4+1]=Math.floor(((v>>5)&31)*255/31);rgba[i*4+2]=Math.floor(((v>>10)&31)*255/31);rgba[i*4+3]=index?255:0;}
              image={w:40,h:30,rgba};
            }else image=[...RE1HDCore.images(bytes,source)].find(im=>im.rule===rule);
            if(!image)throw Error('Missing probe image');const key=RE1HDCore.key(image.rgba,image.w,image.h);RE1HDPack.request(key);return key;
        }''',definition)
        if key not in aliases:
            assert definition.get('row')==46 or source.lower() in ['effspr/esp221.tim','effspr/esp224.tim'],definition
            continue
        page.wait_for_function('(key)=>RE1HDPack.pixels(key)>0',arg=key)
        actual=page.evaluate('''async key=>{
            const raw=new Uint8Array(RE1HDPack.pixels(key));if(!RE1HDPack.take(key,raw))throw Error('Missing probe pixels');
            const view=new DataView(raw.buffer),hash=await crypto.subtle.digest('SHA-256',raw.subarray(32));
            return {version:view.getUint32(4,true),width:view.getUint32(16,true),height:view.getUint32(20,true),sha:Array.from(new Uint8Array(hash),v=>v.toString(16).padStart(2,'0')).join('')};
        }''',key)
        alias=aliases[key];expected=Image.open(io.BytesIO(pack.read(alias['file']))).convert('RGBA')
        if alias.get('crop'):
            x,y,w,h,rw,rh=alias['crop'];sx,sy=expected.width//rw,expected.height//rh;expected=expected.crop((x*sx,y*sy,(x+w)*sx,(y+h)*sy))
        if alias['alpha']=='opaque':expected.putalpha(255)
        assert (actual['width'],actual['height'])==expected.size,(source,actual)
        assert actual['sha']==hashlib.sha256(expected.tobytes()).hexdigest(),source+' lost official HD pixels'
        assert actual['version']==(4 if alias.get('layout')=='page' else 3)
        probes.append({**definition,'key':key,**actual})
        if 'row' in definition:icon_count+=1
        if source in effects:effect_count+=1
    assert icon_count==71 and effect_count==31,(icon_count,effect_count)
    return probes


def verify_rebirth_documents(page,pack):
    probes=[]
    for entry in entries:
        match=re.search(r'(?:^|/)file/FILEM_([0a-z])(\d{2})\.png$',entry.filename,re.I)
        if not match:continue
        source='Item_m2/filem_'+match[1].lower()+'0.pix'
        original=next((name for name in files if name.lower()==source.lower()),None)
        if not original:continue
        key=page.evaluate('''({source,page})=>{
            const raw=Module.FS.readFile('/game/USA/'+source),view=new DataView(raw.buffer,raw.byteOffset,raw.byteLength);
            const start=view.getUint32(page*4,true);if(!start||start>=raw.length)return null;
            const tim=RE1HDCore.tim(RE1HDCore.unpackPak(raw.subarray(start)),0),rgba=RE1HDCore.decode(tim);
            const key=RE1HDCore.key(rgba,tim.w,tim.h);RE1HDPack.request(key);return key;
        }''',{'source':original,'page':int(match[2])})
        if not key:continue
        page.wait_for_function('(key)=>RE1HDPack.pixels(key)>0',arg=key)
        actual=page.evaluate('''async key=>{
            const raw=new Uint8Array(RE1HDPack.pixels(key));if(!RE1HDPack.take(key,raw))throw Error('Document pixels unavailable');
            const view=new DataView(raw.buffer),hash=await crypto.subtle.digest('SHA-256',raw.subarray(32));
            return {width:view.getUint32(16,true),height:view.getUint32(20,true),sha:Array.from(new Uint8Array(hash),n=>n.toString(16).padStart(2,'0')).join('')};
        }''',key)
        image=Image.open(io.BytesIO(pack.read(entry))).convert('RGBA')
        padded=Image.new('RGBA',(actual['width'],actual['height']))
        padded.paste(image,(0,0))
        assert actual['sha']==hashlib.sha256(padded.tobytes()).hexdigest(),entry.filename+' lost translated pixels'
        probes.append({'source':entry.filename,'key':key,**actual})
    assert probes,'No document images were checked'
    return probes


class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *a, **kw):
        super().__init__(*a, directory=str(WEB), **kw)

    def log_message(self, *_):
        pass

    def do_GET(self):
        uri = urllib.parse.urlsplit(self.path)
        query = urllib.parse.parse_qs(uri.query)
        try:
            if uri.path == '/manifest':
                data, kind = json.dumps(game_manifest).encode(), 'application/json'
            elif uri.path == '/private-pack':
                data, kind = json.dumps(packet).encode(), 'application/json'
            elif uri.path in ['/private-game-read', '/private-mod-read']:
                offset, length = int(query['offset'][0]), int(query['length'][0])
                if offset < 0 or not 0 < length <= 262144:
                    raise ValueError('Invalid range')
                if uri.path == '/private-game-read':
                    with files[query['path'][0]].open('rb') as source:
                        source.seek(offset)
                        raw = source.read(length)
                else:
                    entry=entries[int(query['slot'][0])]
                    # Simulate Android's committed raw file, rather than
                    # decompressing the ZIP from byte zero for every range.
                    if entry.filename.lower().endswith('.mp4'):
                        cached=args.output/'media-fixtures'/str(int(query['slot'][0]))
                        with media_lock:
                            if not cached.exists() or cached.stat().st_size!=entry.file_size:
                                cached.parent.mkdir(exist_ok=True)
                                with pack.open(entry) as source,cached.open('wb') as dest:shutil.copyfileobj(source,dest)
                        source=cached.open('rb')
                    else:source=pack.open(entry)
                    with source:
                        source.seek(offset)
                        raw = source.read(length)
                if len(raw) != length:
                    raise ValueError('Incomplete fixture read')
                data, kind = b'ok:' + base64.b64encode(raw), 'text/plain'
            elif uri.path.startswith('/mod-video/'):
                entry=entries[int(uri.path.rsplit('/',1)[-1].replace('.mp4',''))]
                assert entry.filename.lower().startswith('movie/') and entry.filename.lower().endswith('.mp4')
                start,end=0,entry.file_size-1
                header=self.headers.get('Range')
                if header:
                    left,right=header.removeprefix('bytes=').split('-')
                    start=int(left) if left else max(0,entry.file_size-int(right))
                    if left and right:end=min(end,int(right))
                self.send_response(206 if header else 200)
                self.send_header('Content-Type','video/mp4');self.send_header('Accept-Ranges','bytes')
                self.send_header('Content-Length',str(end-start+1))
                if header:self.send_header('Content-Range',f'bytes {start}-{end}/{entry.file_size}')
                self.end_headers()
                try:
                    with pack.open(entry) as source:
                        source.seek(start);remaining=end-start+1
                        while remaining:
                            part=source.read(min(remaining,262144))
                            if not part:break
                            self.wfile.write(part);remaining-=len(part)
                except (BrokenPipeError,ConnectionResetError):pass
                return
            else:
                return super().do_GET()
            self.send_response(200)
            self.send_header('Content-Type', kind)
            self.send_header('Content-Length', str(len(data)))
            self.end_headers()
            self.wfile.write(data)
        except Exception as error:
            data = ('error:' + str(error)).encode()
            self.send_response(400)
            self.end_headers()
            self.wfile.write(data)

    def end_headers(self):
        self.send_header('Cache-Control', 'no-store')
        self.send_header('Content-Security-Policy', "default-src 'self'; script-src 'self' 'wasm-unsafe-eval'; worker-src 'self' blob:; style-src 'self'; connect-src 'self' blob:; media-src 'self' blob:; img-src 'self' data:; object-src 'none'; base-uri 'none'; frame-src 'none'")
        super().end_headers()


server = http.server.ThreadingHTTPServer(('127.0.0.1', args.port), Handler)
threading.Thread(target=server.serve_forever, daemon=True).start()
try:
    with sync_playwright() as p:
        browser = p.chromium.launch_persistent_context(str(args.profile), executable_path='/usr/bin/chromium',
            headless=True, args=['--no-sandbox', '--enable-unsafe-swiftshader'], viewport={'width': 1280, 'height': 720}, has_touch=True)
        for old in browser.pages:
            old.close()
        page = browser.new_page()
        page.set_default_timeout(90000)
        errors = []
        page.on('pageerror', lambda error: errors.append(str(error)))
        media_requests=[]
        page.on('request',lambda request:media_requests.append(request.url) if '/mod-video/' in request.url else None)
        # Only the transport is mocked. The actual app, workers, WASM, codec,
        # OPFS storage and GPU renderer run unchanged.
        page.add_init_script('window.testSHDPMovies='+str(args.movie_check).lower()+';')
        page.add_init_script('''(() => {
          const read=(route,query)=>{
            const xhr=new XMLHttpRequest();xhr.open('GET',route+'?'+new URLSearchParams(query),false);xhr.send();return xhr.responseText;
          };
          const reply=(request,result)=>queueMicrotask(()=>RE1Mods.nativeResult(request,result));
          window.AndroidLauncher={ready:()=>true,status:()=>"Private GOG test files ready",loadingFilenamesVisible:()=>false,
            paused:()=>{},progress:()=>{},begin:()=>{},finished:()=>{},log:()=>{},
            readFileChunk:(path,offset,length)=>read('/private-game-read',{path,offset,length}),
            readModChunk:(pack,slot,offset,length)=>read('/private-mod-read',{slot,offset,length}),
            pickMod:request=>fetch('/private-pack').then(r=>r.json()).then(result=>reply(request,result)),
            finishMod:(request,id,slots)=>{window.shdpCommitSlots=JSON.parse(slots);reply(request,{id});},
            removeMod:(request,id)=>reply(request,{}),cleanMods:(request,keep)=>reply(request,{}),cancelMod:request=>reply(request,{})};
          localStorage.setItem('re1.hd.enabled','false');localStorage.setItem('re1.skipMovies',String(!testSHDPMovies));
        })();''')
        page.add_init_script((WEB / 'hd-core.js').read_text() + '''
        (()=>{window.originalUploads=new Map();for(const cls of [WebGLRenderingContext,WebGL2RenderingContext]){
          const original=cls.prototype.texImage2D;cls.prototype.texImage2D=function(...args){
            const [target,level,format,w,h,border,input,type,data,offset=0]=args;
            if(Number.isInteger(w)&&w>=4&&h>=4&&w<=640&&h<=480&&input===6408&&type===5121&&ArrayBuffer.isView(data)&&data.BYTES_PER_ELEMENT===1){
              const bytes=data.subarray(offset,offset+w*h*4);if(bytes.length===w*h*4){const key=RE1HDCore.key(bytes,w,h);originalUploads.set(key,{w,h,key});}
            }
            return original.apply(this,args);
          };
        }})();''')
        page.goto(f'http://127.0.0.1:{server.server_port}/index.html')
        page.locator('#launcher-mods').click()
        page.wait_for_function('()=>!document.querySelector("#mods-add").disabled')
        if not args.reuse or page.locator('.mod-row').count() == 0:
            page.locator('#mods-add').click()
            deadline = time.monotonic() + 1800
            last = ''
            while page.evaluate('RE1Mods.busy()') and time.monotonic() < deadline:
                page.wait_for_timeout(1000)
                status = page.locator('#mods-status').inner_text()
                if status != last:
                    print('IMPORT', status.replace('\n', ' | '), flush=True)
                    last = status
            assert not page.evaluate('RE1Mods.busy()'), 'SHDP import timed out'
            assert 'installed and enabled' in page.locator('#mods-status').inner_text(), page.locator('#mods-status').inner_text()
        assert ('Classic REbirth translation' if args.rebirth else 'Seamless HD Project') in page.locator('#mods-list').inner_text()
        page.locator('#mods-list summary').filter(has_text='Credits').first.click()
        if not args.rebirth:assert 'MoArtis' in page.locator('#mods-list').inner_text()
        page.screenshot(path=str(args.output / 'mods-installed.png'))
        page.locator('#mods-close').click()
        page.locator('#launch').click()
        page.wait_for_function('()=>RE1Diag.frames>=3||document.querySelector("#error").open')
        assert not page.locator('#error').evaluate('(d)=>d.open'), page.locator('#error-text').inner_text()
        if not args.rebirth:assert page.evaluate('!!Module.re1HDActive'),'SHDP no longer selects the existing HD framebuffer'
        if args.movie_check:
            page.wait_for_function('()=>document.querySelector("#mod-video").videoWidth>=1280&&document.querySelector("#mod-video").currentTime>.2')
            page.wait_for_function('document.querySelector("#mod-video").hasAttribute("data-ready")')
            assert page.evaluate('getComputedStyle(document.querySelector("#mod-video")).opacity')=='1'
            assert page.locator('#mod-video').get_attribute('src').startswith('blob:')
            assert not media_requests,media_requests
            first=page.evaluate('document.querySelector("#mod-video").currentTime')
            page.wait_for_timeout(650)
            assert page.evaluate('document.querySelector("#mod-video").currentTime')>first+.3
            page.screenshot(path=str(args.output/'hd-movie.png'))
        if args.movie_suite:
            page.evaluate('window.re1OpenSettings();document.querySelector("#settings").close();RE1ModVideo.suspend(false)')
            for name in ['DM1','pu','eu4']:
                assert page.evaluate('(name)=>RE1ModVideo.open("/game/USA/MOVIE/"+name+".MP4",0)',name)
                page.wait_for_function('()=>document.querySelector("#mod-video").currentTime>.25&&document.querySelector("#mod-video").hasAttribute("data-ready")')
                state=page.evaluate('()=>{const v=document.querySelector("#mod-video");return {width:v.videoWidth,height:v.videoHeight,duration:v.duration,src:v.src};}')
                assert state['width']>=1258 and state['src'].startswith('blob:'),state
                page.evaluate('(ms)=>RE1ModVideo.playFrom(ms)',max(0,(state['duration']-1.5)*1000))
                page.wait_for_function('(end)=>document.querySelector("#mod-video").currentTime>end',arg=state['duration']-1.4)
                page.screenshot(path=str(args.output/('movie-'+name+'.png')))
                page.evaluate('RE1ModVideo.close()')
                print('MOVIE PASS',name,state['width'],state['height'],'seconds',state['duration'],flush=True)
            assert not media_requests,media_requests
        if args.textures_only:
            # Pause the renderer so it cannot consume a requested texture before
            # the test compares its pixels with the authors' original HD image.
            page.evaluate('window.re1OpenSettings()')
            probes=verify_textures(page,pack)
            (args.output/'texture-probes.json').write_text(json.dumps(probes,indent=2))
            assert not errors,errors
            print('PASS: existing installed SHDP hall/stair/typewriter, 71 item icons and 31 effect pages match official HD pixels; no reimport',flush=True)
            browser.close()
            raise SystemExit(0)
        # Advance startup/title/character dialogs with ordinary touch input.
        controlled = False
        chose_jill = False
        for step in range(200):
            page.wait_for_timeout(350)
            if args.jill and not chose_jill and page.evaluate('()=>RE1Diag.report().toLowerCase().includes("data/sel_back.pix")'):
                page.wait_for_timeout(2500)
                page.locator('#touch-right').click(delay=100)
                page.wait_for_timeout(800)
                chose_jill = True
            page.locator('#touch-action').click(delay=100)
            if step % 5 == 0:
                print('GAME', page.evaluate('()=>({frames:RE1Diag.frames,mode:RE1Presentation.mode,mod:RE1Mods.report(),hits:RE1Mods.hits,stage:document.querySelector("#boot-progress").textContent})'), flush=True)
            if page.evaluate('()=>!!(Module._re1_web_cheat(0,0)&256)'):
                controlled = True
                break
        page.wait_for_timeout(2000)
        page.screenshot(path=str(args.output / 'gameplay.png'))
        assert controlled, 'The gameplay test did not reach normal player control'
        page.evaluate('''()=>{
            window.controllerTurns=0;const turn=Module._re1_web_quick_turn;
            Module._re1_web_quick_turn=(...args)=>{const result=turn(...args);if(result)controllerTurns++;return result;};
            re1Native(1<<11,0,0,true);re1Native(0,0,0,true);
        }''')
        page.wait_for_function('()=>controllerTurns===1')
        page.wait_for_timeout(250)
        assert page.evaluate('controllerTurns')==1
        page.locator('#touch-inventory').click(delay=100)
        page.wait_for_timeout(1500)
        page.screenshot(path=str(args.output / 'inventory.png'))
        assert not page.evaluate('()=>!!(Module._re1_web_cheat(0,0)&256)'), 'Inventory did not open'
        if args.inventory_check:
            page.wait_for_function('RE1Mods.iconUploads>0')
            page.wait_for_timeout(700)
            page.screenshot(path=str(args.output/'hd-inventory.png'))
            print('PASS: actual game inventory uses HD icon sprites; '+page.evaluate('RE1Mods.report()'),flush=True)
            assert not errors,errors
            browser.close()
            raise SystemExit(0)
        page.locator('#touch-run').click(delay=100)
        for _ in range(40):
            if page.evaluate('()=>!!(Module._re1_web_cheat(0,0)&256)'):break
            page.locator('#touch-action').click(delay=100);page.wait_for_timeout(400)
        assert page.evaluate('()=>!!(Module._re1_web_cheat(0,0)&256)'), 'Inventory did not return to player control'
        start_hits = page.evaluate('RE1Mods.hits')
        # The same input model used by real touch buttons, held across ticks.
        page.evaluate('()=>{RE1Input.press("walk","up");RE1Input.press("run","run");}')
        page.wait_for_timeout(7000)
        page.evaluate('()=>{RE1Input.release("walk");RE1Input.release("run");}')
        page.wait_for_timeout(1500)
        page.screenshot(path=str(args.output / 'camera-walk.png'))
        for _ in range(40):
            if page.evaluate('()=>!!(Module._re1_web_cheat(0,0)&256)'):break
            page.locator('#touch-action').click(delay=100);page.wait_for_timeout(400)
        assert page.evaluate('()=>!!(Module._re1_web_cheat(0,0)&256)'), 'Walk did not return to player control'
        page.evaluate('()=>{Module._re1_web_cheat(1,0);if(!Module._re1_web_cheat(6,0))throw Error("Cannot open save screen");}')
        page.wait_for_function('()=>RE1Diag.report().toLowerCase().includes("data/type00.tim")')
        page.wait_for_timeout(2000)
        page.screenshot(path=str(args.output/'typewriter.png'))
        if args.rebirth:
            report={'report':page.evaluate('RE1Diag.report()'),'errors':errors,'localeBytes':page.evaluate('RE1Mods.localization()?.length||0'),'uploads':page.evaluate('RE1Mods.hits')}
            page.evaluate('window.re1OpenSettings()')
            report['documents']=verify_rebirth_documents(page,pack)
            (args.output/'result.json').write_text(json.dumps(report,indent=2))
            assert report['localeBytes']>16 and report['uploads']>0,report
            assert '[MOD TEXT] Loaded' in report['report'],report['report']
            assert not errors,errors
            print('PASS: actual translation imported, compiled strings loaded, gameplay, item inventory and save screen rendered',flush=True)
            browser.close();raise SystemExit(0)
        profile = json.loads((WEB / 'shdp-profile.json').read_text())
        aliases = {record['key']: record for record in profile['records']}
        uploaded = page.evaluate('Array.from(originalUploads.values())')
        matches = [aliases[item['key']] for item in uploaded if item['key'] in aliases]
        gpu_hits=page.evaluate('RE1Mods.hits')
        page.evaluate('window.re1OpenSettings()')
        probes=verify_textures(page,pack)
        report = {'uploads': uploaded, 'matches': matches, 'decodedModUploads': gpu_hits, 'verifiedTextureProbes':probes, 'uploadsBeforeWalk': start_hits, 'report': page.evaluate('RE1Diag.report()'), 'errors': errors}
        (args.output / 'result.json').write_text(json.dumps(report, indent=2))
        print('RESULT', json.dumps({'originalSurfaces': len(uploaded), 'matchedSurfaces': len(matches), 'decodedModUploads': report['decodedModUploads'], 'errors': errors}), flush=True)
        assert report['decodedModUploads'] > 0, 'No mod texture reached the actual renderer'
        assert any(record['file'].startswith('hires/bgd/') for record in matches), 'No room background was matched'
        for slot in page.evaluate('window.shdpCommitSlots||[]'):
            assert entries[slot].filename.lower().startswith('movie/') and entries[slot].filename.lower().endswith('.mp4'), 'Windows room/plugin files were imported'
        assert not errors, errors
        browser.close()
finally:
    server.shutdown()
    pack.close()
