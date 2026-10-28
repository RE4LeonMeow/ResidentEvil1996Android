"""Real Chromium checks for launcher, touch, persistence, range I/O and GLES shaders.
No commercial game data is used and this does not claim gameplay validation.
Run from repository root with Python playwright and system chromium installed.
"""
import functools,json,pathlib,threading,http.server,re,ast,urllib.parse,struct,base64
from playwright.sync_api import sync_playwright
ROOT=pathlib.Path(__file__).resolve().parents[2]
WEB=ROOT/'android/app/src/main/assets/web'
PAYLOAD=bytes(range(256))*2300
BOOT=['fontus.tim','font03t.tim','optkey03.tim','status.tim','statface.tim','blue.tim','staitem.tim','kage.tim']
# Legal synthetic 8bpp TIM textures, not game art, for boot-task execution.
palette=struct.pack('<256H',*([0,31,992,31744,1023]+[0]*251))
pixels=bytes(1+(x//128)+(y//128)*2 for y in range(256) for x in range(256))
TIM=struct.pack('<II',16,9)+struct.pack('<I4H',524,0,0,256,1)+palette+struct.pack('<I4H',65548,0,0,128,256)+pixels
MOVIE=bytes((n*37+11)%256 for n in range(1535452))
FILES={'Data/test.bin':PAYLOAD,'Movie/capcom.avi':MOVIE,'Data/item_all.pix':bytes(91200),**{'Data/'+name:TIM for name in BOOT}}
class Server(http.server.SimpleHTTPRequestHandler):
    def __init__(self,*a,**kw):super().__init__(*a,directory=str(WEB),**kw)
    def log_message(self,*a):pass
    def do_GET(self):
        parsed=urllib.parse.urlsplit(self.path)
        if parsed.path=='/manifest':
            data=json.dumps([{'path':name,'size':len(data)} for name,data in FILES.items()]).encode()
            self.send_response(200);self.send_header('Content-Type','application/json');self.end_headers();self.wfile.write(data);return
        if parsed.path.startswith('/data/'):
            path=urllib.parse.unquote(parsed.path[6:]);payload=FILES[path]
            query=urllib.parse.parse_qs(parsed.query)
            start=int(query['offset'][0]);end=start+int(query['length'][0])-1
            assert self.headers['Range']==f'bytes={start}-{end}'
            data=payload[start:end+1]
            self.send_response(206);self.send_header('Content-Type','application/octet-stream');self.send_header('Content-Length',str(len(data)));self.end_headers();self.wfile.write(data);return
        return super().do_GET()
    def end_headers(self):
        self.send_header('Content-Security-Policy',"default-src 'self'; script-src 'self' 'wasm-unsafe-eval'; style-src 'self'; connect-src 'self'; img-src 'self' data:; object-src 'none'; base-uri 'none'; frame-src 'none'")
        super().end_headers()
server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Server)
threading.Thread(target=server.serve_forever,daemon=True).start()
with sync_playwright() as p:
    browser=p.chromium.launch(executable_path='/usr/bin/chromium',headless=True,args=['--no-sandbox','--enable-unsafe-swiftshader'])
    page=browser.new_page(viewport={'width':960,'height':480},has_touch=True)
    errors=[];page.on('pageerror',lambda error:errors.append(str(error)))
    page.goto(f'http://127.0.0.1:{server.server_port}/index.html')
    page.get_by_text('Controls & settings',exact=True).first.click()
    assert page.locator('#settings').evaluate('(d)=>d.open')
    page.locator('#cheats-tab').click()
    assert page.locator('#cheats-warning').is_visible()
    assert page.locator('#cheats-content').is_hidden()
    page.locator('#cheats-decline').click()
    assert page.locator('#controls-panel').is_visible()
    page.locator('#cheats-tab').click();page.locator('#cheats-agree').click()
    assert page.locator('#cheats-content').is_visible()
    assert page.locator('#cheat-save').is_disabled()
    assert page.locator('#cheat-health').is_disabled()
    assert page.locator('#cheat-ignore-player').is_disabled()
    page.locator('#controls-tab').click()
    page.locator('#opacity').fill('75');page.locator('#opacity').dispatch_event('input')
    assert page.evaluate('RE1Input.settings.opacity')==.75
    page.locator('#edit-layout').click()
    button=page.locator('#touch-action');box=button.bounding_box()
    page.mouse.move(box['x']+box['width']/2,box['y']+box['height']/2);page.mouse.down();page.mouse.move(700,260);page.mouse.up()
    assert page.evaluate('RE1Input.settings.positions.action[0]')>60
    page.locator('#save-layout').click();page.locator('#close-settings').click()
    page.reload();assert page.evaluate('RE1Input.settings.opacity')==.75
    page.locator('#launcher-settings').click();page.locator('#cheats-tab').click()
    assert page.locator('#cheats-warning').is_visible()
    page.locator('#close-settings').click()
    page.evaluate('''() => { window.AndroidLauncher={ready:()=>true,status:()=>"Synthetic storage fixture"};
      window.re1Ready=()=>{window.testReady=true;}; window.re1Exit=()=>{window.testExited=true;}; }''')
    page.wait_for_function('() => !document.getElementById("launch").disabled')
    page.locator('#launch').click()
    page.wait_for_function('() => window.testReady === true',timeout=90000)
    # Exercise range reads across chunk boundaries, seeks, exact binary bytes and EOF.
    result=page.evaluate('''() => {
      const fs=Module.FS, data=fs.readFile('/game/USA/Data/test.bin');
      if(data.length!==588800)throw Error('Wrong file size');
      for(let n=0;n<data.length;n++)if(data[n]!==n%256)throw Error('Binary byte mismatch '+n);
      const stream=fs.open('/game/USA/Data/test.bin','r'), tail=new Uint8Array(50);
      fs.llseek(stream,588780,0);const n=fs.read(stream,tail,0,50);fs.close(stream);
      if(n!==20)throw Error('EOF clamp failed');
      fs.writeFile('/persist/SAVE/test.dat',new Uint8Array([0,128,255]));
      window.re1Sync();return {bytes:data.length,tail:n};
    }''')
    page.wait_for_function('() => !window.re1SyncBusy')
    # Actual game shaders compiled with the same GLSL ES version as the port.
    source=(ROOT/'src/marni/MarniDX_GL.cpp').read_text()
    shaders=[]
    for name in ['kVertexSrc','kFragmentSrc']:
        block=source.split('static const char* '+name+' =',1)[1].split(';\n',1)[0]
        block=re.sub(r'#ifdef __EMSCRIPTEN__\s*(.*?)#else.*?#endif',r'\1',block,flags=re.S)
        block=re.sub(r'//[^\n]*','',block)
        shaders.append(''.join(ast.literal_eval(s) for s in re.findall(r'"(?:[^"\\]|\\.)*"',block)))
    page.evaluate('''sources => {
      const gl=document.createElement('canvas').getContext('webgl2'), program=gl.createProgram();
      sources.forEach((source,i)=>{const sh=gl.createShader(i?gl.FRAGMENT_SHADER:gl.VERTEX_SHADER);gl.shaderSource(sh,source);gl.compileShader(sh);if(!gl.getShaderParameter(sh,gl.COMPILE_STATUS))throw Error(gl.getShaderInfoLog(sh));gl.attachShader(program,sh);});
      gl.linkProgram(program);if(!gl.getProgramParameter(program,gl.LINK_STATUS))throw Error(gl.getProgramInfoLog(program));
    }''',shaders)
    page.evaluate("() => Module.callMain(['--capture','/persist/renderer-smoke.cap','4'])")
    # This is the stale SDL focus event reported after launch on Android.
    page.evaluate("() => window.dispatchEvent(new Event('blur'))")
    page.wait_for_function("() => Module.FS.analyzePath('/persist/renderer-smoke.cap').exists || document.querySelector('#error').open",timeout=30000)
    assert page.evaluate("() => Module.FS.analyzePath('/persist/renderer-smoke.cap').exists"),page.evaluate('RE1Diag.report()')+str(errors)
    assert page.evaluate("() => new TextDecoder().decode(Module.FS.readFile('/persist/renderer-smoke.cap').subarray(0,15))").startswith('RE1CAP 640 480')
    page.wait_for_function('() => window.testExited === true')
    page.wait_for_function('() => !window.re1SyncBusy')
    page.reload()
    page.evaluate("""() => { window.AndroidLauncher={ready:()=>true,status:()=>"Synthetic storage fixture"};
      window.re1Ready=()=>{window.testReady=true;}; window.re1Exit=()=>{window.testExited=true;}; }""")
    page.wait_for_function('() => !document.getElementById("launch").disabled')
    page.locator('#launch').click()
    page.wait_for_function('() => window.testReady === true',timeout=90000)
    assert page.evaluate("() => Array.from(Module.FS.readFile('/persist/SAVE/test.dat'))")==[0,128,255]
    assert not errors,errors
    print('PASS: launcher settings, drag layout, local persistence, wasm initialization, binary range reads/seek/EOF, save flush/restore across reload, GLES shader compilation/link, actual engine SDL/GL/audio initialization, first game ticks with synthetic boot textures after SDL blur, and backbuffer capture',result)
    page.close()
    controls=browser.new_page(viewport={'width':960,'height':540},has_touch=True)
    controls.goto(f'http://127.0.0.1:{server.server_port}/index.html')
    controls.locator('#launcher-settings').click()
    controls.locator('#aspect').select_option('16:9')
    controls.evaluate('''()=>{
      window.loadingCalls=[];window.logCalls=0;
      window.AndroidLauncher={ready:()=>false,status:()=>"Controls test fixture",
        showLoadingFilenames:value=>window.loadingCalls.push(value),setReport:value=>window.nativeReport=value,openLogs:()=>window.logCalls++};
    }''')
    controls.locator('#show-loading-files').uncheck()
    assert controls.evaluate('window.loadingCalls')==[False]
    controls.locator('#open-logs').click();assert controls.evaluate('window.logCalls')==1
    assert 'Recent frame gap:' in controls.evaluate('window.nativeReport')
    controls.locator('#movement').select_option('joystick');controls.locator('#show-fps').check()
    controls.locator('#touch-modes summary').click()
    controls.locator('#touch-mode-run').select_option('toggle')
    controls.locator('#touch-mode-aim').select_option('toggle')
    controls.locator('#smooth-frames').check()
    assert controls.locator('#menu-arrows').is_checked()
    controls.locator('#menu-arrows').uncheck()
    controls.locator('#close-settings').click();controls.reload()
    assert controls.evaluate('RE1Input.settings.movement')=='joystick'
    assert controls.evaluate('RE1Input.settings.showFPS') is True
    assert controls.locator('#aspect').input_value()=='16:9'
    assert controls.locator('#show-loading-files').is_checked() is False
    assert controls.locator('#menu-arrows').is_checked() is False
    assert controls.locator('#touch-mode-run').input_value()=='toggle'
    assert controls.locator('#smooth-frames').is_checked()
    controls.locator('#launcher-settings').click();controls.locator('#menu-arrows').check();controls.locator('#close-settings').click()
    controls.evaluate('''()=>{
      const frame=document.querySelector('#game-frame'),canvas=document.querySelector('#canvas');
      const near=(a,b)=>Math.abs(a-b)<1;
      let f=frame.getBoundingClientRect(),c=canvas.getBoundingClientRect();
      if(!near(f.width/f.height,16/9)||!near(c.width/c.height,4/3)||!near(c.height,f.height))throw Error('Menu fit changed proportions');
      RE1Presentation.frame(1,220,1);c=canvas.getBoundingClientRect();
      if(!near(c.width,f.width)||!near(c.width/c.height,4/3)||!near(c.bottom,f.bottom))throw Error('Gameplay crop/edge failed');
      RE1Presentation.frame(0,120,2);c=canvas.getBoundingClientRect();
      if(!near(c.height,f.height)||!near(c.width/c.height,4/3)||!near(c.left-f.left,(f.width-c.width)/2))throw Error('Cinematic not preserved in full');
      RE1Presentation.frame(2,-900,2);c=canvas.getBoundingClientRect();
      if(!near(c.width,f.width)||!near(c.top-f.top,-f.height/6))throw Error('Door not centered in widescreen');
      RE1Presentation.frame(1,105,3);const previousTop=canvas.getBoundingClientRect().top;
      RE1Presentation.frame(3,-900,3);c=canvas.getBoundingClientRect();
      if(!near(c.width,f.width)||!near(c.top,previousTop)||!near(RE1Presentation.croppedRows,60))throw Error('Prompt moved the camera');
      const shift=RE1Presentation.promptOffset(-900,3),scale=c.height/240;
      for(const y of [181,197,213,225]){
        const top=c.top+(y-shift)*scale;
        if(top<f.top-1||top+14*scale>f.bottom+1)throw Error('Reflowed prompt text clipped');
      }
      const held=c.top;RE1Presentation.frame(4,900,100);c=canvas.getBoundingClientRect();
      if(!near(c.top,held)||RE1Presentation.mode!==3)throw Error('Interaction fade changed framing');
      RE1Presentation.frame(6,120,3);c=canvas.getBoundingClientRect();
      if(!near(c.width,f.width)||!near(c.height,f.height)||!near(c.top,f.top))throw Error('Wide item box not complete');
      RE1Presentation.frame(7,120,3);c=canvas.getBoundingClientRect();
      if(!near(c.width,f.width)||!near(c.height,f.height))throw Error('Wide map/files not complete');
      RE1Presentation.frame(3,-900,3);c=canvas.getBoundingClientRect();
      if(!near(c.top,held))throw Error('Item-box return lost held world framing');
      // Scripted actors can move off screen or keep an old player position.
      // Neither their position nor successive dialogue/camera changes may pan
      // or resize a complete cinematic frame.
      for(const [y,camera] of [[-200,2],[600,3],[120,4]]){
        RE1Presentation.frame(0,y,camera);c=canvas.getBoundingClientRect();
        if(!near(c.top,f.top)||!near(c.height,f.height)||!near(c.width/c.height,4/3))throw Error('Cutscene frame moved');
      }
      RE1Presentation.frame(1,20,4);c=canvas.getBoundingClientRect();
      if(!near(c.top,f.top)||!near(c.width,f.width))throw Error('Control return reused stale pan');
      RE1Presentation.frame(0,120,2);c=canvas.getBoundingClientRect();
      if(!near(c.height,f.height)||!near(c.width/c.height,4/3))throw Error('Menu not restored');
    }''')
    controls.locator('#launcher-settings').click();controls.locator('#edit-layout').click()
    assert controls.locator('#joystick').is_visible()
    assert controls.locator('#touch-up').is_hidden()
    box=controls.locator('#joystick').bounding_box()
    controls.locator('#layout-size').fill('130');controls.locator('#layout-size').dispatch_event('input')
    assert abs(controls.locator('#joystick').bounding_box()['width']-box['width']*1.3)<1
    controls.locator('#layout-control').select_option('action')
    controls.locator('#layout-size').fill('150');controls.locator('#layout-size').dispatch_event('input')
    assert abs(controls.locator('#touch-action').bounding_box()['width']-93)<1
    assert abs(controls.locator('#touch-aim').bounding_box()['width']-62)<1
    controls.locator('#layout-control').select_option('up')
    assert controls.locator('#joystick').is_hidden();assert controls.locator('#touch-up').is_visible()
    controls.locator('#layout-size').fill('80');controls.locator('#layout-size').dispatch_event('input')
    assert abs(controls.locator('#touch-up').bounding_box()['width']-49.6)<1
    controls.locator('#layout-control').select_option('joystick')
    assert controls.locator('#layout-size').input_value()=='130'
    previous=controls.evaluate('RE1Input.settings.positions.joystick || null')
    controls.locator('#joystick').click(position={'x':40,'y':50})
    assert controls.evaluate('RE1Input.settings.positions.joystick || null')==previous
    box=controls.locator('#joystick').bounding_box()
    controls.mouse.move(box['x']+box['width']/2,box['y']+box['height']/2);controls.mouse.down();controls.mouse.move(200,310);controls.mouse.up()
    assert controls.evaluate('RE1Input.settings.positions.joystick[0]')>15
    controls.locator('#save-layout').click();controls.locator('#close-settings').click()
    controls.reload()
    assert controls.evaluate('RE1Input.settings.sizes')=={'joystick':1.3,'action':1.5,'up':.8}
    assert controls.locator('#menu-arrows').is_checked()
    # Show the control surface outside edit mode to test real pointer events.
    controls.evaluate("re1TouchContext(false);document.querySelector('#touch').hidden=false")
    controls.locator('#touch-run').click()
    assert controls.evaluate('RE1Input.poll([])')==512
    assert controls.evaluate('RE1Input.poll([])')==512
    assert controls.locator('#touch-run').get_attribute('aria-pressed')=='true'
    controls.locator('#touch-run').click()
    assert controls.evaluate('RE1Input.poll([])')==0
    controls.locator('#touch-run').click()
    controls.evaluate("re1TouchContext(true);document.querySelector('#touch').hidden=false")
    assert controls.evaluate('RE1Input.poll([])')==0
    controls.evaluate("re1TouchContext(false);document.querySelector('#touch').hidden=false")
    # Remaining multi-touch regression below exercises the original hold mode.
    controls.evaluate("()=>{RE1Input.settings.toggles={};}")
    box=controls.locator('#joystick').bounding_box();x=box['x']+box['width']/2;y=box['y']+box['height']/2
    touch=controls.context.new_cdp_session(controls)
    finger={'id':1,'x':x,'y':y}
    touch.send('Input.dispatchTouchEvent',{'type':'touchStart','touchPoints':[finger]})
    finger['y']=y-box['height']*.35
    touch.send('Input.dispatchTouchEvent',{'type':'touchMove','touchPoints':[finger]})
    assert controls.evaluate('RE1Input.mask([])')==1
    aim=controls.locator('#touch-aim').bounding_box()
    second={'id':2,'x':aim['x']+aim['width']/2,'y':aim['y']+aim['height']/2}
    touch.send('Input.dispatchTouchEvent',{'type':'touchStart','touchPoints':[finger,second]})
    assert controls.evaluate('RE1Input.poll([])')==1|1024
    touch.send('Input.dispatchTouchEvent',{'type':'touchEnd','touchPoints':[finger]})
    assert controls.evaluate('RE1Input.poll([])')==1024
    touch.send('Input.dispatchTouchEvent',{'type':'touchEnd','touchPoints':[]})
    assert controls.evaluate('RE1Input.poll([])')==0
    # Switching to menu arrows releases a finger that was still moving the
    # joystick, so it cannot keep scrolling inventory after the stick is hidden.
    touch.send('Input.dispatchTouchEvent',{'type':'touchStart','touchPoints':[finger]})
    assert controls.evaluate('RE1Input.mask([])')==1
    controls.evaluate("re1TouchContext(true);document.querySelector('#touch').hidden=false")
    assert controls.locator('#joystick').is_hidden();assert controls.locator('#touch-up').is_visible()
    assert controls.evaluate('RE1Input.poll([])')==0
    touch.send('Input.dispatchTouchEvent',{'type':'touchEnd','touchPoints':[]})
    controls.evaluate("re1TouchContext(false);document.querySelector('#touch').hidden=false")
    assert controls.locator('#joystick').is_visible();assert controls.locator('#touch-up').is_hidden()
    controls.evaluate('RE1Input.clear()')
    assert controls.evaluate("getComputedStyle(document.querySelector('#touch-action')).webkitTapHighlightColor")=='rgba(0, 0, 0, 0)'
    # Undo the synthetic touch overlay before using the launcher again. Normal
    # launcher paint keeps it hidden until launch or layout editing.
    controls.evaluate("document.querySelector('#touch').hidden=true")
    controls.locator('#launcher-settings').click();controls.locator('#movement').select_option('arrows')
    controls.locator('#edit-layout').click();assert controls.locator('#joystick').is_hidden();assert controls.locator('#touch-up').is_visible()
    controls.close()
    print('PASS: persisted movement/menu arrows/individual sizes/FPS/aspect/loading; editor resize and both movement layouts; menu switch clears held joystick; 16:9 and complete cutscenes; multi-touch/release')
    for mode in ['3','2']:
        test=browser.new_page(viewport={'width':960,'height':480},has_touch=True)
        failures=[];test.on('pageerror',lambda error:failures.append(str(error)))
        test.goto(f'http://127.0.0.1:{server.server_port}/index.html')
        test.get_by_text('Controls & settings',exact=True).first.click()
        test.locator('#renderer').select_option(mode)
        test.locator('#aspect').select_option('16:9')
        test.locator('#show-fps').check()
        test.locator('#close-settings').click()
        test.locator('#graphics-test').click()
        test.wait_for_function('() => RE1Diag.frames>=3',timeout=90000)
        assert test.evaluate('GL.currentContext.version')==(2 if mode=='3' else 1)
        assert test.evaluate("Module['SDL2'].audioContext.state")=='running'
        test.evaluate('''()=>{
          const f=document.querySelector('#game-frame').getBoundingClientRect(),c=document.querySelector('#canvas').getBoundingClientRect();
          if(Math.abs(c.width/c.height-4/3)>.01||Math.abs(c.height-f.height)>1)throw Error('Engine init broke UI aspect');
        }''')
        test.wait_for_function("() => /^FPS: [1-9][0-9]*$/.test(document.querySelector('#fps-counter').textContent)")
        assert test.locator('#fps-counter').is_visible()
        test.evaluate('''() => {
          window.turnPresses=0;const turn=Module._re1_web_quick_turn;
          Module._re1_web_quick_turn=()=>{window.turnPresses++;if(turn()!==0)throw Error('Quick turn allowed in graphics/menu mode');};
        }''')
        test.locator('#touch-quickturn').click();assert test.evaluate('window.turnPresses')==1
        test.locator('#gear').click()
        assert test.locator('#fps-counter').inner_text()=='FPS: paused'
        test.wait_for_timeout(150)
        stopped=test.evaluate('RE1Diag.frames')
        test.wait_for_timeout(150)
        assert test.evaluate('RE1Diag.frames')==stopped
        # Cover/inner-screen/window sizes must retain the same live WASM/GL
        # session and unsaved in-memory data. This exercises the web half of
        # fold handling; Android Activity retention is checked in the APK.
        test.evaluate('''() => {
          window.foldSession={module:Module,gl:GL.currentContext.GLctx,
            preferences:JSON.stringify(RE1Input.settings)};
          Module.FS.writeFile('/fold-session-marker',new Uint8Array([17,128,255]));
        }''')
        for width,height in [(1100,850),(420,840),(740,540),(960,480)]:
            test.set_viewport_size({'width':width,'height':height})
            test.evaluate('re1DisplayChanged()')
            test.evaluate('''() => {
              const frame=document.querySelector('#game-frame').getBoundingClientRect();
              if(Module!==foldSession.module||GL.currentContext.GLctx!==foldSession.gl)throw Error('Resize replaced the engine/GL context');
              if(Module.FS.readFile('/fold-session-marker').join(',')!=='17,128,255')throw Error('Resize lost in-memory state');
              if(JSON.stringify(RE1Input.settings)!==foldSession.preferences)throw Error('Resize changed control preferences');
              if(!document.querySelector('#settings').open||!RE1Input.paused)throw Error('Resize closed or unpaused settings');
              if(!(frame.width>0&&frame.height>0&&frame.left>=-1&&frame.top>=-1&&frame.right<=innerWidth+1&&frame.bottom<=innerHeight+1))throw Error('Viewport no longer fits display');
            }''')
            assert test.evaluate('RE1Diag.frames')==stopped
        test.evaluate('re1Lifecycle(true);re1DisplayChanged()')
        assert test.evaluate('RE1Input.paused')
        test.evaluate('re1Lifecycle(false);re1DisplayChanged()')
        assert test.evaluate('RE1Input.paused'), 'Settings must stay paused after display resume'
        # A browser/SDL blur must not latch the game paused after Resume.
        test.evaluate("() => window.dispatchEvent(new Event('blur'))")
        test.locator('#close-settings').click()
        test.wait_for_function('n => RE1Diag.frames>=n+2',arg=stopped)
        test.evaluate('''() => {
          RE1Input.press(991,'aim');
          if(RE1Input.poll([])!==1024)throw Error('Held-input setup failed');
          re1Native(1,.8,0,true);re1DisplayChanged();
          if(RE1Input.poll([])!==0||RE1Input.pointers.size)throw Error('Display change left input stuck');
          if(RE1Input.paused||Module!==foldSession.module)throw Error('Active display change interrupted session');
        }''')
        test.set_viewport_size({'width':1100,'height':850})
        test.wait_for_function('n => RE1Diag.frames>=n+3',arg=stopped)
        test.set_viewport_size({'width':960,'height':480})
        print('PASS: GLES '+mode+' display resizing retains WASM/GL/in-memory state, preferences and pause state; held input cleared')
        test.wait_for_function('() => document.querySelector("#error").open',timeout=30000)
        assert test.locator('#error h2').inner_text()=='Graphics check passed'
        assert test.locator('#fps-counter').is_hidden()
        assert test.evaluate('RE1Diag.graphicsPassed') is True
        colors=test.evaluate("""() => {
          const bytes=Module.FS.readFile('/persist/graphics-check.cap');
          let offset=0;while(bytes[offset++]!==10){};
          const seen=new Set();for(let n=offset;n<bytes.length;n+=4)seen.add(bytes[n]+','+bytes[n+1]+','+bytes[n+2]);
          return [...seen].sort();
        }""")
        assert colors==['0,0,255','0,255,0','255,0,0','255,255,0'],colors
        assert not failures,failures
        print('PASS: actual OpenGL ES '+mode+' textured render/readback, live audio context, pause/resume after SDL blur, clean shutdown')
        test.close()
    # Android uses a synchronous native chunk bridge, with no movie XHRs.
    # Exact reported capcom.avi size/tail, all binary bytes, and failed/short
    # provider replies are exercised through the actual wasm filesystem.
    native=browser.new_page(viewport={'width':960,'height':480})
    failures=[];native.on('pageerror',lambda error:failures.append(str(error)))
    data_requests=[]
    native.route('**/data/**',lambda route:(data_requests.append(route.request.url),route.abort()))
    native.goto(f'http://127.0.0.1:{server.server_port}/index.html')
    FILES['Data/disconnected.bin']=PAYLOAD;FILES['Data/short.bin']=PAYLOAD
    native.evaluate('''encoded => {
      const files=Object.fromEntries(Object.entries(encoded).map(([path,data])=>[path,atob(data)]));
      window.nativeReads=[];window.nativeResets=0;
      window.AndroidLauncher={ready:()=>true,status:()=>"Native storage fixture",
        returnToLauncher:()=>{window.nativeResets++;},
        readFileChunk:(path,start,length)=>{
          window.nativeReads.push({path,start,length});
          if(path==='Data/disconnected.bin')return 'error:IOException: Provider disconnected';
          if(path==='Data/short.bin')return 'ok:'+btoa('short');
          return 'ok:'+btoa(files[path].slice(start,start+length));
        }};
      window.re1Ready=()=>{window.testReady=true;};
    }''',{path:base64.b64encode(data).decode() for path,data in FILES.items()})
    try:
        native.wait_for_function('() => !document.getElementById("launch").disabled')
        assert not native.locator('#error').evaluate('(e)=>e.open'),native.locator('#error-text').inner_text()
        native.locator('#launch').click()
        native.wait_for_function('() => window.testReady === true',timeout=90000)
        native.evaluate('''() => {
          const fs=Module.FS, movie=fs.readFile('/game/USA/Movie/capcom.avi');
          if(movie.length!==1535452)throw Error('Wrong movie size');
          for(let n=0;n<movie.length;n++)if(movie[n]!==((n*37+11)&255))throw Error('Wrong movie byte '+n);
          const tail=new Uint8Array(224732), stream=fs.open('/game/USA/Movie/capcom.avi','r');
          if(fs.read(stream,tail,0,tail.length,1310720)!==224732)throw Error('Wrong final chunk');
          if(fs.read(stream,tail,0,1,1535452)!==0)throw Error('Wrong movie EOF');
          fs.close(stream);
          if(!window.nativeReads.some(r=>r.path==='Movie/capcom.avi'&&r.start===1310720&&r.length===224732))throw Error('Reported range was not read');
          for(const path of ['disconnected','short']) {
            try {fs.readFile('/game/USA/Data/'+path+'.bin');throw Error('Read failure was ignored');}
            catch(error){if(error.errno!==29)throw error;}
          }
        }''')
        native.wait_for_function('() => document.querySelector("#error").open')
        assert 'disconnected.bin' in native.locator('#error-text').inner_text()
        native.evaluate('() => window.re1Exit(1)')
        assert 'disconnected.bin' in native.locator('#error-text').inner_text()
        native.locator('#reload').click()
        assert native.evaluate('window.nativeResets')==1
        assert not failures,failures
        assert not data_requests,data_requests
        print('PASS: native file bridge reads every movie byte including reported partial tail without XHR; provider errors become EIO with a specific message; Back to launcher calls native recovery after failure')
    finally:
        del FILES['Data/disconnected.bin'];del FILES['Data/short.bin'];native.close()
    # Reproduce the original sync-XHR NetworkError. It must not escape into
    # wasm as a DOM exception, and browser recovery must still navigate.
    network=browser.new_page(viewport={'width':960,'height':480})
    failures=[];network.on('pageerror',lambda error:failures.append(str(error)))
    network.goto(f'http://127.0.0.1:{server.server_port}/index.html')
    network.evaluate('''() => {
      window.AndroidLauncher={ready:()=>true,status:()=>"HTTP fixture"};
      window.re1Ready=()=>{window.testReady=true;};
    }''')
    network.wait_for_function('() => !document.getElementById("launch").disabled')
    network.locator('#launch').click()
    network.wait_for_function('() => window.testReady === true',timeout=90000)
    network.route('**/data/Movie/**',lambda route:route.abort())
    network.evaluate('''() => {
      try {Module.FS.readFile('/game/USA/Movie/capcom.avi');throw Error('Read failure was ignored');}
      catch(error){if(error.errno!==29)throw error;}
    }''')
    network.wait_for_function('() => document.querySelector("#error").open')
    assert 'capcom.avi' in network.locator('#error-text').inner_text()
    network.locator('#reload').click()
    network.wait_for_function('() => !document.querySelector("#error").open && !window.testReady')
    assert not failures,failures
    print('PASS: sync-XHR NetworkError becomes a normal file I/O error; browser fallback returns to launcher')
    network.close()
    # Save & restart follows the engine's normal save/flush/exit path before
    # asking Android to replace the view, even while settings pause the game.
    restart=browser.new_page(viewport={'width':960,'height':480})
    restart.goto(f'http://127.0.0.1:{server.server_port}/index.html')
    restart.evaluate('''() => {
      window.nativeResets=0;
      window.AndroidLauncher={ready:()=>false,status:()=>"Shutdown fixture",returnToLauncher:()=>{window.nativeResets++;}};
    }''')
    restart.locator('#graphics-test').click()
    restart.wait_for_function('() => RE1Diag.frames>=3',timeout=90000)
    # Pause before Playwright's click auto-wait can race the 90-frame test exit
    # on a heavily loaded software-rendering worker.
    restart.evaluate('re1OpenSettings()');restart.locator('#restart-launcher').click()
    restart.wait_for_function('() => window.nativeResets===1',timeout=10000)
    assert not restart.evaluate('window.re1SyncBusy')
    print('PASS: normal return saves/exits the engine and calls native recovery')
    restart.close()
    # A corrupt provider response must produce an actionable startup error.
    bad=browser.new_page(viewport={'width':960,'height':480})
    bad.goto(f'http://127.0.0.1:{server.server_port}/index.html')
    bad.evaluate("() => {window.AndroidLauncher={ready:()=>true,status:()=> 'Fixture'};}")
    bad.wait_for_function('() => !document.getElementById("launch").disabled')
    original=FILES['Data/fontus.tim'];FILES['Data/fontus.tim']=b'bad-data'
    try:
        bad.locator('#launch').click()
        bad.wait_for_function('() => document.querySelector("#error").open',timeout=90000)
        assert 'fontus.tim' in bad.locator('#error-text').inner_text()
        print('PASS: corrupt boot data produces a visible file-specific error')
    finally:FILES['Data/fontus.tim']=original
    bad.close()
    large=browser.new_page(viewport={'width':960,'height':480})
    large.goto(f'http://127.0.0.1:{server.server_port}/index.html')
    large.evaluate("() => {window.AndroidLauncher={ready:()=>true,status:()=> 'Fixture'};}")
    large.wait_for_function('() => !document.getElementById("launch").disabled')
    old=FILES['Data/item_all.pix'];FILES['Data/item_all.pix']=bytes(91201)
    try:
        large.locator('#launch').click()
        large.wait_for_function('() => document.querySelector("#error").open',timeout=90000)
        assert 'item_all.pix' in large.locator('#error-text').inner_text()
        print('PASS: oversized boot item sheet is rejected before reaching a fixed engine buffer')
    finally:FILES['Data/item_all.pix']=old
    large.close()
    browser.close()
server.shutdown()
