"""Exercise the actual launcher controller bridge without commercial game data.

The controller packets and browser Gamepad API are simulated; physical Android
dispatch and a real Galileo still need device testing.
"""
import functools
import http.server
import pathlib
import threading

from playwright.sync_api import sync_playwright

ROOT = pathlib.Path(__file__).resolve().parents[2]
WEB = ROOT / 'android/app/src/main/assets/web'


class Handler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, *_):
        pass


server = http.server.ThreadingHTTPServer(
    ('127.0.0.1', 0), functools.partial(Handler, directory=str(WEB)))
threading.Thread(target=server.serve_forever, daemon=True).start()
with sync_playwright() as p:
    browser = p.chromium.launch(executable_path='/usr/bin/chromium',
                                args=['--no-sandbox'], headless=True)
    page = browser.new_page()
    errors = []
    page.on('pageerror', lambda error: errors.append(str(error)))
    page.goto(f'http://127.0.0.1:{server.server_port}/index.html')
    page.evaluate('''() => {
      window.testPad={connected:true,mapping:'standard',axes:[1,0],
        buttons:Array.from({length:16},()=>({pressed:false}))};
      Object.defineProperty(navigator,'getGamepads',{value:()=>[window.testPad]});
    }''')
    assert page.evaluate('RE1Input.poll()') == 8
    # Focus/pause cleanup with no native controller must not claim the browser
    # pad. This used to permanently replace getGamepads with an empty snapshot.
    page.evaluate('re1Native(0,0,0,false)')
    assert page.evaluate('RE1Input.poll()') == 8, 'Empty native update swallowed browser controller'
    page.evaluate('window.testPad.axes=[0,0]')
    # Down and up can both arrive between two 30 Hz ticks. Confirm still gets
    # exactly one tick, then releases; this also applies to short D-pad taps.
    page.evaluate('re1Native(1,0,0,true);re1Native(0,0,0,true)')
    assert page.evaluate('RE1Input.poll()') == 256, 'Short native button tap lost'
    assert page.evaluate('RE1Input.poll()') == 0
    # Native state owns the pad while connected: do not mix a stale WebView
    # duplicate with the current native release.
    page.evaluate('window.testPad.buttons[0].pressed=true')
    assert page.evaluate('RE1Input.poll()') == 0
    page.evaluate('re1Native(0,0,0,false)')
    assert page.evaluate('RE1Input.poll()') == 256
    page.evaluate('window.testPad.buttons[0].pressed=false')
    # Menus/settings, remapping, focus changes and reconnects can all clear the
    # JS snapshot. The next real packet must restore input immediately.
    for transition in [
        're1TouchContext(false)',
        're1OpenSettings();document.querySelector("#close-settings").click()',
        'window.dispatchEvent(new Event("blur"))',
        'window.dispatchEvent(new Event("gamepaddisconnected"))',
        're1Lifecycle(true);re1Lifecycle(false)',
    ]:
        page.evaluate('re1Native(1<<2,-1,0,true)')
        assert page.evaluate('RE1Input.poll()') == 1024 | 4
        page.evaluate(transition)
        assert page.evaluate('RE1Input.poll()') == 0, transition
        page.evaluate('re1Native(1<<2,-1,0,true)')
        assert page.evaluate('RE1Input.poll()') == 1024 | 4, transition
        page.evaluate('re1Native(0,0,0,false)')
        assert page.evaluate('RE1Input.poll()') == 0
    page.evaluate('re1OpenSettings();re1Native(1,0,0,true);re1Native(0,0,0,true)')
    assert page.evaluate('RE1Input.poll()') == 0
    page.locator('#close-settings').click()
    assert page.evaluate('RE1Input.poll()') == 0
    page.evaluate('RE1Input.settings.mapping[0]=1024;re1Native(1,0,0,true)')
    assert page.evaluate('RE1Input.poll()') == 1024
    page.evaluate('re1Native(0,0,0,true)')
    assert page.evaluate('RE1Input.poll()') == 0
    page.evaluate('()=>{ready=true;window.testTurns=0;Module._re1_web_quick_turn=()=>{testTurns++;return 1;};}')
    page.evaluate('re1Native(1<<11,0,0,true);re1Native(0,0,0,true)')
    page.evaluate('RE1Input.poll();RE1Input.poll()')
    assert page.evaluate('testTurns')==1
    page.evaluate('re1Native(1<<11,0,0,true);RE1Input.poll();RE1Input.poll()')
    assert page.evaluate('testTurns')==2
    page.evaluate('re1Native(0,0,0,true);RE1Input.poll();ready=false')
    assert page.locator('#controller-quick-turn').input_value()=='11'
    assert not errors, errors
    browser.close()
server.shutdown()
print('PASS: actual launcher native/browser ownership, buffered native taps, remapping, menu/focus/lifecycle recovery and reconnects')
