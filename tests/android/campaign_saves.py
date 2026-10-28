"""Real IndexedDB isolation for campaign/base save import and export. Synthetic slots only."""
import functools,http.server,pathlib,threading
from playwright.sync_api import sync_playwright
WEB=pathlib.Path(__file__).resolve().parents[2]/'android/app/src/main/assets/web'
class Handler(http.server.SimpleHTTPRequestHandler):
    def log_message(self,*_):pass
server=http.server.ThreadingHTTPServer(('127.0.0.1',0),functools.partial(Handler,directory=str(WEB)))
threading.Thread(target=server.serve_forever,daemon=True).start()
try:
    with sync_playwright() as p:
        browser=p.chromium.launch(executable_path='/usr/bin/chromium',headless=True,args=['--no-sandbox'])
        page=browser.new_page();page.goto(f'http://127.0.0.1:{server.server_port}/index.html')
        result=page.evaluate('''async()=>{
          let scope='/persist/SAVE';RE1Mods.savesReady=async()=>scope;
          const slot=value=>[{name:'savedat1.dat',contents:new Uint8Array(2048).fill(value)}];
          await RE1SaveData.write(slot(17));
          scope='/persist/CAMPAIGNS/campaign-a/SAVE';await RE1SaveData.write(slot(34));
          const campaign=(await RE1SaveData.read())[0].contents[0];
          scope='/persist/CAMPAIGNS/campaign-b/SAVE';const empty=(await RE1SaveData.read()).length;
          await RE1SaveData.write(slot(51));
          scope='/persist/SAVE';const base=(await RE1SaveData.read())[0].contents[0];
          scope='/persist/CAMPAIGNS/campaign-a/SAVE';const back=(await RE1SaveData.read())[0].contents[0];
          return {campaign,empty,base,back};
        }''')
        assert result=={'campaign':34,'empty':0,'base':17,'back':34},result
        page.reload()
        result=page.evaluate('''async()=>{RE1Mods.savesReady=async()=>'/persist/CAMPAIGNS/campaign-b/SAVE';
          const slots=await RE1SaveData.unzip(RE1SaveData.zip(await RE1SaveData.read()));return [slots.length,slots[0].contents[0]];}''')
        assert result==[1,51],result
        browser.close()
finally:server.shutdown()
print('PASS: real IndexedDB keeps base and two campaigns separate across reload and ZIP export/import')
