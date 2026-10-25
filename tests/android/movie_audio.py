"""Real WebAudio movie-clock checks with a generated tone, no game data."""
import http.server,pathlib,threading
from playwright.sync_api import sync_playwright
WEB=pathlib.Path(__file__).resolve().parents[2]/'android/app/src/main/assets/web'
class Server(http.server.SimpleHTTPRequestHandler):
    def __init__(self,*args,**kwargs):super().__init__(*args,directory=str(WEB),**kwargs)
    def log_message(self,*args):pass
server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Server)
threading.Thread(target=server.serve_forever,daemon=True).start()
try:
    with sync_playwright() as p:
        browser=p.chromium.launch(executable_path='/usr/bin/chromium',headless=True,args=['--no-sandbox'])
        page=browser.new_page();errors=[];page.on('pageerror',lambda error:errors.append(str(error)))
        page.goto(f'http://127.0.0.1:{server.server_port}/')
        page.evaluate('''()=>{document.querySelector('#launcher-settings').onclick=()=>{
          const context=new AudioContext();Module.SDL2={audioContext:context};
          window.tonePCM=new Int16Array(22050*2*3);
          for(let i=0;i<tonePCM.length;i++)tonePCM[i]=Math.sin(Math.floor(i/2)*2*Math.PI*440/22050)*8192;
          if(!RE1MovieAudio.play(tonePCM,22050,0,0,1))throw Error('Could not schedule tone');
        };}''')
        page.locator('#launcher-settings').click();page.wait_for_function('()=>RE1MovieAudio.position()>22050/4')
        timing=page.evaluate('''()=>{const before=RE1MovieAudio.position(),start=performance.now();
          // Deliberately block JS to model an AVI read/decode stall. The
          // source node and its clock must continue at their original rate.
          while(performance.now()-start<300){}
          return {elapsed:performance.now()-start,advance:(RE1MovieAudio.position()-before)/22050};}''')
        assert abs(timing['advance']-timing['elapsed']/1000)<.1,timing
        page.evaluate('()=>Module.SDL2.audioContext.suspend()');paused=page.evaluate('RE1MovieAudio.position()')
        page.wait_for_timeout(300);assert page.evaluate('RE1MovieAudio.position()')==paused
        page.evaluate('()=>Module.SDL2.audioContext.resume()');page.wait_for_function('(n)=>RE1MovieAudio.position()>n+2205',arg=paused)
        page.evaluate('()=>{window.firstMovieBuffer=RE1MovieAudio.buffer;RE1MovieAudio.play(tonePCM,22050,22050,33075,1);}')
        assert page.evaluate('RE1MovieAudio.buffer===firstMovieBuffer')
        assert 22050<=page.evaluate('RE1MovieAudio.position()')<24255
        page.evaluate('()=>{RE1MovieAudio.stop();return Module.SDL2.audioContext.close();}')
        assert page.evaluate('RE1MovieAudio.position()')==-1
        assert not errors,errors
        print('PASS: actual WebAudio source clock, main-thread stall without skipped samples, suspension/resume, segment seek/cache and cleanup',timing)
        browser.close()
finally:server.shutdown()
