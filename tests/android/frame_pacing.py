"""Actual SDL/WebGL scheduling regression, without commercial game data.
Requires the built engine, Playwright and system Chromium.
"""
import http.server
import pathlib
import threading
from playwright.sync_api import sync_playwright

WEB=pathlib.Path(__file__).resolve().parents[2]/'android/app/src/main/assets/web'
class Server(http.server.SimpleHTTPRequestHandler):
    def __init__(self,*args,**kwargs):super().__init__(*args,directory=str(WEB),**kwargs)
    def log_message(self,*args):pass

server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Server)
threading.Thread(target=server.serve_forever,daemon=True).start()
try:
    with sync_playwright() as p:
        browser=p.chromium.launch(executable_path='/usr/bin/chromium',headless=True,args=['--no-sandbox','--enable-unsafe-swiftshader'])
        page=browser.new_page(viewport={'width':960,'height':540})
        errors=[];page.on('pageerror',lambda error:errors.append(str(error)))
        page.add_init_script('''(()=>{
          const raf=requestAnimationFrame.bind(window),timer=setTimeout.bind(window);
          window.frameQA={slow:false,raf:0,swapSleeps:[]};
          window.requestAnimationFrame=callback=>raf(time=>{frameQA.raf++;callback(time);});
          window.setTimeout=(callback,delay,...args)=>{
            if(frameQA.slow&&delay===0){const stack=new Error().stack;if(stack.includes('_emscripten_sleep'))frameQA.swapSleeps.push(stack);}
            return timer(callback,frameQA.slow?Math.max(80,delay):delay,...args);
          };
        })();''')
        page.goto(f'http://127.0.0.1:{server.server_port}/')
        page.locator('#graphics-test').click()
        page.wait_for_function('()=>RE1Diag.frames>=4',timeout=90000)
        start=page.evaluate('()=>{frameQA.slow=true;return {frames:RE1Diag.frames,raf:frameQA.raf};}')
        page.wait_for_timeout(1600)
        end=page.evaluate('()=>({frames:RE1Diag.frames,raf:frameQA.raf,swapSleeps:frameQA.swapSleeps})')
        assert end['frames']>=start['frames']+8,(start,end)
        assert end['raf']>start['raf'],(start,end)
        assert not end['swapSleeps'],end['swapSleeps']
        assert not errors,errors
        print('PASS: actual SDL/GL foreground frames without touch under 80 ms timer delays; display callbacks continue; no SDL swap sleep',{'frames':end['frames']-start['frames'],'displayCallbacks':end['raf']-start['raf']})
        browser.close()
finally:server.shutdown()
