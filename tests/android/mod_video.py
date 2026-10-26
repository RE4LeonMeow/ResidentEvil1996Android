"""Actual HTML H.264/AAC clock, pause/seek/cut/skip checks; synthetic video only."""
import functools
import http.server
import pathlib
import subprocess
import tempfile
import threading
from playwright.sync_api import sync_playwright

ROOT=pathlib.Path(__file__).resolve().parents[2]
WEB=ROOT/'android/app/src/main/assets/web'
with tempfile.TemporaryDirectory(prefix='re1-mod-video-') as temp:
    fixture=pathlib.Path(temp)/'fixture.mp4'
    release_movie=threading.Event()
    subprocess.run(['ffmpeg','-v','error','-f','lavfi','-i','testsrc2=size=640x480:rate=30','-f','lavfi','-i','sine=frequency=440:sample_rate=44100','-t','4','-c:v','libx264','-preset','ultrafast','-pix_fmt','yuv420p','-c:a','aac','-movflags','+faststart',str(fixture)],check=True)
    class Handler(http.server.SimpleHTTPRequestHandler):
        def log_message(self,*_):pass
        def do_GET(self):
            if self.path in ['/fixture.mp4','/slow.mp4']:
                if self.path=='/slow.mp4':release_movie.wait(10)
                data=fixture.read_bytes();self.send_response(200);self.send_header('Content-Type','video/mp4');self.send_header('Content-Length',str(len(data)));self.end_headers();self.wfile.write(data)
            else:super().do_GET()
    server=http.server.ThreadingHTTPServer(('127.0.0.1',0),functools.partial(Handler,directory=str(WEB)))
    threading.Thread(target=server.serve_forever,daemon=True).start()
    try:
        with sync_playwright() as p:
            browser=p.chromium.launch(executable_path='/usr/bin/chromium',headless=True,args=['--no-sandbox','--autoplay-policy=no-user-gesture-required'])
            page=browser.new_page();errors=[];page.on('pageerror',lambda e:errors.append(str(e)))
            page.goto(f'http://127.0.0.1:{server.server_port}/index.html')
            page.evaluate("()=>{RE1Mods.movie=path=>path.endsWith('fixture.mp4')?'/fixture.mp4':path.endsWith('slow.mp4')?'/slow.mp4':null;}")
            assert not page.evaluate("RE1ModVideo.open('/game/USA/Movie/base.avi',0)")
            assert page.evaluate("RE1ModVideo.open('/game/USA/Movie/slow.mp4',0)")
            page.wait_for_timeout(350)
            assert page.evaluate('getComputedStyle(document.querySelector("#mod-video")).opacity')=='0'
            assert page.evaluate('getComputedStyle(document.querySelector("#mod-video-layer")).backgroundColor')=='rgb(0, 0, 0)'
            assert not page.evaluate('document.querySelector("#mod-video").controls')
            release_movie.set()
            page.wait_for_function('document.querySelector("#mod-video").hasAttribute("data-ready")')
            assert page.evaluate('getComputedStyle(document.querySelector("#mod-video")).opacity')=='1'
            page.evaluate('RE1ModVideo.close()')
            assert page.evaluate("RE1ModVideo.open('/game/USA/Movie/fixture.mp4',1200)")
            page.wait_for_function('()=>document.querySelector("#mod-video").currentTime>.25')
            assert page.evaluate('document.querySelector("#mod-video").videoWidth')==640
            page.evaluate('RE1ModVideo.suspend(true)');before=page.evaluate('document.querySelector("#mod-video").currentTime')
            page.wait_for_timeout(400);after=page.evaluate('document.querySelector("#mod-video").currentTime')
            assert abs(after-before)<.08,(before,after)
            page.evaluate('RE1ModVideo.suspend(false)')
            page.wait_for_function('()=>{RE1ModVideo.tick();return !RE1ModVideo.isActive();}')
            assert 1.15<=page.evaluate('document.querySelector("#mod-video").currentTime')<1.6
            assert page.evaluate('RE1ModVideo.takeEnd()') and not page.evaluate('RE1ModVideo.takeEnd()')
            page.evaluate('RE1ModVideo.playFrom(2000)')
            page.wait_for_function('()=>document.querySelector("#mod-video").currentTime>2.15')
            assert page.evaluate('RE1ModVideo.isActive()')
            page.evaluate('RE1ModVideo.stop()');assert not page.evaluate('RE1ModVideo.isActive()')
            page.evaluate('RE1ModVideo.close();RE1ModVideo.close()');assert page.locator('#mod-video').is_hidden()
            assert not page.locator('#mod-video').get_attribute('src')
            assert page.locator('#mod-video-layer').is_hidden()
            # Closing while a disk-backed movie is prepared must not attach
            # the late reply or leave a blob URL alive on the player.
            page.evaluate('''()=>{RE1Mods.movieBlob=()=>new Promise(resolve=>window.finishMovieCopy=resolve);}''')
            assert page.evaluate("RE1ModVideo.open('/game/USA/Movie/fixture.mp4',0)")
            assert not page.locator('#mod-video').get_attribute('src')
            page.evaluate('RE1ModVideo.close();finishMovieCopy(new Blob(["stale"],{type:"video/mp4"}))')
            page.wait_for_timeout(100)
            assert not page.locator('#mod-video').get_attribute('src')
            assert page.locator('#mod-video-layer').is_hidden()
            # stop() retains the file for a later playFrom(), even when the
            # copy completes while the game has stopped the current segment.
            page.evaluate('''async()=>{window.copyFixture=await (await fetch('/fixture.mp4')).blob();
              RE1Mods.movieBlob=()=>new Promise(resolve=>window.finishMovieCopy=resolve);}''')
            assert page.evaluate("RE1ModVideo.open('/game/USA/Movie/fixture.mp4',0)")
            page.evaluate('RE1ModVideo.stop();finishMovieCopy(copyFixture)')
            page.wait_for_function('document.querySelector("#mod-video").readyState>=1')
            assert page.evaluate('document.querySelector("#mod-video").paused')
            page.evaluate('RE1ModVideo.playFrom(1500)')
            page.wait_for_function('document.querySelector("#mod-video").currentTime>1.65')
            assert page.locator('#mod-video').get_attribute('src').startswith('blob:')
            page.evaluate('RE1ModVideo.close()')
            page.evaluate('()=>{RE1Mods.movieBlob=()=>null;}')
            # A decoded-frame event fallback also works on older WebViews.
            page.evaluate('document.querySelector("#mod-video").requestVideoFrameCallback=undefined')
            assert page.evaluate("RE1ModVideo.open('/game/USA/Movie/fixture.mp4',0)")
            page.wait_for_function('document.querySelector("#mod-video").hasAttribute("data-ready")')
            page.evaluate('RE1ModVideo.close()')
            # Rejected playback must release the game rather than stick on a poster.
            page.evaluate('()=>{document.querySelector("#mod-video").play=()=>Promise.reject(new DOMException("Playback blocked","NotAllowedError"));}')
            assert page.evaluate("RE1ModVideo.open('/game/USA/Movie/fixture.mp4',0)")
            page.wait_for_function('!RE1ModVideo.isActive()')
            assert page.evaluate('RE1ModVideo.takeEnd()')
            page.evaluate('RE1ModVideo.close()')
            assert not errors,errors
            browser.close()
    finally:server.shutdown()
print('PASS: real H.264/AAC clock, delayed first-frame cover, old-WebView fallback, blocked playback recovery, pause/resume, segment, seek, skip and repeated close')
