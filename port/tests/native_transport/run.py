"""Run production POSIX transports against local RFC 6455 fixtures, without game data."""
from pathlib import Path
import base64,hashlib,json,os,shlex,socket,ssl,struct,subprocess,tempfile,threading
ROOT=Path(__file__).resolve().parents[3]

def frame(data,opcode=2,final=True):
    head=bytes([(0x80 if final else 0)|opcode])
    return head+(bytes([len(data)]) if len(data)<126 else b'\x7e'+struct.pack('!H',len(data)))+data

def receive(sock):
    def read(n):
        result=b''
        while len(result)<n:
            part=sock.recv(n-len(result))
            if not part:raise EOFError()
            result+=part
        return result
    a,b=read(2);length=b&127
    if length==126:length=struct.unpack('!H',read(2))[0]
    if length==127:length=struct.unpack('!Q',read(8))[0]
    if length>32768:raise ValueError('oversized fixture frame')
    mask=read(4) if b&128 else None;data=read(length)
    if mask:data=bytes(c^mask[i%4] for i,c in enumerate(data))
    return a&15,data

def fixture(exe,case,context=None):
    listener=socket.socket();listener.bind(('127.0.0.1',0));listener.listen(1);listener.settimeout(12)
    port=listener.getsockname()[1];errors=[]
    def serve():
        try:
            connection,_=listener.accept();connection.settimeout(10)
            if context:
                try:connection=context.wrap_socket(connection,server_side=True)
                except ssl.SSLError:connection.close();return
            with connection:
                request=b''
                while b'\r\n\r\n' not in request:
                    chunk=connection.recv(4096)
                    if not chunk:raise EOFError()
                    request+=chunk
                    if len(request)>16384:raise ValueError('oversized handshake')
                lines=request.decode().split('\r\n');headers={k.lower():v.strip() for k,v in (line.split(':',1) for line in lines[1:] if ':' in line)}
                if case=='voice':
                    assert headers.get('authorization')=='Bearer '+'a'*64
                    assert headers.get('x-bt4-seat')=='0' and headers.get('x-bt4-name')=='NauTaro%20Test'
                accept=base64.b64encode(hashlib.sha1((headers['sec-websocket-key']+'258EAFA5-E914-47DA-95CA-C5AB0DC85B11').encode()).digest())
                connection.sendall(b'HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: '+accept+b'\r\n\r\n')
                count=0
                while count<2:
                    opcode,data=receive(connection)
                    if opcode==10:continue
                    if case=='invalid':connection.sendall(frame(b'tiny'));return
                    if count==0:
                        middle=len(data)//2
                        connection.sendall(frame(data[:middle],opcode,False)+frame(b'fixture',9)+frame(data[middle:],0))
                    else:connection.sendall(frame(data,opcode))
                    count+=1
                # Keep the connection open until the client has consumed the echo.
                try:
                    while connection.recv(1024):pass
                except (ConnectionError,socket.timeout):pass
        except (EOFError,ConnectionError,ssl.SSLError):
            if case not in ('invalid','tls-reject'):errors.append('unexpected connection close')
        except Exception as error:errors.append(repr(error))
        finally:listener.close()
    thread=threading.Thread(target=serve,daemon=True);thread.start()
    url=('https' if context else 'http')+'://127.0.0.1:'+str(port)
    p=subprocess.run([str(exe),url]+(['reject'] if case in ('invalid','tls-reject') else []),capture_output=True,text=True,timeout=15)
    thread.join(2)
    assert p.returncode==0,(case,p.returncode,p.stdout,p.stderr)
    assert not errors,errors
    print(json.dumps(dict(case=case,passed=True,game_data=False,production_transport=True)),flush=True)

def main():
    if os.name=='nt':raise SystemExit('Run this test on native Linux or macOS.')
    env=dict(os.environ)
    prefix=os.environ.get('BT3_CURL')
    if prefix:env['PKG_CONFIG_PATH']=prefix+'/lib/pkgconfig'+os.pathsep+env.get('PKG_CONFIG_PATH','')
    flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','sdl3','libcurl'],env=env,text=True))
    with tempfile.TemporaryDirectory() as directory:
        out=Path(directory);tag=out/'tag-probe';voice=out/'voice-probe'
        for compiler,source,target,standard in [('cc','tag_probe.c',tag,'-std=c99'),('c++','voice_probe.cpp',voice,'-std=c++17')]:
            subprocess.run([compiler,standard,'-O2',str(Path(__file__).parent/source),'-o',str(target),*flags],env=env,check=True)
        fixture(tag,'fragmented');fixture(tag,'invalid');fixture(voice,'voice')
        subprocess.run(['openssl','req','-x509','-newkey','rsa:2048','-nodes','-days','1','-subj','/CN=localhost',
                        '-keyout',str(out/'key.pem'),'-out',str(out/'cert.pem')],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,check=True)
        context=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER);context.load_cert_chain(out/'cert.pem',out/'key.pem')
        fixture(tag,'tls-reject',context)
        audio=out/'audio-probe'
        subprocess.run(['c++','-std=c++17','-O2',str(Path(__file__).parent/'audio_probe.cpp'),'-o',str(audio),*flags],env=env,check=True)
        subprocess.run([str(audio)],env=dict(env,SDL_AUDIO_DRIVER='dummy'),timeout=5,check=True)
        print(json.dumps(dict(case='sdl-audio-dummy',passed=True,physical_audio=False)),flush=True)
        update=out/'update-probe'
        subprocess.run(['cc','-std=c99','-O2',str(ROOT/'port/tests/update/test_update.c'),
            *[str(ROOT/'port/src/gs'/name) for name in ['update_sign.c','update_crypto.c','update_monocypher.c']],
            '-o',str(update),*flags],env=env,check=True)
        subprocess.run([str(update)],check=True,timeout=15)
if __name__=='__main__':main()
