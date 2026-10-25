// Read-only game files are chunk-read from Android storage on demand. Only
// saves/settings live in IDBFS. No full-game copy into WebAssembly memory.
Module['noInitialRun'] = true;
// The launcher owns lifetime; retain IDBFS until WebView is closed/reloaded.
Module['noExitRuntime'] = true;
Module['canvas'] = document.getElementById('canvas');
Module['print'] = text => { console.log(text); globalThis.RE1Diag?.log(text); };
Module['printErr'] = text => { console.error(text); globalThis.RE1Diag?.log(text); };
Module['onExit'] = code => globalThis.re1Exit?.(code);
Module['onAbort'] = reason => globalThis.re1Failure?.(String(reason));
Module['preRun'] = [function () {
  addRunDependency('android-storage');
  (async () => {
    try {
      ENV['RE1_WEB_GLES'] = Module['re1GLES'] === '2' ? '2' : '3';
      ENV['RE1_SKIP_MOVIES'] = Module['re1SkipMovies'] ? '1' : '0';
      ENV['RE1_HD_MODE'] = Module['re1HDActive'] ? '1' : '0';
      // Browser shortcuts are buffered by app.js and enter as one Marni pad.
      // SDL must not also retain keyboard presses across launcher focus changes.
      ENV['RE1_BROWSER_INPUT'] = globalThis.RE1BrowserStorage ? '1' : '0';
      globalThis.RE1Diag?.stage('Reading selected game folder');
      let files = [];
      if (!Module['re1DiagnosticsOnly']) {
        if(globalThis.RE1BrowserStorage)files=globalThis.RE1BrowserStorage.manifest();
        else{
          const response = await fetch('/manifest');
          if (!response.ok) throw new Error('Could not read selected game folder');
          files = await response.json();
        }
      }
      FS.mkdirTree('/game/USA'); FS.mkdirTree('/persist');
      globalThis.RE1Diag?.stage('Restoring saves and settings');
      FS.mount(IDBFS, {}, '/persist');
      await new Promise((resolve, reject) => FS.syncfs(true, e => e ? reject(e) : resolve()));
      FS.mkdirTree('/persist/SAVE');
      if (!FS.analyzePath('/persist/config.ini').exists)
        FS.writeFile('/persist/config.ini', '[Display]\nWidth=640\nHeight=480\nFullScreen=0\nVSync=1\n[Assets]\nPath=/game\nVersion=USA\n[Save]\nPath=/persist/SAVE\n[Game]\nMode=OG\n');
      FS.symlink('/persist/config.ini', '/game/config.ini');
      const cache = new Map(); let cacheSize = 0;
      const chunkSize = 256*1024, budget = 16*1024*1024;
      let reportedReadError = false;
      const nativeStorage = typeof globalThis.AndroidLauncher?.readFileChunk === 'function';
      const browserStorage=globalThis.RE1BrowserStorage;
      globalThis.RE1Diag?.log('Game file transport: '+(browserStorage?'Browser local files':nativeStorage?'Android direct chunks':'HTTP range fallback'));
      function readChunk(file, start, expected) {
        try {
          if(browserStorage)return browserStorage.read(file.path,start,expected);
          let raw;
          if (nativeStorage) {
            const reply = globalThis.AndroidLauncher.readFileChunk(file.path, start, expected);
            if (typeof reply !== 'string' || !reply.startsWith('ok:'))
              throw new Error(typeof reply === 'string' && reply.startsWith('error:') ? reply.slice(6) : 'Invalid Android storage response');
            raw = atob(reply.slice(3));
          } else {
            const xhr = new XMLHttpRequest();
            xhr.open('GET', '/data/' + file.path.split('/').map(encodeURIComponent).join('/')+'?offset='+start+'&length='+expected, false);
            xhr.overrideMimeType('text/plain; charset=x-user-defined');
            xhr.setRequestHeader('Range', 'bytes=' + start + '-' + (start+expected-1));
            xhr.send();
            if (xhr.status !== 206 && !(xhr.status === 200 && start === 0 && file.size <= chunkSize))
              throw new Error('HTTP status '+xhr.status);
            raw = xhr.responseText;
          }
          if (raw.length !== expected) throw new Error('Expected '+expected+' bytes, received '+raw.length);
          const bytes = new Uint8Array(raw.length);
          for (let i=0; i<raw.length; i++) bytes[i] = raw.charCodeAt(i)&255;
          return bytes;
        } catch (error) {
          const message = 'Could not read '+file.path+' at byte '+start+' (length '+expected+'): '+String(error);
          globalThis.RE1Diag?.log(message);
          if (!reportedReadError) {
            reportedReadError = true;
            // Return a normal I/O error to stdio/FFmpeg; do not let a DOM
            // NetworkError escape through the wasm/Asyncify call stack.
            setTimeout(() => {
              globalThis.re1Failure?.(message+'\nCheck that the game folder is complete and stored locally, then select it again.');
              Module['_re1_web_request_quit']?.();
            }, 0);
          }
          throw new FS.ErrnoError(29); // EIO
        }
      }
      for (const file of files) {
        const path = '/game/USA/' + file.path;
        FS.mkdirTree(path.substring(0, path.lastIndexOf('/')));
        const node = FS.createFile(path.substring(0, path.lastIndexOf('/')), path.split('/').pop(), {}, true, false);
        node.usedBytes = file.size;
        const ops = node.stream_ops;
        node.stream_ops = {...ops, read(stream, buffer, offset, length, position) {
          let done = 0; length = Math.min(length, Math.max(0, file.size-position));
          while (done < length) {
            const start = Math.floor((position+done)/chunkSize)*chunkSize;
            const key = file.path + ':' + start;
            let chunk = cache.get(key);
            if (!chunk) {
              const expected = Math.min(chunkSize, file.size-start);
              globalThis.RE1Diag?.io(file.path);
              chunk = readChunk(file, start, expected);
              while (cacheSize+chunk.length>budget && cache.size) {
                const oldest = cache.keys().next().value;
                cacheSize -= cache.get(oldest).length; cache.delete(oldest);
              }
              cache.set(key, chunk); cacheSize += chunk.length;
            } else { cache.delete(key); cache.set(key, chunk); }
            const begin = position+done-start, count = Math.min(length-done, chunk.length-begin);
            if (count <= 0) throw new FS.ErrnoError(29);
            buffer.set(chunk.subarray(begin, begin+count), offset+done); done += count;
          }
          return done;
        }};
      }
      // Native file-provider reads need the same byte checks as browser tests.
      if (!Module['re1DiagnosticsOnly']) {
        globalThis.RE1Diag?.stage('Checking boot file reads');
        const items=files.find(f=>f.path.toLowerCase()==='data/item_all.pix');
        if(!items || items.size<=0 || items.size>91200)
          throw new Error('Missing or incompatible item sheet: Data/item_all.pix (maximum 91200 bytes)');
        const bootNames=['fontus.tim','font03t.tim','optkey03.tim','status.tim','statface.tim','blue.tim','staitem.tim','kage.tim'];
        for (const name of bootNames) {
          const entry=files.find(f=>f.path.toLowerCase()==='data/'+name);
          if(!entry || entry.size<8 || entry.size>832728)throw new Error('Missing, truncated or oversized boot texture: Data/'+name);
          const stream=FS.open('/game/USA/'+entry.path,'r'), bytes=new Uint8Array(8);
          let count;try {count=FS.read(stream,bytes,0,8,0);}finally {FS.close(stream);}
          if(count!==8 || bytes[0]!==0x10 || bytes[1]!==0 || bytes[2]!==0 || bytes[3]!==0)
            throw new Error('Invalid TIM header or file-provider read: '+entry.path);
        }
      }
      const open=FS.open;
      const missing=new Set();
      FS.open=function(path,...args) {
        try {return open(path,...args);} catch(error) {
          if(typeof path==='string' && path.startsWith('/game/USA/') && !missing.has(path)) {
            missing.add(path);globalThis.RE1Diag?.log('File not found: '+path);
          }
          throw error;
        }
      };
      FS.chdir('/game');
      globalThis.re1Sync = () => {
        if (globalThis.re1SyncBusy) { globalThis.re1SyncAgain = true; return; }
        globalThis.re1SyncBusy = true;
        FS.syncfs(false, e => {
          globalThis.re1SyncBusy = false;
          if (e) globalThis.re1Failure?.('Could not persist saves: ' + e);
          if (globalThis.re1SyncAgain) { globalThis.re1SyncAgain = false; globalThis.re1Sync(); }
        });
      };
      // Persist after each closed save/config file, including in-game saves.
      const close = FS.close;
      FS.close = function(stream) {
        const persist = stream.path?.startsWith('/persist/') && (stream.flags & 3) !== 0;
        const result = close(stream);
        if (persist) globalThis.re1Sync();
        return result;
      };
      removeRunDependency('android-storage');
    } catch (error) { globalThis.re1Failure?.(String(error)); }
  })();
}];
Module['onRuntimeInitialized'] = () => globalThis.re1Ready?.();
