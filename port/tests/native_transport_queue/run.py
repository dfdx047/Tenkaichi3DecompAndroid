"""Compile and execute the unchanged POSIX VoiceWs worker with deterministic I/O."""
import argparse
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cxx', default=os.environ.get('CXX', 'c++'))
    options=parser.parse_args()
    here=Path(__file__).resolve().parent
    source=here.parents[1]/'src/gs/voice_transport_curl.inc'
    # Private access changes only; the entire production run/send/read code is
    # copied verbatim. No network, SDL process state or installed assets needed.
    production=source.read_text().replace('class VoiceWs {', 'struct VoiceWs {', 1)
    environment=os.environ.copy()
    compiler=shlex.split(options.cxx, posix=os.name!='nt')
    compiler=[value.strip('"') for value in compiler]
    resolved=shutil.which(compiler[0]) or compiler[0]
    environment['PATH']=str(Path(resolved).resolve().parent)+os.pathsep+environment.get('PATH','')
    flags={'creationflags': subprocess.CREATE_NO_WINDOW} if os.name=='nt' else {}
    with tempfile.TemporaryDirectory(prefix='bt4-native-transport-') as directory:
        temporary=Path(directory)
        (temporary/'production_voice_ws.inc').write_text(production)
        for name in ('curl_ws_io.h','voice_ws_fixture.cpp'):
            shutil.copy2(here/name,temporary/name)
        executable=temporary/('fixture.exe' if os.name=='nt' else 'fixture')
        subprocess.run(compiler+['-std=c++17','-O2',str(temporary/'voice_ws_fixture.cpp'),'-o',str(executable)],
                       check=True,env=environment,**flags)
        result=subprocess.run([str(executable)],check=True,timeout=3,env=environment,
                              capture_output=True,text=True,**flags)
        print(result.stdout,end='')


if __name__=='__main__':
    main()
