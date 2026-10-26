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
    # Legacy description encodings must not block otherwise supported packs.
    # The released APK still receives UTF-8 credits, and the original bytes
    # remain in notices/. Game XML/media and the input folder stay untouched.
    descriptions=[
        ('Windows ANSI', 'Traducción al español. Autor: José.\r\n', 'cp1252', []),
        ('Polish ANSI', 'Pełne tłumaczenie. Autor: Łukasz.\r\n', 'cp1250', ['--description-encoding','windows-1250']),
        ('UTF16 LE', 'Créditos: José y Łukasz.\r\n', 'utf-16', []),
        ('UTF16 BE', 'Créditos: José y Łukasz.\r\n', 'utf-16-be', []),
        ('Explicit UTF16', 'Créditos: José.\r\n', 'utf-16-le', ['--description-encoding','utf-16le']),
    ]
    for label,text,encoding,options in descriptions:
        original=text.encode(encoding)
        if label=='UTF16 BE':original=b'\xfe\xff'+original
        (mod/'description.txt').write_bytes(original)
        output=base/(label+'.zip')
        run('rebirth',download,'--out',output,*options)
        with zipfile.ZipFile(output) as z:
            assert z.testzip() is None
            assert z.read('description.txt').decode('utf-8')==text
            assert z.read('notices/original-description.txt')==original
            for name in expected:
                if name!='description.txt':assert z.read(name)==(mod/name).read_bytes(),name
            assert z.read('Movie/pu.mp4')==(download/'Movie/pu.mp4').read_bytes()
        assert (mod/'description.txt').read_bytes()==original
        report=output.with_suffix('.preparation.txt').read_text(encoding='utf-8')
        assert 'Converted description.txt' in report
        if label=='Windows ANSI':assert 'assuming Windows-1252' in report
    failed=base/'bad encoding.zip'
    run('rebirth',download,'--out',failed,'--description-encoding','unknown-codepage',ok=False)
    assert not failed.exists() and not failed.with_suffix('.preparation.txt').exists()
    (mod/'description.txt').write_bytes(b'\xe9'*(2*1024*1024))
    result=run('rebirth',download,'--out',failed,ok=False)
    assert 'after UTF-8 conversion' in result.stderr and not failed.exists()
    (mod/'description.txt').write_bytes(before[pathlib.Path('translation files/description.txt')])
    # UTF-8 credits, including their BOM, are copied exactly as before.
    bom=b'\xef\xbb\xbfCr\xc3\xa9ditos de autor\r\n';(mod/'description.txt').write_bytes(bom)
    output=base/'BOM.zip';run('rebirth',download,'--out',output)
    with zipfile.ZipFile(output) as z:
        assert z.read('description.txt')==bom and 'notices/original-description.txt' not in z.namelist()
    (mod/'description.txt').write_bytes(before[pathlib.Path('translation files/description.txt')])
    # Encoding fallback applies only to metadata, never translated game XML.
    (mod/'misc.xml').write_bytes('<Strings><Text>español</Text></Strings>'.encode('cp1252'))
    result=run('rebirth',download,'--out',failed,ok=False)
    assert 'misc.xml must be UTF-8' in result.stderr and not failed.exists()
    (mod/'misc.xml').write_bytes(before[pathlib.Path('translation files/misc.xml')])
    # A description fix must not hide a genuinely missing required font.
    atlas=(mod/'Data/PBFONT0.tim').read_bytes();(mod/'Data/PBFONT0.tim').unlink()
    result=run('rebirth',download,'--out',failed,ok=False)
    assert 'does not include Data/PBFONT0.tim' in result.stderr and not failed.exists()
    (mod/'Data/PBFONT0.tim').write_bytes(atlas)
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
print('PASS: Windows-1252/1250 and UTF-16 description normalization, exact original credits retained, UTF-8 BOM preservation, strict XML validation and missing-font rejection')
