"""Launcher update UX with synthetic GitHub replies and Android bridge.
No GitHub writes, game data or APK installation. Run with Python Playwright.
"""
import functools
import http.server
import json
import pathlib
import os
import re
import threading
from playwright.sync_api import sync_playwright

ROOT = pathlib.Path(__file__).resolve().parents[2]
WEB = ROOT / 'android/app/src/main/assets/web'


class Handler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def end_headers(self):
        self.send_header('Content-Security-Policy', "default-src 'self'; script-src 'self' 'wasm-unsafe-eval'; style-src 'self'; connect-src 'self' blob:; worker-src 'self' blob:; img-src 'self' data:; object-src 'none'; base-uri 'none'; frame-src 'none'")
        super().end_headers()


server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), functools.partial(Handler, directory=str(WEB)))
threading.Thread(target=server.serve_forever, daemon=True).start()
origin = f'http://127.0.0.1:{server.server_port}'
CHECK_URL = re.compile(r'/updates(?:\?|$)')
release = lambda tag: {'state': 'ok', 'release': {'tag_name': tag, 'draft': False, 'prerelease': False}}
with sync_playwright() as p:
    browser = p.chromium.launch(executable_path='/usr/bin/chromium', headless=True, args=['--no-sandbox', '--enable-unsafe-swiftshader'])
    context = browser.new_context(viewport={'width': 960, 'height': 480}, has_touch=True)
    page = context.new_page()
    errors = []
    page.on('pageerror', lambda error: errors.append(str(error)))
    page.add_init_script('''window.testOnline=true;window.testLinks=[];
      window.AndroidLauncher={hasUpdateChecker:()=>true,updateNetworkAvailable:()=>testOnline,
        openExternal:link=>testLinks.push(link),ready:()=>false,status:()=>"Select a folder"};''')
    replies = {'value': release('v1.12'), 'status': 200, 'requests': []}

    def respond(route):
        replies['requests'].append(route.request.url)
        route.fulfill(status=replies['status'], content_type='application/json', body=json.dumps(replies['value']))

    page.route(CHECK_URL, respond)
    page.goto(origin + '/index.html')
    page.wait_for_function('()=>document.querySelector("#updates").dataset.state==="available"')
    assert 'v1.12' in page.locator('#update-status').inner_text()
    assert page.locator('#update-open').is_enabled()
    assert page.locator('dialog[open]').count() == 0
    assert page.locator('#folder').is_enabled()
    page.locator('#update-open').click()
    assert page.evaluate('testLinks') == ['https://github.com/RE4LeonMeow/ResidentEvil1996Android/releases']
    for width, height in [(960, 480), (480, 320), (640, 360), (800, 360), (1280, 576), (1920, 1080)]:
        page.set_viewport_size({'width': width, 'height': height})
        badge = page.locator('#updates').bounding_box()
        card = page.locator('.launch-card').bounding_box()
        assert badge['x'] + badge['width'] <= card['x'], (width, height, badge, card)
        assert page.locator('#launcher').evaluate('(node)=>parseFloat(getComputedStyle(node).paddingTop)') <= 24
        if height <= 576:
            assert card['y'] <= 28, (width, height, card)
        assert 0 <= badge['x'] <= 20 and badge['x'] + badge['width'] <= width
        assert page.evaluate('async()=>{await document.fonts.ready;return [...document.fonts].some(face=>face.family.includes("Compacta Launcher")&&face.status==="loaded")}')
        assert 'Compacta Launcher' in page.locator('.launch-card h2').evaluate('(node)=>getComputedStyle(node).fontFamily')
        if os.environ.get('RE1_UI_CAPTURE_DIR') and width in (640, 960):
            capture=pathlib.Path(os.environ['RE1_UI_CAPTURE_DIR']);capture.mkdir(parents=True,exist_ok=True)
            page.screenshot(path=str(capture/f'launcher-{width}.png'))
        assert card['y'] + card['height'] <= height
    page.set_viewport_size({'width': 960, 'height': 480})
    for value, state in [(release('v1.01'), 'current'), ({'state': 'no-release'}, 'no-release'),
                         ({'state': 'unavailable'}, 'unavailable'), (release('v1.3-beta'), 'unavailable')]:
        replies['value'] = value
        page.locator('#update-status').click()
        page.wait_for_function('(state)=>document.querySelector("#updates").dataset.state===state', arg=state)
        assert replies['requests'][-1].endswith('?force=1')
        assert page.locator('#update-open').is_hidden()
        assert page.locator('#update-open').is_disabled()
        page.evaluate('document.querySelector("#update-open").click()')
        assert len(page.evaluate('testLinks')) == 1
        if state == 'current' and os.environ.get('RE1_UI_CAPTURE_DIR'):
            page.set_viewport_size({'width':1280,'height':576})
            capture=pathlib.Path(os.environ['RE1_UI_CAPTURE_DIR']);capture.mkdir(parents=True,exist_ok=True)
            page.screenshot(path=str(capture/'launcher-current-1280.png'))
            page.set_viewport_size({'width':960,'height':480})
    replies['status'] = 429
    page.locator('#update-status').click()
    page.wait_for_function('()=>document.querySelector("#updates").dataset.state==="unavailable"')
    replies['status'] = 200
    count = len(replies['requests'])
    page.evaluate('testOnline=false;window.dispatchEvent(new Event("offline"))')
    assert page.locator('#update-status').inner_text() == 'Offline mode'
    assert page.locator('#update-open').is_disabled()
    assert page.locator('#update-open').is_hidden()
    page.locator('#update-status').click()
    assert len(replies['requests']) == count
    replies['value'] = release('v1.12')
    page.evaluate('testOnline=true;window.dispatchEvent(new Event("online"))')
    page.wait_for_function('()=>document.querySelector("#updates").dataset.state==="available"')
    page.locator('#launcher-settings').click()
    page.locator('#check-updates').uncheck()
    page.locator('#close-settings').click()
    assert page.locator('#updates').get_attribute('data-state') == 'disabled'
    assert page.locator('#update-open').is_hidden()
    count = len(replies['requests'])
    page.reload()
    assert page.locator('#update-status').inner_text() == 'Update checks off'
    assert len(replies['requests']) == count
    page.locator('#update-status').click()  # Manual checks work after opting out.
    page.wait_for_function('()=>document.querySelector("#updates").dataset.state==="available"')
    page.reload()
    assert page.locator('#update-status').inner_text() == 'Update checks off'
    # Long text dialogs start at their heading, including after a prior scroll.
    page.route('**/plain.txt', lambda route: route.fulfill(content_type='text/plain', body='Credits\n'+'Playtester name\n'*100))
    for attempt in range(2):
        page.locator('#launcher-credits').click()
        page.wait_for_function('()=>document.querySelector("#credits-text").textContent.includes("Playtester name")')
        assert page.locator('#credits').evaluate('(dialog)=>dialog.scrollTop===0&&dialog.contains(document.activeElement)&&document.activeElement.tagName==="H2"')
        page.locator('#credits').evaluate('(dialog)=>dialog.scrollTop=dialog.scrollHeight')
        page.locator('#close-credits').click()
    page.locator('#launcher-donation').click()
    assert page.locator('#donation').evaluate('(dialog)=>dialog.scrollTop===0&&document.activeElement.tagName==="H2"')
    page.locator('#donation-ok').click()
    assert page.locator('#donation').evaluate('(dialog)=>dialog.scrollTop===0&&document.activeElement.tagName==="H2"')
    page.locator('#donation-close').click()
    page.locator('#launcher-saves').click()
    assert page.locator('#saves').evaluate('(dialog)=>dialog.scrollTop===0&&document.activeElement.tagName==="H2"')
    page.locator('#saves-close').click()
    page.locator('#launcher-settings').click()
    page.locator('#game-action-recovery summary').click()
    assert page.locator('#restore-game-actions').is_disabled()
    page.locator('#close-settings').click()
    page.evaluate('''()=>{
      ready=true;window.testRestoreCalls=0;window.testPaused=0;
      Module._re1_web_suspend=paused=>window.testPaused=paused;
      Module._re1_web_settings_save=()=>{};
      Module._re1_web_restore_game_actions=()=>{if(!testPaused)throw Error('Game was not paused');testRestoreCalls++;return 1;};
      RE1Input.settings.positions.aim=[73,42];RE1Input.settings.sizes.aim=1.35;
      RE1Input.settings.toggles.aim=true;RE1Input.settings.mapping[0]=32768;
      window.testTouchSettings=JSON.stringify(RE1Input.settings);
    }''')
    page.locator('#launcher-settings').click()
    assert page.locator('#restore-game-actions').is_enabled()
    page.locator('#restore-game-actions').click()
    assert page.evaluate('testRestoreCalls')==1
    assert page.evaluate('JSON.stringify(RE1Input.settings)===testTouchSettings')
    assert 'Game actions restored' in page.locator('#game-actions-status').inner_text()
    assert 'Save again' in page.locator('#game-actions-status').inner_text()
    page.locator('#close-settings').click()
    page.evaluate('ready=false')
    # A pending check must not block using the launcher or survive game start.
    pending = []
    page.unroute(CHECK_URL)
    page.route(CHECK_URL, lambda route: pending.append(route))
    page.locator('#update-status').click()
    page.wait_for_function('()=>document.querySelector("#updates").dataset.state==="checking"')
    assert page.locator('#folder').is_enabled()
    page.locator('#launcher-settings').click()
    page.locator('#check-updates').check()
    page.locator('#check-updates').uncheck()
    page.locator('#close-settings').click()
    # Old replies must not replace the opt-out/offline state.
    for route in pending:
        try:
            route.fulfill(content_type='application/json', body=json.dumps(release('v9.0')))
        except Exception:
            pass  # The client has already aborted this request.
    page.wait_for_timeout(150)
    assert page.locator('#updates').get_attribute('data-state') == 'disabled'
    pending.clear()
    page.locator('#update-status').click()
    page.evaluate('document.querySelector("#launcher").hidden=true')
    page.wait_for_timeout(100)
    assert page.locator('#updates').is_hidden()
    for route in pending:
        try:
            route.fulfill(content_type='application/json', body=json.dumps(release('v9.0')))
        except Exception:
            pass
    assert not errors, errors
    # The standalone browser has no Android checker and gains no update request.
    plain = context.new_page()
    unexpected = []
    plain.route(CHECK_URL, lambda route: (unexpected.append(route.request.url), route.abort()))
    plain.goto(origin + '/index.html')
    assert plain.locator('#updates').is_hidden()
    assert plain.locator('#check-updates').is_hidden()
    assert not unexpected
    browser.close()
server.shutdown()
print('Update launcher passed: highlighting, safe links, offline, errors, opt-out, retry, cancellation and mobile layout')
