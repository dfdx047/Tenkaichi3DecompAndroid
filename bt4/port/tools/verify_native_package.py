"""Check native release layout, architecture, library closure and execute bits."""
from pathlib import Path
import os,subprocess,sys,zipfile
root=Path(__file__).resolve().parents[2];mac=sys.argv[1]=='mac'
out=root/'port/build'/('release-mac' if mac else 'release')
archive=root/'port/build'/('BT4decomp-macos-arm64.zip' if mac else 'BT4decomp-linux-x64.zip')
for name in ['BT4decomp','BT4decomp-setup','BT4decomp.dat','BT4decomp.mem','BT4_VERSION.txt']:
    assert (out/name).is_file(),name
rtc_library=out/'lib'/('libdatachannel.dylib' if mac else 'libdatachannel.so')
assert rtc_library.is_file(),'The native RTC SCTP library must be bundled'
if mac:
    from mac_dependencies import verify_dependencies
    import ctypes
    for library in verify_dependencies(out):
        subprocess.run(['codesign','--verify',str(library)],check=True)
        if library.suffix=='.dylib':ctypes.CDLL(str(library))
for name in ['SOURCES.json','BUILD.json','libdatachannel-build-source.zip','libdatachannel-MPL-2.0.txt',
             'libjuice-MPL-2.0.txt','libsrtp-BSD.txt','plog-MIT.txt','usrsctp-BSD.txt']:
    assert (out/'licenses/native/BT4-RTC'/name).is_file(),name
for name in ['BT4decomp','BT4decomp-setup']:
    file=out/name;assert os.access(file,os.X_OK)
    description=subprocess.check_output(['file',str(file)],text=True)
    assert ('arm64' if mac else 'x86-64') in description,description
    if mac:
        subprocess.run(['codesign','--verify',str(file)],check=True)
        deps=subprocess.check_output(['otool','-L',str(file)],text=True)
        assert '/opt/homebrew/' not in deps and '/usr/local/' not in deps,deps
    else:
        deps=subprocess.check_output(['ldd',str(file)],text=True);assert 'not found' not in deps,deps
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None
    assert all(n.startswith('BT4decomp/') and '..' not in Path(n).parts for n in z.namelist())
    assert not any('/gamedata/' in n or '/saves/' in n or n.endswith('.iso') for n in z.namelist())
    assert z.getinfo('BT4decomp/BT4decomp').external_attr>>16&0o111
subprocess.run([sys.executable,str(root/'port/tests/native_rtc/run.py'),'--library',str(rtc_library)],check=True)
print(('macOS arm64' if mac else 'Linux x86-64')+' package: layout, architecture, executable bits and linked dependencies passed')
