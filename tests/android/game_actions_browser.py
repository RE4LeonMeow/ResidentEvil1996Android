"""Exercise game-action recovery through the actual WASM and Settings UI.
Synthetic boot images and save-file sentinels only; no commercial game data.
"""
import base64
import functools
import http.server
import json
import pathlib
import struct
import threading
from playwright.sync_api import sync_playwright

ROOT=pathlib.Path(__file__).resolve().parents[2]
WEB=ROOT/'android/app/src/main/assets/web'
BOOT=['fontus.tim','font03t.tim','optkey03.tim','status.tim','statface.tim','blue.tim','staitem.tim','kage.tim']
palette=struct.pack('<256H',*([0,31,992,31744,1023]+[0]*251))
pixels=bytes(1+(x//128)+(y//128)*2 for y in range(256) for x in range(256))
TIM=struct.pack('<II',16,9)+struct.pack('<I4H',524,0,0,256,1)+palette+struct.pack('<I4H',65548,0,0,128,256)+pixels
FILES={'Data/'+name:TIM for name in BOOT}
FILES['Data/item_all.pix']=bytes(91200)

class Handler(http.server.SimpleHTTPRequestHandler):
    def log_message(self,*_):pass
    def do_GET(self):
        if self.path=='/manifest':
            raw=json.dumps([{'path':name,'size':len(data)} for name,data in FILES.items()]).encode()
            self.send_response(200);self.send_header('Content-Type','application/json');self.end_headers();self.wfile.write(raw);return
        return super().do_GET()
    def end_headers(self):
        self.send_header('Content-Security-Policy',"default-src 'self'; script-src 'self' 'wasm-unsafe-eval'; style-src 'self'; connect-src 'self' blob:; worker-src 'self' blob:; img-src 'self' data:; object-src 'none'; base-uri 'none'; frame-src 'none'")
        super().end_headers()

server=http.server.ThreadingHTTPServer(('127.0.0.1',0),functools.partial(Handler,directory=str(WEB)))
threading.Thread(target=server.serve_forever,daemon=True).start()
with sync_playwright() as p:
    browser=p.chromium.launch(executable_path='/usr/bin/chromium',headless=True,args=['--no-sandbox','--enable-unsafe-swiftshader'])
    page=browser.new_page(viewport={'width':960,'height':480},has_touch=True)
    errors=[];page.on('pageerror',lambda error:errors.append(str(error)))
    page.add_init_script('window.fixtureFiles='+json.dumps({name:base64.b64encode(data).decode() for name,data in FILES.items()})+''';
      window.AndroidLauncher={ready:()=>true,status:()=>"Synthetic game-action test",paused:()=>{},loadingFilenamesVisible:()=>false,
        readFileChunk:(name,offset,length)=>fixtureFiles[name]?'ok:'+btoa(atob(fixtureFiles[name]).slice(offset,offset+length)):'error:Missing fixture'};''')
    page.goto(f'http://127.0.0.1:{server.server_port}/index.html')
    page.evaluate('''()=>{
      const start=window.re1Ready;
      window.re1Ready=()=>{
        Module.FS.writeFile('/persist/SAVE/savedat1.dat',new Uint8Array([1,7,20,26,42]));
        return start();
      };
    }''')
    page.locator('#graphics-test').click()
    page.wait_for_function('()=>RE1Diag.frames>=2',timeout=90000)
    page.evaluate('window.re1OpenSettings()')
    page.locator('#game-action-recovery summary').click()
    before=page.evaluate('''()=>{
      RE1Input.settings.positions.aim=[72,39];RE1Input.settings.sizes.aim=1.3;
      RE1Input.settings.toggles.aim=true;RE1Input.settings.mapping[0]=32768;
      return {touch:JSON.stringify(RE1Input.settings),save:Array.from(Module.FS.readFile('/persist/SAVE/savedat1.dat')),
        bindings:Array.from({length:6},(_,i)=>Module._re1_web_game_action_binding(i))};
    }''')
    page.locator('#restore-game-actions').click()
    after=page.evaluate('''()=>({touch:JSON.stringify(RE1Input.settings),save:Array.from(Module.FS.readFile('/persist/SAVE/savedat1.dat')),
      bindings:Array.from({length:6},(_,i)=>Module._re1_web_game_action_binding(i)),report:RE1Diag.report()})''')
    assert after['bindings']==[0x80,0x40,8,0x800,0x900,0],after
    assert after['save']==before['save'] and after['touch']==before['touch']
    assert 'Game actions restored' in page.locator('#game-actions-status').inner_text()
    assert 'AIM=0x8' in after['report'] and 'PAUSE=0x900' in after['report']
    assert page.evaluate('Module._re1_web_game_action_binding(-1)')==-1
    assert page.evaluate('Module._re1_web_game_action_binding(6)')==-1
    page.locator('#close-settings').click()
    assert page.evaluate('Module._re1_web_restore_game_actions()')==0,'Recovery must require pausing'
    page.wait_for_function('()=>document.querySelector("#error").open',timeout=30000)
    assert page.locator('#error h2').inner_text()=='Graphics check passed'
    assert not errors,errors
    print('PASS: actual WASM action defaults, paused guard, readable binding diagnostics, settings/save preservation and graphics/audio completion')
    browser.close()
server.shutdown()
