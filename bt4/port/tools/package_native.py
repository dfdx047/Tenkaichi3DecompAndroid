"""Native release folders with bundled SDL, TLS transport and Opus libraries."""
import json,os,pathlib,re,shutil,subprocess,sys,zipfile,struct

def require_mac_pagezero(executable):
    """Reject the startup failure fixed by upstream 0.1.39.2 before packaging."""
    data=pathlib.Path(executable).read_bytes()
    if len(data)<32 or struct.unpack_from('<I',data)[0]!=0xFEEDFACF:
        raise RuntimeError('Expected a 64-bit Mach-O runtime')
    count=struct.unpack_from('<I',data,16)[0];offset=32
    for _ in range(count):
        if offset+8>len(data):break
        command,size=struct.unpack_from('<II',data,offset)
        if size<8 or offset+size>len(data):break
        if command==0x19 and size>=72 and data[offset+8:offset+24].rstrip(b'\0')==b'__PAGEZERO':
            address,length=struct.unpack_from('<QQ',data,offset+24)
            if address==0 and length>=0x100000000:return
            break
        offset+=size
    raise RuntimeError('The macOS runtime needs a 4 GiB __PAGEZERO segment; rebuild with the current linker')

def run(*args):
    return subprocess.check_output(list(map(str,args)),text=True).strip()

def libraries_linux(out):
    search=['/usr/local/lib','/usr/lib/x86_64-linux-gnu','/usr/lib64','/usr/lib']
    def find(name):
        return next((pathlib.Path(d)/name for d in search if (pathlib.Path(d)/name).exists()),None)
    seeds=[find(name) for name in ('libSDL3.so.0','libcurl.so.4','libopus.so.0','libdatachannel.so')]
    if any(p is None for p in seeds):raise RuntimeError('SDL3, WebSocket-enabled libcurl, Opus and the RTC SCTP SDK are required')
    queue=[out/'BT4decomp',out/'BT4decomp-setup',*seeds];done=set();sources={}
    core=re.compile(r'^(?:ld-linux|lib(?:c|m|pthread|dl|rt|resolv|gcc_s|stdc\+\+)\.so)')
    while queue:
        file=queue.pop(0)
        if str(file) in done:continue
        done.add(str(file))
        deps=[]
        for line in run('ldd',file).splitlines():
            match=re.match(r'\s*(\S+) => (/\S+)',line)
            if 'not found' in line:raise RuntimeError(str(file)+': '+line)
            if match and not core.match(match[1]):deps.append((match[1],pathlib.Path(match[2])))
        if file in seeds:deps.append((file.name,file))
        for name,source in deps:
            if name in sources:continue
            sources[name]=str(source.resolve());shutil.copy2(source.resolve(),out/'lib'/name);queue.append(source)
    for file in (out/'lib').iterdir():
        if file.suffix!='.pem':subprocess.run(['patchelf','--set-rpath','$ORIGIN',str(file)],check=True)
    return sources

def libraries_mac(out):
    from mac_dependencies import collect_dependencies,verify_dependencies
    seeds=[]
    for env,default,name in [('BT3_SDL3','/opt/homebrew/opt/sdl3','libSDL3.0.dylib'),
                              ('BT3_CURL','/opt/homebrew/opt/curl','libcurl.4.dylib'),
                              ('BT3_OPUS','/opt/homebrew/opt/opus','libopus.0.dylib'),
                              ('BT3_RTC','/usr/local','libdatachannel.dylib')]:
        source=pathlib.Path(os.environ.get(env,default))/'lib'/name
        if not source.exists():raise RuntimeError('Missing native library: '+str(source))
        seeds.append(source)
    executables=[out/'BT4decomp',out/'BT4decomp-setup']
    sources,changes=collect_dependencies(executables,seeds)
    for name,source in sources.items():shutil.copy2(source,out/'lib'/name)
    destinations=[(file.resolve(),file) for file in executables]
    destinations.extend((source,out/'lib'/name) for name,source in sources.items())
    for original,dest in destinations:
        for name,alias in changes[original]:
            relative=('@executable_path/lib/' if dest.parent==out else '@loader_path/')+alias
            subprocess.run(['install_name_tool','-change',name,relative,str(dest)],check=True)
        if dest.parent!=out:subprocess.run(['install_name_tool','-id','@rpath/'+dest.name,str(dest)],check=True)
    verify_dependencies(out)
    for file in [*sorted((out/'lib').glob('*.dylib')),out/'BT4decomp',out/'BT4decomp-setup']:
        subprocess.run(['codesign','--force','--sign','-',str(file)],check=True)
    return {name:str(source) for name,source in sources.items()}

