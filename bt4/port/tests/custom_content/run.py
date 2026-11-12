"""Regression probes for custom content and music tempo, without desktop audio.

SDL3_DIR selects a prebuilt SDL3 prefix on Windows. Else pkg-config is used.
Optional --adx and --textures arguments exercise a user's real resources.
"""
from pathlib import Path
import argparse,json,os,struct,subprocess,sys,tempfile,wave,zipfile,zlib,shutil
root=Path(__file__).resolve().parents[3]
ap=argparse.ArgumentParser();ap.add_argument('--adx',type=Path);ap.add_argument('--textures',type=Path);args=ap.parse_args()
out=root/'port/build/test_custom_content';out.mkdir(parents=True,exist_ok=True)
env=dict(os.environ);cc=env.get('CC','gcc')
if env.get('SDL3_DIR'):
    sdl=Path(env['SDL3_DIR']);flags=['-I'+str(sdl/'include'),'-L'+str(sdl/'lib'),'-lSDL3']
    if (sdl/'bin/SDL3.dll').exists():shutil.copy2(sdl/'bin/SDL3.dll',out/'SDL3.dll')
else:flags=subprocess.check_output(['pkg-config','--cflags','--libs','sdl3'],text=True).split()
def run(command,**kw):
    print(' '.join(map(str,command)),flush=True)
    prefix=['rtk','proxy'] if shutil.which('rtk') else []
    return subprocess.run(prefix+list(map(str,command)),env=env,check=True,**kw)
dead='-Wl,-dead_strip' if sys.platform=='darwin' else '-Wl,--gc-sections'
common=[cc,'-std=gnu99','-O2','-ffunction-sections','-fdata-sections',dead,'-I'+str(root/'port/include')]
suffix='.exe' if os.name=='nt' else ''
def build(name,sources,extra=(),cxx=False):
    options=common if not cxx else [env.get('CXX','g++'),'-std=c++17']+common[2:]
    exe=out/(name+suffix);run(options+[str(root/p) for p in sources]+list(extra)+flags+['-lm','-o',str(exe)]);return exe
audio=build('audio_probe',['port/tests/custom_content/audio_probe.c'])
songs=build('songs_probe',['port/tests/custom_content/songs_probe.c','port/src/plat_songs.c','port/src/plat_extras.c','port/src/plat_file.c'])
pack=build('pack_probe',['port/tests/custom_content/pack_probe.c','port/third_party/miniz/miniz.c'],['-DMINIZ_NO_ARCHIVE_WRITING_APIS','-DMINIZ_NO_STDIO','-DMINIZ_NO_TIME','-DMINIZ_NO_ZLIB_COMPATIBLE_NAMES'])
pack_cache=build('pack_cache_probe',['port/tests/custom_content/pack_cache_probe.c','port/third_party/miniz/miniz.c'],['-DMINIZ_NO_ARCHIVE_WRITING_APIS','-DMINIZ_NO_STDIO','-DMINIZ_NO_TIME','-DMINIZ_NO_ZLIB_COMPATIBLE_NAMES'])
run([str(pack_cache)],cwd=out)
pack_async=build('pack_async_probe',['port/tests/custom_content/pack_async_probe.c','port/third_party/miniz/miniz.c'],['-DMINIZ_NO_ARCHIVE_WRITING_APIS','-DMINIZ_NO_STDIO','-DMINIZ_NO_TIME','-DMINIZ_NO_ZLIB_COMPATIBLE_NAMES'])
run([str(pack_async)],cwd=out,timeout=30)
menu=build('menu_probe',['port/tests/custom_content/menu_probe.cpp'],cxx=True)
run([str(menu)],cwd=out)
menu_shutdown=build('menu_shutdown_probe',['port/tests/custom_content/menu_shutdown_probe.cpp'],cxx=True)
for attempt in range(3):run([str(menu_shutdown)],cwd=out,timeout=30)
def function(path,signature):
    text=(root/path).read_text();start=text.index(signature);at=text.index('{',start)+1;depth=1
    while depth:depth+=(text[at]=='{')-(text[at]=='}');at+=1
    return text[start:at]
(out/'sync_under_test.inc').write_text(function('port/src/gs/gs_core.c','int Gs_SyncPath(')+'\n'+function('port/src/plat_stub.c','int sceGsSyncPath('))
sync=build('sync_probe',['port/tests/custom_content/sync_probe.c'],['-I'+str(out),'-pthread'])
run([str(sync)],cwd=out)
(out/'epoch_under_test.inc').write_text(function('port/src/gs/tag_team_net.c','int Port_TagTakeFrame('))
epoch=build('epoch_probe',['port/tests/custom_content/epoch_probe.c','port/src/tag_team.c'],['-I'+str(out)])
run([str(epoch)],cwd=out)
with tempfile.TemporaryDirectory(prefix='BT4 media test ') as temp:
    temp=Path(temp);songdir=temp/'songs';songdir.mkdir()
    for extension in ('wav','mp3','flac','adx'):
        # Alias resolution uses the content path, independently of decoder type.
        with wave.open(str(songdir/('Test.'+extension)),'wb') as w:w.setparams((2,2,24000,0,'NONE','not compressed'));w.writeframes(b'\0'*400)
    (songdir/'songs.txt').write_text('Test.mp3|First track\n',encoding='utf-8')
    env.update(BT4_CONTENT_ROOT=str(temp),BT3_SONGS='',BT3_DATA=str(temp/'data'))
    run([str(songs)],cwd=out)
    adx=args.adx
    if adx is None:
        adx=temp/'tone.adx';h=bytearray(64);struct.pack_into('>HHBBBBIIHBB',h,0,0x8000,60,3,18,4,1,24000,24000*20,500,4,0)
        adx.write_bytes(h+(b'\x01\0'+bytes.fromhex('12345676543210fedcba98789abcdef0'))*(24000*20//32))
    run([str(audio),str(adx.resolve())],cwd=out)
    textures=temp/'textures';textures.mkdir()
    def chunk(tag,body):return struct.pack('>I',len(body))+tag+body+struct.pack('>I',zlib.crc32(tag+body))
    png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',16,16,8,6,0,0,0))+chunk(b'IDAT',zlib.compress((b'\0'+bytes([180,40,20,128])*16)*16))+chunk(b'IEND',b'')
    (textures/'1-00005dd4.png').write_bytes(png)
    with zipfile.ZipFile(textures/'pack.zip','w',zipfile.ZIP_DEFLATED) as z:z.writestr('nested/2-3-00005dd4.png',png)
    (textures/'broken.zip').write_bytes(b'invalid ZIP')
    run([str(pack),str(textures)],cwd=out)
    if args.textures:run([str(pack),str(args.textures.resolve())],cwd=out)
print('Custom content and music tempo probes passed. No playback device opened.')
