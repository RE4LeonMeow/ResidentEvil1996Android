"""Actual Chromium/OPFS mod manager tests with a simulated native ZIP transport.
Synthetic textures/data only. Android ZIP I/O is covered by ModArchiveTest.
"""
import base64
import functools
import http.server
import json
import pathlib
import os
import struct
import threading
import zlib

from playwright.sync_api import sync_playwright

ROOT = pathlib.Path(__file__).resolve().parents[2]
WEB = ROOT / 'android/app/src/main/assets/web'


def png(w, h, colour):
    def chunk(tag, body):
        return struct.pack('>I', len(body)) + tag + body + struct.pack('>I', zlib.crc32(tag + body))
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress((b'\0' + bytes(colour) * w) * h)) + chunk(b'IEND', b'')


def png_pixels(w, h, pixels):
    def chunk(tag, body):
        return struct.pack('>I', len(body)) + tag + body + struct.pack('>I', zlib.crc32(tag + body))
    rows = b''.join(b'\0' + pixels[y * w * 4:(y + 1) * w * 4] for y in range(h))
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b'')


palette = struct.pack('<256H', *([0, 31, 992, 31744] + [0] * 252))
pixels = bytes([1]) * (256 * 256)
TIM = struct.pack('<II', 16, 9) + struct.pack('<I4H', 524, 0, 0, 256, 1) + palette + struct.pack('<I4H', 65548, 0, 0, 128, 256) + pixels
FILES = {'Data/' + name: TIM for name in ['fontus.tim', 'font03t.tim', 'optkey03.tim', 'status.tim', 'statface.tim', 'blue.tim', 'staitem.tim', 'kage.tim']}
FILES.update({'Data/item_all.pix': bytes(91200), 'Stage1/ROOM1000.rdt': bytes(128), 'Sound/gun01.wav': b'old sound'})
FILES['Enemy/EM1000.EMD']=b'original model fixture'
FILES['Movie/Capcom.avi']=b'original movie fixture'
RAW = bytes((n * 37 + 11) % 256 for n in range(300000))


def fixture(slug, name, colour, overrides=False, bad_crc=False, unsafe=False):
    manifest = {'format': 're1-android-mod', 'formatVersion': 1, 'game': 're1-usa', 'id': slug, 'name': name, 'author': 'Test author', 'version': '1.0'}
    data = {'Example/mod.json': json.dumps(manifest).encode(), 'Example/textures/Data/fontus.tim.png': png(512, 512, colour)}
    if overrides:
        data['Example/files/Sound/gun01.wav'] = RAW
    if unsafe:
        data['Example/files/SAVE/savedat1.dat'] = b'not a save'
    entries = []
    for slot, (path, contents) in enumerate(data.items()):
        entries.append({'path': path, 'slot': slot, 'size': len(contents), 'crc': (zlib.crc32(contents) ^ (1 if bad_crc and slot == 1 else 0)), 'data': base64.b64encode(contents).decode()})
    return {'name': name + '.zip', 'files': entries}


FIXTURES = {
    'green': fixture('test.green', 'Green pack', [0, 255, 0, 255], overrides=True),
    'blue': fixture('test.blue', 'Blue pack', [0, 0, 255, 255]),
    'broken': fixture('test.green', 'Broken update', [255, 0, 0, 255], bad_crc=True),
    'unsafe': fixture('test.bad', '<img src=x onerror=alert(1)>', [255, 0, 0, 255], unsafe=True),
    'cancel': fixture('test.cancel', 'Cancelled pack', [255, 255, 0, 255]),
}
pc_files={'Megaman/USA/Enemy/EM1000.EMD':b'PC model fixture','Megaman/ddraw.dll':b'ignored Windows plugin','Megaman/MOVIE/Capcom.mp4':RAW*4}
FIXTURES['pc']={'name':'Megaman.zip','files':[{'path':name,'slot':n,'size':len(data),'crc':zlib.crc32(data),'data':base64.b64encode(data).decode()} for n,(name,data) in enumerate(pc_files.items())]}
info_files={'Claire/Enemy/EM1000.EMD':b'metadata model fixture','Claire/mod-info.txt':b'\xef\xbb\xbfName: Claire Redfield\nDescription: Replaces a character.\nLiteral <img src=x> text.\nAuthor: Original mod author\nVersion: 1.1'}
FIXTURES['pc-info']={'name':'unknown-download.zip','files':[{'path':name,'slot':n,'size':len(data),'crc':zlib.crc32(data),'data':base64.b64encode(data).decode()} for n,(name,data) in enumerate(info_files.items())]}
EDGE_KEY = '123456789abcdef0123456789abcdef0_4x1'
EDGE_PIXELS = bytes([255, 0, 0, 0, 50, 100, 200, 128, 10, 20, 30, 255, 250, 60, 90, 1])
edge_manifest = {'format': 're1-android-mod', 'formatVersion': 1, 'game': 're1-usa', 'id': 'test.edges', 'name': 'Transparent edges',
                 'textures': [{'key': EDGE_KEY, 'file': 'edge.png', 'alpha': 'image'}]}