def main(p,mac=False):
    root=p.ROOT;out=root/'port/build'/('release-mac' if mac else 'release')
    archive=root/'port/build'/('BT4decomp-macos-arm64.zip' if mac else 'BT4decomp-linux-x64.zip')
    exe=root/'port/build'/('bt3_mac' if mac else 'bt3_64');setup=root/'BT4decomp-setup'
    if not exe.is_file() or not setup.is_file():raise RuntimeError('Build the native runtime and setup first')
    if mac:require_mac_pagezero(exe)
    if out.exists():
        assert out.resolve().is_relative_to((root/'port/build').resolve()) and not out.is_symlink()
        shutil.rmtree(out)
    p.take_program(exe,out);p.take_lang(out);p.take_translations(out)
    shutil.copy2(setup,out/'BT4decomp-setup');shutil.copy2(root/'port/release/BT4_VERSION.txt',out/'BT4_VERSION.txt')
    for name in ('lib','licenses','textures','stages','songs','mods'):(out/name).mkdir()
    licenses=out/'licenses'
    for source,target in [('LICENSE','bt4decomp-agpl.txt'),('LICENSE-MIT','bt4decomp-mit.txt'),
      ('port/third_party/imgui/LICENSE.txt','dear-imgui.txt'),('port/third_party/newlib_libm/COPYING.NEWLIB','newlib.txt'),
      ('port/third_party/xxhash/LICENSE','xxhash.txt'),('port/third_party/miniaudio/LICENSE','miniaudio.txt'),
      ('port/third_party/miniz/LICENSE','miniz.txt'),('port/third_party/lua/LICENSE','lua.txt'),('port/third_party/monocypher/LICENCE.md','monocypher.txt'),
      ('port/third_party/voice/LICENSE-opus','opus.txt')]:shutil.copy2(root/source,licenses/target)
    (licenses/'sdl3.txt').write_text('SDL3: zlib license. https://www.libsdl.org/license.php\n')
    (licenses/'curl.txt').write_text('curl license: https://curl.se/docs/copyright.html\n')
    p.take_fonts(out,licenses);p.take_tbo_art(out,licenses)
    p.take_example_mods(out)
    for name,note in [('textures',p.TEXTURES_NOTE),('stages',p.STAGES_NOTE),('songs',p.SONGS_NOTE),('mods',p.MODS_NOTE)]:
        (out/name/'README.txt').write_text(note,encoding='utf8')
    readme=p.README.replace('(Linux, 64-bit)','(macOS, Apple Silicon arm64)' if mac else '(Linux, x86-64)')
    readme+='\nThe native package includes RTC media and SCTP DataChannels. TLS WebSocket transport remains available for relay connections.\n'
    if mac:readme+='This macOS package requires macOS 15 or later on Apple Silicon. It is ad-hoc signed, not notarized. Allow the downloaded application in macOS Privacy & Security if Gatekeeper blocks it. Intel Macs are not supported by this arm64 build.\n'
    (out/'README.txt').write_text(readme,encoding='utf8')
    sources=libraries_mac(out) if mac else libraries_linux(out)
    (licenses/'native-libraries.json').write_text(json.dumps(sources,indent=2))
    notices=licenses/'native';notices.mkdir()
    rtc_notices=pathlib.Path(os.environ.get('BT3_RTC','/usr/local'))/'share/licenses/BT4-RTC'
    if not (rtc_notices/'SOURCES.json').is_file():raise RuntimeError('Pinned RTC source and license notices are missing')
    shutil.copytree(rtc_notices,notices/'BT4-RTC',dirs_exist_ok=True)
    if mac:
        # Homebrew retains the installed source recipe, receipt and upstream
        # notices in each keg. Include them with every redistributed library.
        seen=set()
        for source in sources.values():
            path=pathlib.Path(source)
            if '/Cellar/' not in source:continue
            cellar=path.parts.index('Cellar');keg=pathlib.Path(*path.parts[:cellar+3])
            if keg in seen:continue
            seen.add(keg);dest=notices/keg.parent.name;dest.mkdir(exist_ok=True)
            for file in [*keg.glob('*'),*keg.glob('.brew/*.rb'),*keg.glob('share/doc/*/*'),*keg.glob('share/licenses/*/*')]:
                if file.is_file() and (re.search(r'COPYING|LICENSE|LICENCE|COPYRIGHT|NOTICE|INSTALL_RECEIPT',file.name,re.I) or file.suffix=='.rb'):
                    shutil.copy2(file,dest/file.name)
    else:
        for source in sources.values():
            found=subprocess.run(['dpkg-query','-S',source],capture_output=True,text=True)
            if found.returncode==0:
                package=found.stdout.split(': ',1)[0].split(':',1)[0]
                file=pathlib.Path('/usr/share/doc')/package/'copyright'
                if file.is_file():shutil.copy2(file,notices/(package+'-copyright.txt'))
        for kind in ['SDL3','curl']:
            source=pathlib.Path('/usr/local/share/licenses')/kind
            if source.is_dir():shutil.copytree(source,notices/kind,dirs_exist_ok=True)
    # Public CA certificates, never private account material.
    ca=next((pathlib.Path(x) for x in ['/etc/ssl/certs/ca-certificates.crt','/opt/homebrew/etc/ca-certificates/cert.pem','/etc/ssl/cert.pem'] if pathlib.Path(x).is_file()),None)
    if ca:shutil.copy2(ca,out/'lib/cacert.pem')
    for file in [out/'BT4decomp',out/'BT4decomp-setup',*(out/'lib').glob('*')]:
        if file.suffix!='.pem':file.chmod(0o755)
    if not mac:p.portable(out)
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for file in sorted(out.rglob('*')):
            if file.is_file():z.write(file,'BT4decomp/'+file.relative_to(out).as_posix())
    print(str(archive.relative_to(root))+': '+str(archive.stat().st_size)+' bytes')
