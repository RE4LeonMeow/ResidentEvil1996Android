package org.re1.launcher;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.pm.PackageInfo;
import android.graphics.Color;
import android.graphics.Bitmap;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.view.Gravity;
import android.util.Base64;
import android.widget.FrameLayout;
import android.widget.TextView;
import android.webkit.ConsoleMessage;
import android.webkit.RenderProcessGoneDetail;
import android.webkit.WebChromeClient;
import android.webkit.WebResourceError;
import android.content.Intent;
import android.content.ActivityNotFoundException;
import android.widget.Toast;
import android.database.Cursor;
import android.hardware.input.InputManager;
import android.net.Uri;
import android.net.ConnectivityManager;
import android.net.NetworkCapabilities;
import android.os.Bundle;
import android.provider.DocumentsContract;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;
import android.webkit.JavascriptInterface;
import android.webkit.WebResourceRequest;
import android.webkit.WebResourceResponse;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import org.json.JSONArray;
import org.json.JSONObject;
import java.io.ByteArrayInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.Locale;
import java.util.Map;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/** Port-added offline Android shell. The user's files never leave the device. */
public final class MainActivity extends Activity implements InputManager.InputDeviceListener {
    private static final String ORIGIN = "https://re1.invalid";
    private final Object updateLock = new Object();
    private static final int PICK_DATA = 41;
    private static final int IMPORT_SAVES = 42, EXPORT_SAVES = 43;
    private static final int IMPORT_MOD = 44;
    private String saveRequest,saveExport;
    private String modRequest;
    private ModArchive modArchive;
    private volatile ModArchive.Import modImport;
    private java.util.concurrent.atomic.AtomicBoolean modCancel=new java.util.concurrent.atomic.AtomicBoolean();
    private long lastModProgress;
    private WebView web;
    private TextView progressText;
    private volatile boolean loadingFilenames=true;
    private final Runnable hideLoadingText=()->{if(progressText!=null)progressText.setVisibility(View.GONE);};
    private final Handler ui = new Handler(Looper.getMainLooper());
    private final java.util.ArrayDeque<String> reportLines = new java.util.ArrayDeque<>();
    private volatile long lastProgress;
    private volatile boolean loading, paused, rendererGone;
    private String lastPhase = "Opening launcher";
    private volatile String javascriptReport = "";
    private boolean timeoutShown, resetting;
    private final Runnable watchdog = new Runnable() {
        @Override public void run() {
            if (destroyed) return;
            if (loading && !paused && !timeoutShown && SystemClock.uptimeMillis()-lastProgress>30000) {
                timeoutShown=true;
                record("No progress for 30 seconds: "+lastPhase);
                showReport("Startup stalled: "+lastPhase);
            }
            ui.postDelayed(this,1000);
        }
    };
    private final ExecutorService storage = Executors.newSingleThreadExecutor();
    private volatile Map<String, Entry> files = Collections.emptyMap();
    private volatile String manifest = "[]";
    private volatile String storageError = "Select your GOG game folder.";
    private final GamepadState gamepad = new GamepadState();
    private int padKeyEvents, padMotionEvents;
    private InputManager inputs;
    private boolean destroyed;
    private static final class Entry {
        final Uri uri; final long size;
        Entry(Uri uri, long size) { this.uri = uri; this.size = size; }
    }
    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        immersive();
        record("Resident Evil Android 1.1 (build 42) | "+Build.MANUFACTURER+" "+Build.MODEL+" | Android "+Build.VERSION.RELEASE);
        PackageInfo provider=WebView.getCurrentWebViewPackage();
        record("WebView: "+(provider==null?"unavailable":provider.packageName+" "+provider.versionName));
        try{modArchive=new ModArchive(new java.io.File(getFilesDir(),"mods-v1"),new java.io.File(getCacheDir(),"mod-imports"));}
        catch(IOException error){record("Mod storage unavailable: "+error.getMessage());}
        web = new WebView(this);
        web.setWebChromeClient(new WebChromeClient() {
            private final Bitmap moviePoster=Bitmap.createBitmap(1,1,Bitmap.Config.ARGB_8888);
            @Override public Bitmap getDefaultVideoPoster() { return moviePoster; }
            @Override public boolean onConsoleMessage(ConsoleMessage message) {
                record(message.messageLevel()+": "+message.message());return true;
            }
        });
        web.setBackgroundColor(0xff080a0b);
        web.getSettings().setJavaScriptEnabled(true);
        web.getSettings().setDomStorageEnabled(true);
        web.getSettings().setAllowFileAccess(false);
        web.getSettings().setAllowContentAccess(false);
        web.getSettings().setMediaPlaybackRequiresUserGesture(false);
        web.getSettings().setSupportMultipleWindows(false);
        web.addJavascriptInterface(new Bridge(), "AndroidLauncher");
        web.setWebViewClient(new WebViewClient() {
            @Override public boolean shouldOverrideUrlLoading(WebView view, WebResourceRequest req) {
                Uri target=req.getUrl();
                // Permit our own launcher reload; keep external pages out of
                // the WebView that has access to the native storage bridge.
                return !(req.isForMainFrame() && "https".equals(target.getScheme()) &&
                    "re1.invalid".equals(target.getHost()) && "/web/index.html".equals(target.getPath()));
            }
            @Override public void onReceivedError(WebView view, WebResourceRequest req, WebResourceError error) {
                record("WebView load error: "+error.getErrorCode()+" "+error.getDescription()+" | "+req.getMethod()+" "+req.getUrl());
                if(req.isForMainFrame()) showReport("Could not load launcher");
            }
            @Override public boolean onRenderProcessGone(WebView view, RenderProcessGoneDetail detail) {
                rendererGone=true;loading=false;
                record("WebView renderer stopped; crashed="+detail.didCrash());
                view.destroy();showReport("Android stopped the graphics process. Copy this report, then reload.");return true;
            }
            @Override public WebResourceResponse shouldInterceptRequest(WebView view, WebResourceRequest req) {
                return serve(req);
            }
        });
        FrameLayout layout=new FrameLayout(this);
        FrameLayout.LayoutParams webParams=new FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT,FrameLayout.LayoutParams.MATCH_PARENT);
        layout.addView(web,webParams);
        loadingFilenames=getPreferences(0).getBoolean("loadingFilenames",true);
        progressText=new TextView(this);progressText.setTextColor(Color.WHITE);progressText.setTextSize(10);progressText.setMaxLines(1);
        progressText.setEllipsize(android.text.TextUtils.TruncateAt.END);
        progressText.setShadowLayer(3,1,1,Color.BLACK);progressText.setPadding(12,0,12,8);progressText.setVisibility(View.GONE);
        progressText.setClickable(false);progressText.setFocusable(false);
        FrameLayout.LayoutParams textParams=new FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT,FrameLayout.LayoutParams.WRAP_CONTENT,Gravity.BOTTOM);
        layout.addView(progressText,textParams);setContentView(layout);
        ui.postDelayed(watchdog,1000);
        web.loadUrl(ORIGIN + "/web/index.html");
        inputs = (InputManager)getSystemService(INPUT_SERVICE);
        inputs.registerInputDeviceListener(this, null);
        String saved = getPreferences(0).getString("dataTree", null);
        if (saved != null) index(Uri.parse(saved));
    }
    private void immersive() {
        getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_FULLSCREEN |
            View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY |
            View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION |
            View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
    }
    private void showLoadingText(String value) {
        if(destroyed||!loadingFilenames)return;
        progressText.setText(value);progressText.setVisibility(View.VISIBLE);
        ui.removeCallbacks(hideLoadingText);ui.postDelayed(hideLoadingText,1500);
    }
    private synchronized void record(String line) {
        reportLines.addLast(line.substring(0,Math.min(line.length(),1200)));
        while(reportLines.size()>220)reportLines.removeFirst();
    }
    private synchronized String report() {
        return (javascriptReport.isEmpty()?"":javascriptReport+"\n\nNative log:\n")+
            "Controller events: keys="+padKeyEvents+" motion="+padMotionEvents+"\n"+String.join("\n",reportLines);
    }
    private void copy(String value) {
        ((ClipboardManager)getSystemService(CLIPBOARD_SERVICE)).setPrimaryClip(ClipData.newPlainText("RE1 diagnostic report",value));
    }
    private void showReport(String title) {
        runOnUiThread(()->{
            if(destroyed||isFinishing())return;
            new AlertDialog.Builder(this).setTitle(title).setMessage(report())
                .setPositiveButton("Copy report",(dialog,which)->copy(report()))
                .setNeutralButton("Reload launcher",(dialog,which)->resetLauncher())
                .setNegativeButton("Close",null).show();
        });
    }
    private void resetLauncher() {
        runOnUiThread(()->{
            if (destroyed || resetting || isFinishing()) return;
            resetting=true;loading=false;
            // Replace the failed engine's view instead of navigating within
            // that page. Keep the selected folder and persistent app storage.
            recreate();
        });
    }
    private byte[] readChunk(String path, long offset, int length) throws IOException {
        Entry entry=files.get(path);
        if(entry==null)throw new FileNotFoundException(path);
        FileChunks.validate(entry.size,offset,length);
        record("Read "+path+" bytes "+offset+".."+(offset+length-1));
        runOnUiThread(()->showLoadingText("Loading: "+path));
        try(InputStream source=getContentResolver().openInputStream(entry.uri)) {
            if(source==null)throw new FileNotFoundException(path);
            return FileChunks.read(source,entry.size,offset,length);
        }
    }
    public final class Bridge {
        @JavascriptInterface public void pickMod(String request) {
            runOnUiThread(()->{
                if(modArchive==null){modResult(request,null,"Mod storage is unavailable.",false);return;}
                if(modRequest!=null||saveRequest!=null){modResult(request,null,"Finish the other import first.",false);return;}
                modRequest=request;modCancel=new java.util.concurrent.atomic.AtomicBoolean();
                Intent intent=new Intent(Intent.ACTION_OPEN_DOCUMENT).addCategory(Intent.CATEGORY_OPENABLE)
                    .setType("*/*").putExtra(Intent.EXTRA_ALLOW_MULTIPLE,false);
                try{startActivityForResult(intent,IMPORT_MOD);}
                catch(ActivityNotFoundException error){modRequest=null;modResult(request,null,"No file picker is available.",false);}
            });
        }
        @JavascriptInterface public String readModChunk(String id,int slot,long offset,int length) {
            try{
                ModArchive.Import staged=modImport;
                byte[] data=staged!=null&&staged.id.equals(id)?staged.read(slot,offset,length):modArchive.read(id,slot,offset,length);
                return "ok:"+Base64.encodeToString(data,Base64.NO_WRAP);
            }catch(Exception error){return "error:"+error.getMessage();}
        }
        @JavascriptInterface public void finishMod(String request,String id,String slotsJson) {
            ModArchive.Import staged=modImport;
            if(staged==null||!staged.id.equals(id)){modResult(request,null,"The mod import is no longer open.",false);return;}
            storage.execute(()->{
                boolean success=false;
                try{
                    JSONArray input=new JSONArray(slotsJson);if(input.length()>ModArchive.MAX_FILES)throw new IOException("Too many mod data files");
                    int[] slots=new int[input.length()];for(int n=0;n<slots.length;n++)slots[n]=input.getInt(n);
                    staged.commit(slots,(done,total,phase)->modProgress(done,total,phase));success=true;
                    modResult(request,new JSONObject().put("id",id),null,false);
                }catch(Exception error){modResult(request,null,error.getMessage(),false);}
                finally{
                    try{staged.close();if(!success)modArchive.remove(staged.id);}catch(IOException error){record("Mod cleanup: "+error.getMessage());}
                    if(modImport==staged)modImport=null;runOnUiThread(()->modRequest=null);
                }
            });
        }
        @JavascriptInterface public void cancelMod(String request) {
            modCancel.set(true);
            storage.execute(()->{
                ModArchive.Import staged=modImport;
                try{if(staged!=null){staged.close();modArchive.remove(staged.id);}}
                catch(IOException error){record("Mod cleanup: "+error.getMessage());}
                finally{
                    if(modImport==staged)modImport=null;
                    runOnUiThread(()->{modRequest=null;modResult(request,new JSONObject(),null,false);});
                }
            });
        }
        @JavascriptInterface public void removeMod(String request,String id) {
            storage.execute(()->{
                try{modArchive.remove(id);modResult(request,new JSONObject(),null,false);}
                catch(Exception error){modResult(request,null,error.getMessage(),false);}
            });
        }
        @JavascriptInterface public void cleanMods(String request,String keepJson) {
            storage.execute(()->{
                try{
                    JSONArray values=new JSONArray(keepJson);if(values.length()>100)throw new IOException("Invalid mod library");
                    java.util.Set<String> keep=new java.util.HashSet<>();for(int n=0;n<values.length();n++)keep.add(values.getString(n));
                    ModArchive.Import staged=modImport;modArchive.cleanup(keep,staged==null?null:staged.id);
                    modResult(request,new JSONObject(),null,false);
                }catch(Exception error){modResult(request,null,error.getMessage(),false);}
            });
        }
        @JavascriptInterface public void exportSaves(String request,String text,String filename) {
            beginSaveExport(request,text,filename);
        }
        @JavascriptInterface public void importSaves(String request) {
            runOnUiThread(()->{
                if(destroyed||rendererGone)return;
                if(saveRequest!=null){saveResult(request,null,"Another save operation is open.",false);return;}
                saveRequest=request;saveExport=null;
                Intent intent=new Intent(Intent.ACTION_OPEN_DOCUMENT).addCategory(Intent.CATEGORY_OPENABLE)
                    .setType("*/*");
                try{startActivityForResult(intent,IMPORT_SAVES);}
                catch(ActivityNotFoundException error){saveRequest=null;saveResult(request,null,"No file picker is available.",false);}
            });
        }
        @JavascriptInterface public void openExternal(String address) {
            if(!"https://x.com/4wu4_".equals(address) && !"https://discord.gg/5PVsXAgvM2".equals(address) && !"https://github.com/RE4LeonMeow".equals(address) && !UpdateTransport.RELEASES.equals(address) && !"https://github.com/RE4LeonMeow/ResidentEvil1996Android#mods-and-making-texture-packs".equals(address))return;
            runOnUiThread(()->{
                if(destroyed||isFinishing())return;
                try{startActivity(new Intent(Intent.ACTION_VIEW,Uri.parse(address)));}
                catch(ActivityNotFoundException error){Toast.makeText(MainActivity.this,"No app can open this link.",Toast.LENGTH_SHORT).show();}
            });
        }
        @JavascriptInterface public boolean hasUpdateChecker() { return true; }
        @JavascriptInterface public boolean updateNetworkAvailable() { return networkAvailable(); }
        @JavascriptInterface public boolean loadingFilenamesVisible() {return loadingFilenames;}
        @JavascriptInterface public void showLoadingFilenames(boolean visible) {
            runOnUiThread(()->{
                if(destroyed)return;
                loadingFilenames=visible;getPreferences(0).edit().putBoolean("loadingFilenames",visible).apply();
                if(!visible)progressText.setVisibility(View.GONE);
            });
        }
        @JavascriptInterface public void setReport(String value) {javascriptReport=value;}
        @JavascriptInterface public void openLogs() {showReport("Startup diagnostics");}
        @JavascriptInterface public void returnToLauncher() {resetLauncher();}
        @JavascriptInterface public String readFileChunk(String path, long offset, int length) {
            try {
                // Avoid synchronous XHR/partial-response handling in WebView.
                return "ok:"+Base64.encodeToString(readChunk(path,offset,length),Base64.NO_WRAP);
            } catch (Exception error) {
                String detail=error.getClass().getSimpleName()+": "+error.getMessage();
                record("File read failed: "+path+" offset="+offset+" length="+length+" "+detail);
                return "error:"+detail;
            }
        }
        @JavascriptInterface public void log(String line) {record(line);}
        @JavascriptInterface public void progress(String phase) {
            lastProgress=SystemClock.uptimeMillis();lastPhase=phase;
        }
        @JavascriptInterface public void begin() {
            loading=true;timeoutShown=false;lastProgress=SystemClock.uptimeMillis();
            runOnUiThread(()->showLoadingText("Loading game engine…"));
        }
        @JavascriptInterface public void paused(boolean value) {paused=value;lastProgress=SystemClock.uptimeMillis();}
        @JavascriptInterface public void finished() {loading=false;}
        @JavascriptInterface public void copyReport(String value) {
            javascriptReport=value;runOnUiThread(()->copy(report()));
        }
        @JavascriptInterface public void selectFolder() {
            runOnUiThread(() -> {
                Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
                intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
                startActivityForResult(intent, PICK_DATA);
            });
        }
        @JavascriptInterface public String status() {
            return files.isEmpty() ? storageError : "Ready: " + files.size() + " game files";
        }
        @JavascriptInterface public boolean ready() { return !files.isEmpty(); }
    }
    @Override protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if(request==IMPORT_MOD){
            String callback=modRequest;
            if(callback==null)return;
            if(result!=RESULT_OK||data==null){modRequest=null;modResult(callback,null,null,true);return;}
            ClipData clips=data.getClipData();
            if(clips!=null&&clips.getItemCount()!=1){modRequest=null;modResult(callback,null,"Select one mod ZIP at a time",false);return;}
            Uri document=data.getData()!=null?data.getData():clips!=null?clips.getItemAt(0).getUri():null;
            if(document==null){modRequest=null;modResult(callback,null,null,true);return;}
            java.util.concurrent.atomic.AtomicBoolean cancelled=modCancel;
            storage.execute(()->{
                try{
                    long size=-1;String name="Mod ZIP";
                    try(Cursor cursor=getContentResolver().query(document,new String[]{android.provider.OpenableColumns.DISPLAY_NAME,android.provider.OpenableColumns.SIZE},null,null,null)){
                        if(cursor!=null&&cursor.moveToFirst()){
                            if(!cursor.isNull(0))name=cursor.getString(0);
                            if(!cursor.isNull(1))size=cursor.getLong(1);
                        }
                    }
                    ModArchive.Import opened;
                    try(InputStream source=getContentResolver().openInputStream(document)){
                        opened=modArchive.open(source,size,cancelled::get,(done,total,phase)->modProgress(done,total,phase));
                    }
                    if(cancelled.get()){opened.close();throw new IOException("Import cancelled");}
                    modImport=opened;JSONArray entries=new JSONArray();
                    for(ModArchive.Item item:opened.items)entries.put(new JSONObject().put("path",item.path).put("slot",item.slot).put("size",item.size).put("crc",item.crc));
                    modResult(callback,new JSONObject().put("id",opened.id).put("name",name).put("files",entries),null,false);
                }catch(Exception error){runOnUiThread(()->modRequest=null);modResult(callback,null,error.getMessage(),cancelled.get());}
            });return;
        }
        if(request==IMPORT_SAVES||request==EXPORT_SAVES){
            String id=saveRequest,text=saveExport;saveRequest=null;saveExport=null;
            if(id==null)return;
            if(result!=RESULT_OK||data==null||(data.getData()==null&&data.getClipData()==null)){saveResult(id,null,null,true);return;}
            java.util.ArrayList<Uri> documents=new java.util.ArrayList<>();
            if(request==IMPORT_SAVES&&data.getClipData()!=null){
                ClipData clips=data.getClipData();
                if(clips.getItemCount()>1){saveResult(id,null,"Choose one save ZIP.",false);return;}
                for(int n=0;n<clips.getItemCount();n++)documents.add(clips.getItemAt(n).getUri());
            }else if(data.getData()!=null)documents.add(data.getData());
            if(documents.isEmpty()){saveResult(id,null,null,true);return;}
            storage.execute(()->{
                try{
                    if(request==IMPORT_SAVES){
                        for(Uri document:documents){
                            String name=null;
                            try(Cursor cursor=getContentResolver().query(document,new String[]{android.provider.OpenableColumns.DISPLAY_NAME},null,null,null)){
                                if(cursor!=null&&cursor.moveToFirst())name=cursor.getString(0);
                            }
                            if(name==null)throw new IOException("Save file name unavailable.");
                            name=name.toLowerCase(Locale.ROOT);
                            if(!name.endsWith(".zip"))throw new IOException("Choose a ZIP containing savedat1.dat through savedat8.dat.");
                            byte[] bytes;
                            try(InputStream input=getContentResolver().openInputStream(document)){bytes=SaveBackupIO.readBytes(input,SaveBackupIO.MAX_BYTES);}
                            saveResult(id,Base64.encodeToString(bytes,Base64.NO_WRAP),null,false);
                        }
                    }else{
                        try(OutputStream output=getContentResolver().openOutputStream(documents.get(0),"wt")){
                            SaveBackupIO.writeBytes(output,Base64.decode(text,Base64.NO_WRAP));
                        }
                        saveResult(id,null,null,false);
                    }
                }catch(Exception error){saveResult(id,null,"Save backup: "+error.getMessage(),false);}
            });
            return;
        }
        if (request != PICK_DATA || result != RESULT_OK || data == null || data.getData() == null) return;
        Uri tree = data.getData();
        try {
            getContentResolver().takePersistableUriPermission(tree, Intent.FLAG_GRANT_READ_URI_PERMISSION);
            index(tree);
        } catch (Exception e) { storageError = "Cannot access folder: " + e.getMessage(); }
    }
    private void beginSaveExport(String request,String data,String filename) {
        runOnUiThread(()->{
            if(destroyed||rendererGone)return;
            if(saveRequest!=null){saveResult(request,null,"Another save operation is open.",false);return;}
            String pattern="resident-evil-[A-Za-z0-9T-]+\\.saves\\.zip";
            if(data==null||data.length()>SaveBackupIO.MAX_BYTES||filename==null||!filename.matches(pattern)){
                saveResult(request,null,"Invalid save backup.",false);return;
            }
            saveRequest=request;saveExport=data;
            Intent intent=new Intent(Intent.ACTION_CREATE_DOCUMENT).addCategory(Intent.CATEGORY_OPENABLE)
                .setType("application/zip").putExtra(Intent.EXTRA_TITLE,filename);
            try{startActivityForResult(intent,EXPORT_SAVES);}
            catch(ActivityNotFoundException error){saveRequest=null;saveExport=null;saveResult(request,null,"No file picker is available.",false);}
        });
    }
    private void saveResult(String request,String text,String error,boolean cancelled) {
        try{
            JSONObject result=new JSONObject();result.put("cancelled",cancelled);
            if(text!=null)result.put("data",text);
            if(error!=null)result.put("error",error);
            String script="window.RE1SaveData?.nativeResult("+JSONObject.quote(request)+","+result.toString()+")";
            runOnUiThread(()->{if(!destroyed&&!rendererGone)web.evaluateJavascript(script,null);});
        }catch(Exception failure){record("Save backup result failed: "+failure.getMessage());}
    }
    private void modProgress(long done,long total,String phase) {
        long now=SystemClock.uptimeMillis();if(now-lastModProgress<100&&done!=total)return;lastModProgress=now;
        String script="window.RE1Mods?.nativeProgress("+done+","+total+","+JSONObject.quote(phase)+")";
        runOnUiThread(()->{if(!destroyed&&!rendererGone)web.evaluateJavascript(script,null);});
    }
    private void modResult(String request,JSONObject value,String error,boolean cancelled) {
        try{
            JSONObject result=value==null?new JSONObject():value;result.put("cancelled",cancelled);
            if(error!=null)result.put("error",error);
            String script="window.RE1Mods?.nativeResult("+JSONObject.quote(request)+","+result+")";
            runOnUiThread(()->{if(!destroyed&&!rendererGone)web.evaluateJavascript(script,null);});
        }catch(Exception failure){record("Mod result failed: "+failure.getMessage());}
    }
    private void index(Uri tree) {
        storageError = "Scanning game files…";
        files=Collections.emptyMap();manifest="[]";
        storage.execute(() -> {
            try {
                Map<String, Entry> found = new LinkedHashMap<>();
                scan(tree, DocumentsContract.getTreeDocumentId(tree), "", found, 0);
                String prefix = null;
                for (String path : found.keySet()) {
                    if (path.toLowerCase(Locale.ROOT).endsWith("data/fontus.tim")) {
                        String candidate = path.substring(0, path.length() - "data/fontus.tim".length());
                        if (prefix != null && !prefix.equals(candidate))
                            throw new IOException("Multiple game installations found. Select the specific USA game folder.");
                        prefix = candidate;
                    }
                }
                if (prefix == null) throw new IOException("Data/fontus.tim is missing. Select an installed/extracted GOG game folder, not the installer EXE.");
                Map<String, Entry> selected = new LinkedHashMap<>();
                JSONArray listing = new JSONArray();
                for (Map.Entry<String, Entry> item : found.entrySet()) {
                    if (!item.getKey().startsWith(prefix)) continue;
                    String path = item.getKey().substring(prefix.length());
                    selected.put(path, item.getValue());
                    JSONObject value = new JSONObject();
                    value.put("path", path); value.put("size", item.getValue().size); listing.put(value);
                }
                java.util.Set<String> paths = new java.util.HashSet<>();
                boolean rooms = false;
                for (Map.Entry<String, Entry> item : selected.entrySet()) {
                    String lower = item.getKey().toLowerCase(Locale.ROOT);
                    if (item.getValue().size > 0) paths.add(lower);
                    if (lower.startsWith("stage") && lower.endsWith(".rdt") && item.getValue().size > 0) rooms = true;
                }
                for (String required : new String[]{"data/fontus.tim", "data/font03t.tim", "data/item_all.pix", "data/optkey03.tim", "data/status.tim", "data/statface.tim", "data/blue.tim", "data/staitem.tim", "data/kage.tim"})
                    if (!paths.contains(required)) throw new IOException("Required game file is missing or empty: " + required);
                if (!rooms) throw new IOException("Stage room files (.rdt) are missing. Select the complete installed game folder.");
                manifest = listing.toString();
                files = Collections.unmodifiableMap(selected);
                getPreferences(0).edit().putString("dataTree", tree.toString()).apply();
                storageError = "";
                runOnUiThread(()->{if(!destroyed&&!rendererGone)web.evaluateJavascript("window.dispatchEvent(new Event('re1-storage-changed'))",null);});
            } catch (Exception error) {
                files = Collections.emptyMap(); manifest = "[]";
                storageError = "Game folder: " + error.getMessage();
            }
        });
    }
    private void scan(Uri tree, String parent, String prefix, Map<String, Entry> found, int depth) throws Exception {
        if (depth > 24 || found.size() > 50000) throw new IOException("Folder is too large or deeply nested");
        Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(tree, parent);
        try (Cursor cursor = getContentResolver().query(children, new String[]{
            DocumentsContract.Document.COLUMN_DOCUMENT_ID, DocumentsContract.Document.COLUMN_DISPLAY_NAME,
            DocumentsContract.Document.COLUMN_MIME_TYPE, DocumentsContract.Document.COLUMN_SIZE}, null, null, null)) {
            if (cursor == null) throw new IOException("Folder provider did not return a file list");
            while (cursor.moveToNext()) {
                String id = cursor.getString(0), name = cursor.getString(1), type = cursor.getString(2);
                if (name == null || name.equals(".") || name.equals("..") || name.contains("/") || name.contains("\\"))
                    throw new IOException("Invalid file name in selected folder");
                String path = prefix + name;
                if (DocumentsContract.Document.MIME_TYPE_DIR.equals(type)) scan(tree, id, path + "/", found, depth+1);
                else {
                    if (cursor.isNull(3) || cursor.getLong(3) < 0) throw new IOException("File size unavailable: " + path);
                    if (found.put(path, new Entry(DocumentsContract.buildDocumentUriUsingTree(tree, id), cursor.getLong(3))) != null)
                        throw new IOException("Duplicate file name: " + path);
                }
            }
        }
    }
    private WebResourceResponse serve(WebResourceRequest request) {
        Uri uri = request.getUrl();
        if (!"https".equals(uri.getScheme()) || !"re1.invalid".equals(uri.getHost()) || !"GET".equals(request.getMethod()))
            return response(403, "Forbidden", "text/plain", new byte[0]);
        String path = uri.getPath();
        try {
            if ("/updates".equals(path))return updates("1".equals(uri.getQueryParameter("force")));
            if ("/manifest".equals(path))
                return response(files.isEmpty()?409:200, files.isEmpty()?"Conflict":"OK", "application/json", manifest.getBytes(StandardCharsets.UTF_8));
            if(path!=null&&path.matches("/mod-video/pack-[0-9a-f]{32}/[0-9]{1,5}\\.mp4")){
                String[] parts=path.split("/");java.io.File movie=modArchive.mediaFile(parts[2],Integer.parseInt(parts[3].replace(".mp4","")));
                String range=request.getRequestHeaders().get("Range");if(range==null)range=request.getRequestHeaders().get("range");
                long[] slice;
                try{slice=ModArchive.mediaRange(range,movie.length());}
                catch(IOException error){return response(416,"Range Not Satisfiable","text/plain",new byte[0]);}
                Map<String,String> h=headers();h.put("Accept-Ranges","bytes");h.put("Content-Length",Long.toString(slice[1]));
                if(range!=null)h.put("Content-Range","bytes "+slice[0]+"-"+(slice[0]+slice[1]-1)+"/"+movie.length());
                return new WebResourceResponse("video/mp4",null,range==null?200:206,range==null?"OK":"Partial Content",h,ModArchive.mediaStream(movie,slice[0],slice[1]));
            }
            if (path != null && path.startsWith("/data/")) {
                Entry entry = files.get(path.substring(6));
                if (entry == null) return response(404, "Not Found", "text/plain", new byte[0]);
                long start=0, end=entry.size-1; boolean ranged=false;
                String offset=uri.getQueryParameter("offset"),length=uri.getQueryParameter("length");
                if(offset!=null || length!=null) {
                    if(offset==null||length==null||!offset.matches("[0-9]+")||!length.matches("[0-9]+"))
                        return response(416,"Range Not Satisfiable","text/plain",new byte[0]);
                    start=Long.parseLong(offset);long count=Long.parseLong(length);
                    if(count<=0||count>FileChunks.MAX_LENGTH||start<0||start>entry.size-count)
                        return response(416,"Range Not Satisfiable","text/plain",new byte[0]);
                    end=start+count-1;ranged=true;
                }
                String range = request.getRequestHeaders().get("Range");
                if (range == null) range = request.getRequestHeaders().get("range");
                if (!ranged && range != null) {
                    if (!range.matches("bytes=[0-9]+-[0-9]+")) return response(416, "Range Not Satisfiable", "text/plain", new byte[0]);
                    String[] numbers = range.substring(6).split("-");
                    start=Long.parseLong(numbers[0]); end=Math.min(Long.parseLong(numbers[1]), entry.size-1); ranged=true;
                    if (start > end || end-start+1 > FileChunks.MAX_LENGTH) return response(416, "Range Not Satisfiable", "text/plain", new byte[0]);
                }
                final long count = Math.max(0, end-start+1);
                if(count>FileChunks.MAX_LENGTH)return response(416,"Range Not Satisfiable","text/plain",new byte[0]);
                // Finish provider I/O here so exceptions are caught and the
                // response cannot fail later while Chromium consumes it.
                byte[] bytes=count==0?new byte[0]:readChunk(path.substring(6),start,(int)count);
                Map<String,String> headers = headers();
                headers.put("Content-Length", Long.toString(count));
                headers.put("Accept-Ranges", "bytes");
                if(ranged) headers.put("Content-Range", "bytes "+start+"-"+end+"/"+entry.size);
                // No charset header: the runtime deliberately selects x-user-defined for binary sync XHR.
                return new WebResourceResponse("application/octet-stream", null, ranged?206:200, ranged?"Partial Content":"OK", headers, new ByteArrayInputStream(bytes));
            }
            if (path == null || !path.startsWith("/web/") || path.contains("..")) return response(404,"Not Found","text/plain",new byte[0]);
            String name=path.substring(1);
            String mime=path.endsWith(".wasm")?"application/wasm":path.endsWith(".js")?"application/javascript":path.endsWith(".json")?"application/json":path.endsWith(".css")?"text/css":path.endsWith(".ttf")?"font/ttf":path.endsWith(".png")?"image/png":path.endsWith(".jpg")?"image/jpeg":path.endsWith(".txt")?"text/plain":"text/html";
            boolean binary=path.endsWith(".wasm")||mime.startsWith("image/")||mime.startsWith("font/");
            return new WebResourceResponse(mime, binary?null:"UTF-8", 200,"OK",headers(),getAssets().open(name));
        } catch (Exception error) { record("File request failed: "+path+" "+error.getClass().getSimpleName()+" "+error.getMessage());return response(404,"Not Found","text/plain",new byte[0]); }
    }
    private boolean networkAvailable() {
        ConnectivityManager manager=(ConnectivityManager)getSystemService(CONNECTIVITY_SERVICE);
        if(manager==null)return false;
        NetworkCapabilities capabilities=manager.getNetworkCapabilities(manager.getActiveNetwork());
        return capabilities!=null&&capabilities.hasCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET);
    }
    private WebResourceResponse updates(boolean force) {
        // WebView resource interception runs off the UI thread. This request
        // never shares the game-file/mod executor or blocks the Launch button.
        synchronized(updateLock) {
            JSONObject reply=new JSONObject();
            try {
                if(!networkAvailable())reply.put("state","offline");
                else {
                    android.content.SharedPreferences preferences=getPreferences(0);
                    long age=System.currentTimeMillis()-preferences.getLong("releaseCheckedAt",0);
                    String cached=preferences.getString("latestRelease","");
                    JSONObject release=null;
                    if(!force&&age>=0&&age<6*60*60*1000L&&!cached.isEmpty()) {
                        try{release=new JSONObject(cached);}catch(Exception ignored){}
                    }
                    if(release==null) {
                        UpdateTransport.Result result=UpdateTransport.latest();
                        if(result.status==200) {
                            JSONObject source=new JSONObject(result.body);
                            // Expose only public metadata used by the badge.
                            release=new JSONObject().put("tag_name",source.getString("tag_name"))
                                .put("draft",source.getBoolean("draft")).put("prerelease",source.getBoolean("prerelease"));
                            preferences.edit().putString("latestRelease",release.toString())
                                .putLong("releaseCheckedAt",System.currentTimeMillis()).apply();
                        }else reply.put("state",result.status==404?"no-release":"unavailable");
                    }
                    if(release!=null)reply.put("state","ok").put("release",release);
                }
            }catch(Exception error) {
                try{reply=new JSONObject().put("state",networkAvailable()?"unavailable":"offline");}
                catch(Exception ignored){}
            }
            return response(200,"OK","application/json",reply.toString().getBytes(StandardCharsets.UTF_8));
        }
    }
    private static Map<String,String> headers() {
        Map<String,String> h=new LinkedHashMap<>();
        h.put("Cache-Control","no-store"); h.put("X-Content-Type-Options","nosniff");
        h.put("Content-Security-Policy","default-src 'self'; script-src 'self' 'wasm-unsafe-eval'; worker-src 'self' blob:; style-src 'self'; connect-src 'self' blob:; media-src 'self' blob:; img-src 'self' data:; object-src 'none'; base-uri 'none'; frame-src 'none'");
        return h;
    }
    private static WebResourceResponse response(int code,String reason,String mime,byte[] bytes) {
        return new WebResourceResponse(mime,"UTF-8",code,reason,headers(),new ByteArrayInputStream(bytes));
    }
    private int button(int code) {
        switch(code) {
            case KeyEvent.KEYCODE_BUTTON_A:case KeyEvent.KEYCODE_DPAD_CENTER:return 0;
            case KeyEvent.KEYCODE_BUTTON_B:case KeyEvent.KEYCODE_BACK:return 1;
            case KeyEvent.KEYCODE_BUTTON_X:return 2; case KeyEvent.KEYCODE_BUTTON_Y:return 3;
            case KeyEvent.KEYCODE_BUTTON_L1:return 4; case KeyEvent.KEYCODE_BUTTON_R1:return 5;
            case KeyEvent.KEYCODE_BUTTON_L2:return 6; case KeyEvent.KEYCODE_BUTTON_R2:return 7;
            case KeyEvent.KEYCODE_BUTTON_SELECT:return 8; case KeyEvent.KEYCODE_BUTTON_START:return 9;
            case KeyEvent.KEYCODE_BUTTON_THUMBL:return 10; case KeyEvent.KEYCODE_BUTTON_THUMBR:return 11;
            case KeyEvent.KEYCODE_DPAD_UP:return 12; case KeyEvent.KEYCODE_DPAD_DOWN:return 13;
            case KeyEvent.KEYCODE_DPAD_LEFT:return 14; case KeyEvent.KEYCODE_DPAD_RIGHT:return 15;
            default:return -1;
        }
    }
    private boolean controllerSource(int source) {
        return (source&InputDevice.SOURCE_GAMEPAD)==InputDevice.SOURCE_GAMEPAD ||
            (source&InputDevice.SOURCE_JOYSTICK)==InputDevice.SOURCE_JOYSTICK ||
            (source&InputDevice.SOURCE_DPAD)==InputDevice.SOURCE_DPAD;
    }
    private void describePad(int id,InputDevice device) {
        if(!gamepad.contains(id))record("Controller input: device="+id+" "+
            (device==null?"unknown":device.getName()+" vendor="+device.getVendorId()+
            " product="+device.getProductId()+" sources=0x"+Integer.toHexString(device.getSources())));
    }
    @Override public boolean dispatchKeyEvent(KeyEvent event) {
        int bit=button(event.getKeyCode());
        InputDevice device=event.getDevice();
        // Some composite HID controllers label button packets as KEYBOARD.
        // Accept those only when the originating device also advertises a pad.
        boolean controller=controllerSource(event.getSource()) || (device!=null&&
            (device.supportsSource(InputDevice.SOURCE_GAMEPAD)||device.supportsSource(InputDevice.SOURCE_JOYSTICK)));
        if(bit>=0 && controller && (event.getAction()==KeyEvent.ACTION_DOWN||event.getAction()==KeyEvent.ACTION_UP)) {
            describePad(event.getDeviceId(),device);padKeyEvents++;
            // Publish repeats too: JS may have cleared its snapshot on a menu
            // transition while Android still holds the same physical button.
            gamepad.key(event.getDeviceId(),bit,event.getAction()==KeyEvent.ACTION_DOWN);
            publishPad();return true;
        }
        return super.dispatchKeyEvent(event);
    }
    @Override public boolean dispatchGenericMotionEvent(MotionEvent event) {
        // Activity.onGenericMotionEvent is only a fallback AFTER the focused
        // WebView. Read axes here, alongside dispatchKeyEvent, so one device
        // cannot be split between native buttons and swallowed WebView axes.
        if(controllerSource(event.getSource()) && event.getAction()==MotionEvent.ACTION_MOVE) {
            describePad(event.getDeviceId(),event.getDevice());padMotionEvents++;
            gamepad.motion(event.getDeviceId(),event.getAxisValue(MotionEvent.AXIS_X),event.getAxisValue(MotionEvent.AXIS_Y),
                event.getAxisValue(MotionEvent.AXIS_HAT_X),event.getAxisValue(MotionEvent.AXIS_HAT_Y),
                Math.max(event.getAxisValue(MotionEvent.AXIS_LTRIGGER),event.getAxisValue(MotionEvent.AXIS_BRAKE)),
                Math.max(event.getAxisValue(MotionEvent.AXIS_RTRIGGER),event.getAxisValue(MotionEvent.AXIS_GAS)));
            publishPad();return true;
        }
        return super.dispatchGenericMotionEvent(event);
    }
    private void publishPad() {
        GamepadState.Snapshot pad=gamepad.snapshot();
        if(web!=null && !destroyed && !rendererGone && !resetting)
            web.evaluateJavascript("window.re1Native?.("+pad.buttons+","+pad.x+","+pad.y+","+pad.connected+")",null);
    }
    private void clearPad() {gamepad.clear();publishPad();}
    @Override public void onInputDeviceRemoved(int id) { if(gamepad.remove(id)){record("Controller removed: "+id);publishPad();} }
    @Override public void onInputDeviceAdded(int id) {}
    @Override public void onInputDeviceChanged(int id) { if(gamepad.remove(id)){record("Controller changed: "+id);publishPad();} }
    @Override public void onWindowFocusChanged(boolean focused) {super.onWindowFocusChanged(focused);if(focused)immersive();else clearPad();}
    @Override protected void onPause() {
        paused=true;clearPad();if(!rendererGone){if(!resetting)web.evaluateJavascript("window.re1Lifecycle?.(true)",null);web.onPause();}super.onPause();
    }
    @Override protected void onResume() {super.onResume();paused=false;lastProgress=SystemClock.uptimeMillis();if(web!=null&&!rendererGone){web.onResume();web.evaluateJavascript("window.re1Lifecycle?.(false);window.RE1Updates?.networkChanged()",null);}}
    @Override public void onBackPressed() {
        if(rendererGone){showReport("Graphics process stopped");return;}
        final boolean[] answered={false};
        web.evaluateJavascript("(()=>{if(window.re1OpenSettings){window.re1OpenSettings();return true;}return false;})()",value->{
            answered[0]=true;if(!"true".equals(value))showReport("Launcher recovery");
        });
        // Recovery stays native if JavaScript cannot open settings.
        ui.postDelayed(()->{if(!destroyed&&!answered[0])showReport("Launcher recovery");},1200);
    }
    @Override protected void onDestroy() {
        modCancel.set(true);ModArchive.Import staged=modImport;
        if(staged!=null)try{staged.close();}catch(IOException error){record("Mod staging close: "+error.getMessage());}
        destroyed=true;ui.removeCallbacksAndMessages(null);if(inputs!=null)inputs.unregisterInputDeviceListener(this);
        storage.shutdownNow();if(web!=null&&!rendererGone){web.removeJavascriptInterface("AndroidLauncher");web.destroy();}super.onDestroy();
    }
}
