"""Run the asset-free Android GL pixel regression in GLES 2 and GLES 3.

Build build/render-batch.{js,wasm} first; see docs/ANDROID.md.
"""
import argparse
import hashlib
import http.server
import json
import pathlib
import threading
import urllib.parse
from playwright.sync_api import sync_playwright

ROOT=pathlib.Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser()
parser.add_argument('--stem',default='render-batch')
parser.add_argument('--output',type=pathlib.Path)
parser.add_argument('--interpolation',action='store_true')
parser.add_argument('--hd',action='store_true')
parser.add_argument('--menu',action='store_true')
args=parser.parse_args()
assert args.stem.replace('-','').isalnum()

class Server(http.server.SimpleHTTPRequestHandler):
    def __init__(self,*a,**kw):super().__init__(*a,directory=str(ROOT/'build'),**kw)
    def log_message(self,*a):pass
    def do_GET(self):
        path=urllib.parse.urlsplit(self.path).path
        if path=='/':
            payload=f'<canvas id="canvas" width="64" height="64"></canvas><script src="/start.js"></script><script src="/{args.stem}.js"></script>'.encode()
            kind='text/html'
        elif path=='/start.js':
            payload=b'''var Module={canvas:document.getElementById('canvas'),
              print:value=>(window.output??=[]).push(String(value)),
              printErr:value=>(window.output??=[]).push(String(value)),
              onRuntimeInitialized:()=>setTimeout(()=>Module.callMain([new URL(location.href).searchParams.get('gles')]),0),
              onExit:status=>{window.exitStatus=status;window.done=true;},
              onAbort:reason=>{window.abortReason=String(reason);window.done=true;}};'''
            kind='application/javascript'
        else:return super().do_GET()
        self.send_response(200);self.send_header('Content-Type',kind);self.end_headers();self.wfile.write(payload)
    def end_headers(self):
        self.send_header('Content-Security-Policy',"default-src 'self'; script-src 'self' 'wasm-unsafe-eval'; object-src 'none'")
        super().end_headers()

server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Server)
threading.Thread(target=server.serve_forever,daemon=True).start()
results={}
try:
    with sync_playwright() as p:
        browser=p.chromium.launch(executable_path='/usr/bin/chromium',headless=True,args=['--no-sandbox','--enable-unsafe-swiftshader'])
        for mode in ['2','3']:
            page=browser.new_page()
            errors=[]
            page.on('pageerror',lambda error:errors.append(str(error)))
            page.goto(f'http://127.0.0.1:{server.server_port}/?gles={mode}')
            try:page.wait_for_function('()=>window.done===true',timeout=60000)
            except Exception as error:
                raise AssertionError({'gles':mode,'errors':errors,'runtime':page.evaluate('({output:window.output,abort:window.abortReason})')}) from error
            assert not errors,errors
            assert page.evaluate('window.exitStatus')==0,page.evaluate('({output:window.output,abort:window.abortReason})')
            captures=page.evaluate('''()=>Object.fromEntries(Module.FS.readdir('/').filter(n=>n.endsWith('.rgba')).map(n=>[n,Array.from(Module.FS.readFile('/'+n))]))''')
            assert len(captures)==(0 if args.interpolation or args.hd or args.menu else 17),captures.keys()
            results[mode]={name:hashlib.sha256(bytes(data)).hexdigest() for name,data in captures.items()}
            print('PASS GLES '+mode+(': complete wide menu, centered model, proportional glyphs and drawable restoration' if args.menu else ': asynchronous HD pixels, resident/shared texture reuse, original UV dimensions, corrupt-image fallback and streaming-video updates' if args.hd else ': real generated midpoint/endpoint pixels; unchanged tick count; mutation/camera/budget/readback fallback; GLES3 invisible-effect samples skipped' if args.interpolation else ': alpha order; texture update/delete; retained samplers; capacity flush; depth; zero/faint/additive transparency'))
            page.close()
        browser.close()
finally:server.shutdown()
if args.output:args.output.write_text(json.dumps(results,indent=2)+'\n')
