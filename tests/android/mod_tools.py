"""Run the beginner authoring commands on synthetic TIMs; independently read ZIP/PNG."""
import json
import pathlib
import struct
import subprocess
import tempfile
import zipfile
import zlib

ROOT=pathlib.Path(__file__).resolve().parents[2]
TOOL=ROOT/'tools/mods/pack.js'
with tempfile.TemporaryDirectory(prefix='re1 mod tools ') as temp:
    base=pathlib.Path(temp);game=base/'My Game/USA';(game/'Data').mkdir(parents=True)
    tim=struct.pack('<II',16,2)+struct.pack('<I4H',44,0,0,4,4)+struct.pack('<16H',*([31]*16))
    (game/'Data/TEST.TIM').write_bytes(tim)
    work=base/'My Pack'
    def run(*args,ok=True):
        result=subprocess.run(['node',str(TOOL),*map(str,args)],capture_output=True,text=True)
        assert (result.returncode==0)==ok,(result.stdout,result.stderr)
        return result
    run('new',work,'--name','My first pack','--author','Test author')
    run('texture',work,'--game',game,'--source','data/test.tim')
    manifest=json.loads((work/'mod.json').read_text());definition=manifest['textures'][0]
    png=(work/definition['file']).read_bytes();at=8;parts=[]
    while at<len(png):
        size=struct.unpack_from('>I',png,at)[0];kind=png[at+4:at+8];data=png[at+8:at+8+size]
        assert zlib.crc32(kind+data)==struct.unpack_from('>I',png,at+8+size)[0]
        if kind==b'IDAT':parts.append(data)
        at+=12+size
    assert zlib.decompress(b''.join(parts))==(b'\0'+bytes([255,0,0,255])*4)*4
    run('texture',work,'--game',game,'--source','data/test.tim',ok=False)
    (work/'unreferenced-private.txt').write_text('not part of the mod')
    (work/'files/Sound').mkdir(parents=True);(work/'files/Sound/gun01.wav').write_bytes(b'synthetic audio')
    run('build',work)
    with zipfile.ZipFile(str(work)+'.zip') as z:
        assert z.testzip() is None
        assert set(z.namelist())=={'mod.json',definition['file'],'files/Sound/gun01.wav'}
        assert z.read(definition['file'])==png
        assert z.read('files/Sound/gun01.wav')==b'synthetic audio'
    manifest['textures'][0]['file']='../outside.png';(work/'mod.json').write_text(json.dumps(manifest));run('build',work,ok=False)

    # Prepare a nested translation download while retaining the author's file
    # bytes and credits. The archive is inspected independently of the writer.
    download=base/'Polish download';mod=download/'translation files'
    (mod/'Data').mkdir(parents=True);(mod/'text').mkdir();(mod/'file').mkdir();(download/'Movie').mkdir()
    (mod/'manifest.txt').write_text('[MOD]\nName = Test translation\nUseRdtXml = 1\n')
    (mod/'encoding.xml').write_text('<Encoding width="8"><Entry Encode="0" Char=" "/><Entry Encode="27" Char="?"/><Entry Encode="29" Char="A"/><Entry Encode="95" Char="ą"/></Encoding>',encoding='utf-8')
    (mod/'misc.xml').write_text('<Strings><Text>Aą</Text></Strings>',encoding='utf-8')
    (mod/'text/ROOM_106.xml').write_text('<Strings><Text>{scroll 0}A{scroll 1}{timed 40}</Text></Strings>')
    (mod/'Data/PBFONT0.tim').write_bytes(struct.pack('<II I4H',16,2,131084,640,0,256,256)+struct.pack('<H',32767)*65536)
    (mod/'file/FILEM_L00.png').write_bytes(png)
    (mod/'description.txt').write_text('Original translation author credits\n')
    (mod/'LICENSE').write_text('Keep this notice\n')
    (mod/'Readme — tłumaczenie.txt').write_text('Author instructions retained as text\n',encoding='utf-8')
    (download/'README.txt').write_text('Outer author credits\n')
    (download/'Movie/pu.mp4').write_bytes(b'unchanged video fixture')
    for name in ['ddraw.dll','install.bat','config.xml','save2.xml','Data/status.tim.backup']:(mod/name).write_bytes(b'not part of the Android pack')
    (download/'mod_translation.7z').write_bytes(b'already extracted archive fixture')
    (download/'unrelated-private.txt').write_text('do not include')
    before={p.relative_to(download):p.read_bytes() for p in download.rglob('*') if p.is_file()}
    prepared=base/'Translation Android.zip'
    result=run('rebirth',download,'--out',prepared)
    assert 'text entries checked' in result.stdout
    report=prepared.with_suffix('.preparation.txt').read_text(encoding='utf-8')
    for name in ['ddraw.dll','config.xml','save2.xml','mod_translation.7z','unrelated-private.txt']:assert name in report
    expected=['manifest.txt','encoding.xml','misc.xml','text/ROOM_106.xml','Data/PBFONT0.tim','file/FILEM_L00.png','description.txt','LICENSE','Readme — tłumaczenie.txt']
    with zipfile.ZipFile(prepared) as z:
        assert z.testzip() is None
        assert set(z.namelist())==set(expected+['notices/README.txt','Movie/pu.mp4'])
        for name in expected:assert z.read(name)==(mod/name).read_bytes(),name
        assert z.read('notices/README.txt')==(download/'README.txt').read_bytes()
        assert z.read('Movie/pu.mp4')==(download/'Movie/pu.mp4').read_bytes()
    assert before=={p.relative_to(download):p.read_bytes() for p in download.rglob('*') if p.is_file()}
    unchanged=prepared.read_bytes();run('rebirth',download,'--out',prepared,ok=False);assert prepared.read_bytes()==unchanged
    run('rebirth',download,'--out',prepared,'--overwrite')
    run('rebirth',download,'--out',mod/'inside.zip',ok=False)
    assert not (mod/'inside.zip').exists()
    run('rebirth',prepared,ok=False)
    extra=base/'Extra Movies';extra.mkdir();(extra/'eu4.mp4').write_bytes(b'extra ending fixture');(extra/'run.exe').write_bytes(b'excluded')
    separate=base/'Separate.zip';run('rebirth',mod,'--movies',extra,'--out',separate)
    with zipfile.ZipFile(separate) as z:assert z.read('Movie/eu4.mp4')==b'extra ending fixture' and not any(n.endswith('.exe') for n in z.namelist())
    (mod/'Movie').mkdir();(mod/'Movie/PU.mp4').write_bytes(b'conflicting movie')
    failed=base/'failed.zip';run('rebirth',download,'--out',failed,ok=False);assert not failed.exists()
    (mod/'Movie/PU.mp4').unlink();(mod/'Movie').rmdir()
    (mod/'misc.xml').write_text('<!DOCTYPE Strings><Strings><Text>A</Text></Strings>')
    run('rebirth',download,'--out',failed,ok=False);assert not failed.exists()
    (mod/'misc.xml').write_bytes(before[pathlib.Path('translation files/misc.xml')])
    (mod/'linked.txt').symlink_to(download/'unrelated-private.txt')
    run('rebirth',download,'--out',failed,ok=False);assert not failed.exists();(mod/'linked.txt').unlink()
    alias=base/'alias';alias.symlink_to(mod,target_is_directory=True)
    run('rebirth',download,'--out',alias/'inside.zip',ok=False);assert not (mod/'inside.zip').exists()
    trap=base/'trap.zip';trap_report=trap.with_suffix('.preparation.txt');trap_report.symlink_to(mod/'misc.xml')
    saved=(mod/'misc.xml').read_bytes();run('rebirth',download,'--out',trap,'--overwrite',ok=False)
    assert (mod/'misc.xml').read_bytes()==saved and not trap.exists();trap_report.unlink()
    trap_report.hardlink_to(mod/'misc.xml')
    run('rebirth',download,'--out',trap,'--overwrite');assert (mod/'misc.xml').read_bytes()==saved
print('PASS: beginner new/export/build commands, paths with spaces, case-insensitive game lookup, lossless PNG bytes, readable ZIP CRCs, private-file exclusion and traversal rejection')
print('PASS: Classic REbirth preparation, nested folders, automatic/separate movies, UTF-8 names/credits, unchanged source bytes, exclusion report, overwrite protection, duplicate/DTD/symlink/output-path rejection')