edge_files = {'mod.json': json.dumps(edge_manifest).encode(), 'edge.png': png_pixels(4, 1, EDGE_PIXELS)}
FIXTURES['edges'] = {'name': 'edges.zip', 'files': [{'path': name, 'slot': n, 'size': len(data), 'crc': zlib.crc32(data), 'data': base64.b64encode(data).decode()}
                                                for n, (name, data) in enumerate(edge_files.items())]}
FIXTURES['partial'] = {'name': 'patch-only.zip', 'files': [{'path': 'hires/bgd/DD17F52F.webp', 'slot': 0, 'size': 0, 'crc': 0, 'data': ''}]}
rebirth_files={
    'manifest.txt':b'[MOD]\nName = Rebirth test\nUseRdtXml = 1\n',
    'encoding.xml':'<Encoding width="8"><Entry Encode="0" Char=" "/><Entry Encode="27" Char="?"/><Entry Encode="29" Char="A"/><Entry Encode="95" Char="ą"/></Encoding>'.encode(),
    'misc.xml':'<Strings><Text>Aą</Text></Strings>'.encode(),
    'Data/PBFONT0.tim':struct.pack('<II I4H',16,2,131084,640,0,256,256)+struct.pack('<H',32767)*65536,
    'ddraw.dll':b'not executed',
}
FIXTURES['rebirth']={'name':'Rebirth.zip','files':[{'path':name,'slot':n,'size':len(data),'crc':zlib.crc32(data),'data':base64.b64encode(data).decode()} for n,(name,data) in enumerate(rebirth_files.items())]}


class Handler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def do_GET(self):
        if self.path == '/manifest':
            data = json.dumps([{'path': name, 'size': len(data)} for name, data in FILES.items()]).encode()
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            self.wfile.write(data)
            return
        return super().do_GET()

    def end_headers(self):
        self.send_header('Content-Security-Policy', "default-src 'self'; script-src 'self' 'wasm-unsafe-eval'; worker-src 'self' blob:; style-src 'self'; connect-src 'self' blob:; img-src 'self' data:; object-src 'none'; base-uri 'none'; frame-src 'none'")
        super().end_headers()


