#!/usr/bin/env python3
"""Build the same pinned RTC C API with SCTP and media on native Linux/macOS.

Only shared libdatachannel is installed; its libjuice/usrsctp/libsrtp dependencies
are linked statically. OpenSSL stays a shared native dependency and is included
by the release packager's recursive dependency scan.
"""
from pathlib import Path,PurePosixPath
import argparse,hashlib,json,os,shutil,subprocess,tarfile,urllib.request,zipfile

LOCK=Path(__file__).with_name('rtc-sources.lock.json')

def fetch(entry,cache):
    target=cache/(entry['name']+'-'+entry['commit']+'.tar.gz')
    if not target.is_file():
        with urllib.request.urlopen(entry['url'],timeout=120) as response:
            data=response.read()
        if hashlib.sha256(data).hexdigest()!=entry['sha256']:
            raise RuntimeError('Source digest mismatch: '+entry['name'])
        target.write_bytes(data)
    if hashlib.sha256(target.read_bytes()).hexdigest()!=entry['sha256']:
        raise RuntimeError('Cached source digest mismatch: '+entry['name'])
    return target

def unpack(archive,destination):
    destination.mkdir(parents=True,exist_ok=True)
    with tarfile.open(archive,'r:gz') as package:
        for member in package:
            parts=PurePosixPath(member.name).parts
            if not parts or '..' in parts or PurePosixPath(member.name).is_absolute():
                raise RuntimeError('Unsafe source member: '+member.name)
            relative=Path(*parts[1:])
            if not relative.parts:continue
            target=destination/relative
            if not target.resolve().is_relative_to(destination.resolve()):
                raise RuntimeError('Source member escapes destination: '+member.name)
            if member.isdir():target.mkdir(parents=True,exist_ok=True)
            elif member.isfile():
                target.parent.mkdir(parents=True,exist_ok=True)
                with package.extractfile(member) as source,target.open('wb') as output:
                    shutil.copyfileobj(source,output)
                target.chmod(member.mode&0o777)
            elif member.issym() or member.islnk():
                # No build-required source is a link in the locked archives.
                raise RuntimeError('Source archive contains an unexpected link: '+member.name)

def command(*args):
    subprocess.run(list(map(str,args)),check=True)

def build(prefix,work,openssl=None,jobs=2,download_only=False):
    metadata=json.loads(LOCK.read_text(encoding='utf8'))
    cache=work/'cache';cache.mkdir(parents=True,exist_ok=True)
    archives=[(entry,fetch(entry,cache)) for entry in metadata['sources']]
    if download_only:return
    source=work/'source';stamp=source/'.bt4-rtc-lock'
    stamp_value=hashlib.sha256(LOCK.read_bytes()).hexdigest()
    if not stamp.is_file() or stamp.read_text()!=stamp_value:
        if source.exists():
            assert source.resolve().is_relative_to(work.resolve()) and not source.is_symlink()
            shutil.rmtree(source)
        for entry,archive in archives:unpack(archive,source/entry['destination'])
        stamp.write_text(stamp_value)
    args=['-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DCMAKE_INSTALL_PREFIX='+str(prefix),
          '-DCMAKE_INSTALL_LIBDIR=lib','-DCMAKE_POLICY_VERSION_MINIMUM=3.5',
          '-DBUILD_SHARED_LIBS=ON','-DBUILD_SHARED_DEPS_LIBS=OFF','-DPREFER_SYSTEM_LIB=OFF',
          '-DUSE_SYSTEM_JUICE=OFF','-DUSE_SYSTEM_USRSCTP=OFF','-DUSE_SYSTEM_SRTP=OFF',
          '-DNO_WEBSOCKET=ON','-DNO_MEDIA=OFF','-DNO_EXAMPLES=ON','-DNO_TESTS=ON',
          '-DWARNINGS_AS_ERRORS=OFF','-DBUILD_WITH_WARNINGS=OFF',
          '-DCMAKE_POSITION_INDEPENDENT_CODE=ON']
    if openssl:args+=['-DOPENSSL_ROOT_DIR='+str(openssl)]
    if os.uname().sysname=='Darwin':args+=['-DCMAKE_OSX_ARCHITECTURES=arm64','-DCMAKE_OSX_DEPLOYMENT_TARGET=15.0']
    command('cmake','-S',source,'-B',work/'build',*args)
    command('cmake','--build',work/'build','--parallel',str(jobs))
    command('cmake','--install',work/'build')
    notices=prefix/'share/licenses/BT4-RTC';notices.mkdir(parents=True,exist_ok=True)
    for relative,target in [('LICENSE','libdatachannel-MPL-2.0.txt'),
                            ('deps/libjuice/LICENSE','libjuice-MPL-2.0.txt'),
                            ('deps/libsrtp/LICENSE','libsrtp-BSD.txt'),
                            ('deps/plog/LICENSE','plog-MIT.txt'),
                            ('deps/usrsctp/LICENSE.md','usrsctp-BSD.txt')]:
        shutil.copy2(source/relative,notices/target)
    shutil.copy2(LOCK,notices/'SOURCES.json')
    receipt=dict(source_lock_sha256=stamp_value,cmake_args=args,
                 cmake_version=subprocess.check_output(['cmake','--version'],text=True).splitlines()[0],
                 openssl_version=subprocess.check_output(['openssl','version'],text=True).strip())
    (notices/'BUILD.json').write_text(json.dumps(receipt,indent=2)+'\n')
    # Keep the compiled MPL sources and submodules alongside the binaries.
    # Disabled example videos and tests are omitted; complete original archives
    # remain reproducible through the pinned URLs and digests in SOURCES.json.
    with zipfile.ZipFile(notices/'libdatachannel-build-source.zip','w',zipfile.ZIP_DEFLATED,compresslevel=9) as package:
        for file in sorted(source.rglob('*')):
            if not file.is_file():continue
            relative=file.relative_to(source)
            if any(part in {'.git','.github','examples','test','tests'} for part in relative.parts):continue
            if relative.name=='.bt4-rtc-lock':continue
            package.write(file,'libdatachannel/'+relative.as_posix())
    print('RTC SDK built with SCTP, media and locked submodules: '+str(prefix))

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prefix',type=Path,default=Path('/usr/local'))
    parser.add_argument('--work',type=Path,default=Path('/tmp/bt4-native-rtc'))
    parser.add_argument('--openssl-prefix',type=Path)
    parser.add_argument('--jobs',type=int,default=2)
    parser.add_argument('--download-only',action='store_true')
    args=parser.parse_args()
    build(args.prefix.resolve(),args.work.resolve(),args.openssl_prefix,args.jobs,args.download_only)