server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), functools.partial(Handler, directory=str(WEB)))
threading.Thread(target=server.serve_forever, daemon=True).start()
with sync_playwright() as p:
    browser = p.chromium.launch(executable_path='/usr/bin/chromium', headless=True, args=['--no-sandbox', '--enable-unsafe-swiftshader'])
    page = browser.new_page(viewport={'width': 1000, 'height': 650})
    errors = []
    page.on('pageerror', lambda error: errors.append(str(error)))
    encoded = {name: base64.b64encode(data).decode() for name, data in FILES.items()}
    page.add_init_script('window.fixtureGame=' + json.dumps(encoded) + ';window.fixturePacks=' + json.dumps(FIXTURES) + ';')
    page.add_init_script('''(() => {
      const binary=base64=>Uint8Array.from(atob(base64),c=>c.charCodeAt(0));
      const encode=bytes=>{let text='';for(let at=0;at<bytes.length;at+=8192)text+=String.fromCharCode(...bytes.subarray(at,at+8192));return btoa(text);};
      const game=new Map(Object.entries(fixtureGame).map(([name,data])=>[name,binary(data)]));
      const refs=JSON.parse(localStorage.getItem('fixture.refs')||'{}');
      window.fixtureNext='green';window.fixtureCommits=[];window.fixtureRemoved=[];window.fixtureCancelCalls=0;
      let staged=null,pick=null;
      const reply=(request,result)=>setTimeout(()=>window.RE1Mods.nativeResult(request,result),0);
      window.AndroidLauncher={
        ready:()=>true,status:()=>"Synthetic game folder",loadingFilenamesVisible:()=>false,
        paused:()=>{},progress:()=>{},begin:()=>{},finished:()=>{},log:()=>{},
        openExternal:link=>window.fixtureLink=link,
        readFileChunk:(name,offset,length)=>game.has(name)?'ok:'+encode(game.get(name).subarray(offset,offset+length)):'error:Missing fixture',
        pickMod(request){
          pick=request;const kind=window.fixtureNext,pack=fixturePacks[kind];
          const next=Number(localStorage.getItem('fixture.serial')||0)+1;localStorage.setItem('fixture.serial',String(next));
          const id='pack-'+next.toString(16).padStart(32,'0');refs[id]=kind;localStorage.setItem('fixture.refs',JSON.stringify(refs));staged=id;
          setTimeout(()=>{if(pick!==request)return;pick=null;reply(request,{id,name:pack.name,files:pack.files.map(({data,...entry})=>entry)});},kind==='cancel'?500:10);
        },
        readModChunk(id,slot,offset,length){
          const pack=fixturePacks[refs[id]],entry=pack?.files.find(e=>e.slot===slot);
          return entry?'ok:'+encode(binary(entry.data).subarray(offset,offset+length)):'error:Missing mod entry';
        },
        finishMod(request,id,slots){fixtureCommits.push({id,slots:JSON.parse(slots)});staged=null;reply(request,{id});},
        cancelMod(request){
          fixtureCancelCalls++;staged=null;
          if(pick){const original=pick;pick=null;reply(original,{cancelled:true});}
          reply(request,{});
        },
        removeMod(request,id){fixtureRemoved.push(id);reply(request,{});},
        cleanMods(request,keep){reply(request,{});}
      };
    })();''')
    page.goto(f'http://127.0.0.1:{server.server_port}/index.html')
    page.locator('#launcher-mods').click()
    page.wait_for_function('()=>!document.querySelector("#mods-add").disabled')
    assert page.locator('#mods-empty').is_visible()
    page.set_viewport_size({'width':640,'height':360})
    page.locator('#mods-help-button').click()
    page.wait_for_function('()=>document.activeElement.id==="mods-help-title"&&document.querySelector("#mods-help").scrollTop===0')
    assert page.locator('#mods-help').evaluate('(dialog)=>dialog.scrollHeight>dialog.clientHeight')
    assert page.evaluate('async()=>{await document.fonts.ready;return [...document.fonts].some(face=>face.family.includes("Compacta Launcher")&&face.status==="loaded")}')
    if os.environ.get('RE1_UI_CAPTURE_DIR'):
        capture=pathlib.Path(os.environ['RE1_UI_CAPTURE_DIR']);capture.mkdir(parents=True,exist_ok=True)
        page.screenshot(path=str(capture/'mods-help-640.png'))
    page.locator('#mods-help').evaluate('(dialog)=>dialog.scrollTop=dialog.scrollHeight')
    page.locator('#mods-help-close').click()
    page.locator('#mods-help-button').click()
    page.wait_for_function('()=>document.activeElement.id==="mods-help-title"&&document.querySelector("#mods-help").scrollTop===0')
    page.keyboard.press('Tab')
    assert page.evaluate('document.activeElement.id')=='mods-discord'
    page.locator('#mods-help-close').click()
    page.locator('#mods-help-button').click()
    page.wait_for_function('()=>document.querySelector("#mods-help").scrollTop===0')
    assert '#mods' in page.locator('#mods-help').inner_text()
    assert "4wu4_" not in page.locator('#mods-help').inner_text()
    page.locator('#mods-guide-button').click()
    assert page.evaluate('window.fixtureLink')=='https://github.com/RE4LeonMeow/ResidentEvil1996Android#mods-and-making-texture-packs'
    assert page.locator('#mods-help').evaluate('(dialog)=>dialog.open')
    page.locator('#mods-discord').click()
    assert page.evaluate('window.fixtureLink') == 'https://discord.gg/5PVsXAgvM2'
    page.locator('#mods-help-close').click()
    page.set_viewport_size({'width':1000,'height':650})

    def install(kind, result='installed and enabled'):
        page.evaluate('(kind)=>window.fixtureNext=kind', kind)
        page.locator('#mods-add').click()
        if kind=='pc':
            page.wait_for_function('()=>document.querySelector("#mod-details").open')
            assert page.locator('#mod-name').input_value()=='Megaman'
            page.locator('#mod-name').fill(' ');page.locator('#mod-details-save').click()
            assert page.locator('#mod-details').evaluate('(d)=>d.open')
            page.locator('#mod-name').fill('Megaman');page.locator('#mod-description').fill('My model replacement.')
            page.locator('#mod-details-save').click()
        page.wait_for_function('()=>!RE1Mods.busy()', timeout=60000)
        assert result in page.locator('#mods-status').inner_text(), page.locator('#mods-status').inner_text()

    install('green')
    assert page.locator('.mod-row').count() == 1
    assert page.evaluate('fixtureCommits[0].slots') == [2]
    install('blue')
    assert page.locator('.mod-row strong').all_text_contents() == ['Blue pack', 'Green pack']
    page.get_by_role('button', name='Move Blue pack down', exact=True).click()
    page.wait_for_function('()=>!RE1Mods.busy()')
    assert page.locator('.mod-row strong').all_text_contents() == ['Green pack', 'Blue pack']
    assert 'covered by a mod above' in page.locator('.mod-row').nth(1).inner_text()
    page.get_by_role('checkbox', name='Enable Green pack', exact=True).uncheck()
    page.wait_for_function('()=>!RE1Mods.busy()')
    page.reload()
    page.locator('#launcher-mods').click()
    page.wait_for_function('()=>!document.querySelector("#mods-add").disabled')
    assert not page.get_by_role('checkbox', name='Enable Green pack', exact=True).is_checked()
    page.get_by_role('checkbox', name='Enable Green pack', exact=True).check()
    page.wait_for_function('()=>!RE1Mods.busy()')
    install('broken', 'damaged')
    assert page.locator('.mod-row strong').all_text_contents() == ['Green pack', 'Blue pack']
    install('unsafe', 'Unsupported game resource')
    install('partial', 'patch-only')
    assert page.locator('#mods-list img').count() == 0
    page.evaluate("window.fixtureNext='cancel'")
    page.locator('#mods-add').click()
    page.locator('#mods-cancel').click()
    page.wait_for_function('()=>!RE1Mods.busy()', timeout=30000)
    assert 'cancelled' in page.locator('#mods-status').inner_text()
    assert page.evaluate('fixtureCancelCalls') > 0
    assert page.locator('.mod-row').count() == 2
    page.get_by_role('button', name='Remove Blue pack', exact=True).click()
    page.wait_for_function('()=>!RE1Mods.busy()')
    assert page.locator('.mod-row strong').all_text_contents() == ['Green pack']
    install('edges')
    install('pc-info')
    assert not page.locator('#mod-details').evaluate('(d)=>d.open')
    info_row=page.locator('.mod-row').filter(has=page.get_by_role('checkbox',name='Enable Claire Redfield',exact=True))
    assert 'Original mod author · 1.1' in info_row.inner_text()
    assert 'Literal <img src=x> text.' in info_row.inner_text()
    assert page.locator('#mods-list img').count()==0
    assert page.evaluate('fixtureCommits.at(-1).slots')==[0]
    page.get_by_role('button',name='Remove Claire Redfield',exact=True).click();page.wait_for_function('()=>!RE1Mods.busy()')
    # Cancelling the missing-metadata dialog also closes the staged ZIP.
    page.evaluate("window.fixtureNext='pc'");page.locator('#mods-add').click()
    page.wait_for_function('()=>document.querySelector("#mod-details").open')
    before=page.locator('.mod-row').count();page.evaluate('RE1Mods.close()')
    page.wait_for_function('()=>!RE1Mods.busy()')
    assert page.locator('.mod-row').count()==before and page.locator('#mods').evaluate('(d)=>d.open')
    install('pc')
    assert page.get_by_role('checkbox',name='Enable Megaman',exact=True).is_checked()
    page.get_by_role('button',name='Edit details for Megaman',exact=True).click()
    page.locator('#mod-name').fill('My custom label');page.locator('#mod-description').fill('My <script>literal</script> description.')
    page.locator('#mod-details-save').click();page.wait_for_function('()=>!RE1Mods.busy()')
    page.reload();page.locator('#launcher-mods').click();page.wait_for_function('()=>!document.querySelector("#mods-add").disabled')
    assert page.get_by_role('checkbox',name='Enable My custom label',exact=True).is_checked()
    assert 'My <script>literal</script> description.' in page.locator('#mods-list').inner_text()
    assert page.locator('#mods-list script').count()==0
    page.get_by_role('button',name='Edit details for My custom label',exact=True).click()
    page.locator('#mod-name').fill('Megaman');page.locator('#mod-details-save').click();page.wait_for_function('()=>!RE1Mods.busy()')
    # A modal import manager cannot be bypassed by clicking Launch from script.
    page.evaluate('document.querySelector("#launch").click()')
    assert page.evaluate('typeof Module.FS') == 'undefined'
    page.locator('#mods-close').click()
    page.evaluate('()=>{window.re1Ready=()=>{window.fixtureReady=true;};}')
    page.locator('#launch').click()
    page.wait_for_function('()=>window.fixtureReady===true', timeout=90000)
    data = page.evaluate("Array.from(Module.FS.readFile('/game/USA/Sound/gun01.wav'))")
    assert bytes(data) == RAW
    assert bytes(page.evaluate("Array.from(Module.FS.readFile('/game/USA/Enemy/EM1000.EMD'))"))==b'PC model fixture'
    # Source provenance follows active raw-file overrides, including PC-style
    # separators/case; a texture-only replacement does not change its source.
    assert page.evaluate('(path)=>RE1Mods.fileOverridden(path)',r'eNeMy\eM1000.EmD')
    assert page.evaluate('RE1Mods.fileOverridden("Sound/gun01.wav")')
    assert not page.evaluate('RE1Mods.fileOverridden("Data/fontus.tim")')
    assert not page.evaluate('RE1Mods.fileOverridden(null)')
    page.add_script_tag(url=f'http://127.0.0.1:{server.server_port}/hd-core.js')
    key = page.evaluate('''()=>{
      const data=Uint8Array.from(atob(fixtureGame['Data/fontus.tim']),c=>c.charCodeAt(0));
      const image=[...RE1HDCore.images(data,'Data/fontus.tim')][0];return RE1HDCore.key(image.rgba,image.w,image.h);
    }''')
    page.evaluate('(key)=>RE1HDPack.request(key)', key)
    page.wait_for_function('(key)=>RE1HDPack.pixels(key)>0', arg=key)
    result = page.evaluate('''key=>{
      const bytes=new Uint8Array(RE1HDPack.pixels(key));if(!RE1HDPack.take(key,bytes))throw Error('Missing decoded texture');
      const v=new DataView(bytes.buffer);return {version:v.getUint32(4,true),original:[v.getUint32(8,true),v.getUint32(12,true)],
        replacement:[v.getUint32(16,true),v.getUint32(20,true)],pixel:Array.from(bytes.subarray(32,36))};
    }''', key)
    assert result == {'version': 3, 'original': [256, 256], 'replacement': [512, 512], 'pixel': [0, 255, 0, 255]}, result
    page.evaluate('(key)=>RE1HDPack.request(key)', EDGE_KEY)
    page.wait_for_function('(key)=>RE1HDPack.pixels(key)>0', arg=EDGE_KEY)
    edge_result = page.evaluate('''key=>{const bytes=new Uint8Array(RE1HDPack.pixels(key));RE1HDPack.take(key,bytes);return Array.from(bytes.subarray(32));}''', EDGE_KEY)
    assert bytes(edge_result) == EDGE_PIXELS, 'Transparent/partial-alpha RGB was changed during import'
    assert 'Data overrides: 3' in page.evaluate('RE1Mods.report()')
    cancelled=page.evaluate('''async()=>{
      const pending=RE1Mods.movieBlob('/game/USA/Movie/capcom.mp4');RE1Mods.cancelMovie();
      return pending.then(()=>false,error=>error.message.includes('cancelled'));
    }''')
    assert cancelled,'A skipped movie continued preparing its temporary file'
    movie=page.evaluate('''async()=>{
      const file=await RE1Mods.movieBlob('/game/USA/Movie/capcom.MP4');
      return {size:file.size,type:file.type,bytes:Array.from(new Uint8Array(await file.arrayBuffer()))};
    }''')
    assert movie['size']==len(RAW)*4 and movie['type']=='video/mp4' and bytes(movie['bytes'])==RAW*4
    cached=page.evaluate('''async()=>{const file=await RE1Mods.movieBlob('/game/USA/MOVIE/Capcom.mp4');return file.size;}''')
    assert cached==len(RAW)*4
    assert not page.locator('#error').evaluate('(dialog)=>dialog.open')
    assert not errors, errors
    # Disabling the imported PC model removes its overlay on the next launch.
    page.reload();page.locator('#launcher-mods').click()
    page.wait_for_function('()=>!document.querySelector("#mods-add").disabled')
    page.get_by_role('checkbox',name='Enable Megaman',exact=True).uncheck()
    page.wait_for_function('()=>!RE1Mods.busy()');page.locator('#mods-close').click()
    page.evaluate('RE1Mods.prepare()')
    assert page.evaluate('RE1Mods.readFile("Enemy/EM1000.EMD",0,1)') is None
    assert not page.evaluate('RE1Mods.fileOverridden("Enemy/EM1000.EMD")')
    page.reload();page.locator('#launcher-mods').click()
    page.wait_for_function('()=>!document.querySelector("#mods-add").disabled')
    assert not page.get_by_role('checkbox',name='Enable Megaman',exact=True).is_checked()
    page.get_by_role('checkbox',name='Enable Megaman',exact=True).check()
    page.wait_for_function('()=>!RE1Mods.busy()');page.locator('#mods-close').click()
    page.evaluate('RE1Mods.prepare()')
    assert page.evaluate('Array.from(RE1Mods.readFile("Enemy/EM1000.EMD",0,2))')==list(b'PC')
    # Translation metadata/cache survives restart and does not affect model
    # overlays. Conflicting fonts must produce an actionable launch error.
    page.reload();page.locator('#launcher-mods').click()
    page.wait_for_function('()=>!document.querySelector("#mods-add").disabled')
    page.evaluate('window.fixtureNext="rebirth"');page.locator('#mods-add').click()
    page.wait_for_function('()=>!RE1Mods.busy()')
    assert 'installed and enabled' in page.locator('#mods-status').inner_text(),page.locator('#mods-status').inner_text()
    row=page.locator('.mod-row').filter(has=page.get_by_text('Rebirth test',exact=True))
    row.locator('summary').filter(has_text='Compatibility report').click()
    assert 'ddraw.dll' in row.inner_text() and 'not executed' in row.inner_text()
    page.get_by_role('checkbox',name='Enable Transparent edges',exact=True).uncheck()
    page.wait_for_function('()=>!RE1Mods.busy()')
    page.locator('#mods-close').click();page.evaluate('RE1Mods.prepare()')
    assert page.evaluate('RE1Mods.localization()?.length')>16
    assert not page.evaluate('RE1HDPack.prepare()'), 'Native-size translation unexpectedly forced the HD framebuffer'
    assert page.evaluate('RE1HDPack.active'), 'Native-size translation textures were disabled with the HD framebuffer'
    assert page.evaluate('Array.from(RE1Mods.readFile("Enemy/EM1000.EMD",0,2))')==list(b'PC')
    page.reload();page.locator('#launcher-mods').click()
    page.wait_for_function('()=>!document.querySelector("#mods-add").disabled')
    for _ in range(12):
        down=page.get_by_role('button',name='Move Rebirth test down',exact=True)
        if down.is_disabled():break
        down.click();page.wait_for_function('()=>!RE1Mods.busy()')
    page.locator('#mods-close').click()
    blocked=page.evaluate('async()=>{try{await RE1Mods.prepare();return "";}catch(error){return error.message;}}')
    assert 'above other font/UI packs' in blocked,blocked
    page.locator('#launcher-mods').click()
    page.get_by_role('checkbox',name='Enable Rebirth test',exact=True).uncheck()
    page.wait_for_function('()=>!RE1Mods.busy()')
    page.reload();page.locator('#launcher-mods').click()
    page.wait_for_function('()=>!document.querySelector("#mods-add").disabled')
    assert not page.get_by_role('checkbox',name='Enable Rebirth test',exact=True).is_checked()
    page.locator('#mods-close').click();page.evaluate('RE1Mods.prepare()')
    assert page.evaluate('RE1Mods.localization()') is None
    assert not errors, errors
    browser.close()
server.shutdown()
print('PASS: native-bridge mod ZIP install, ?/Discord/+ UI, source/persistence/priority/toggles/removal, CRC and SAVE-target rejection, immediate cancellation, real WASM data overlay and decoded texture bytes')
