"""Real native host: ten player seats, ten spectators, and lobby role changes."""
import argparse
import os
import pathlib
import re
import socket
import struct
import subprocess
import time

p=argparse.ArgumentParser()
p.add_argument('--port',type=int,default=17015)
p.add_argument('--gpu',action='store_true')
p.add_argument('--window',default='800x600')
a=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[2]
out=root/'port/build/tag-online/room-roles'
out.mkdir(parents=True,exist_ok=True)
env={k:v for k,v in os.environ.items() if not k.startswith('BT3_')}
env.update(BT3_TAG_TEAM='1',BT3_TAG_ONLINE='1',BT3_TAG_HOST=str(a.port),
           BT3_DATA=str(root/'gamedata-bt4'),BT3_UPDATE_CHECK='0',BT3_NOSOUND='1',
           BT3_SAVES=str(out/'saves'),BT3_SETTINGS=str(out/'settings.txt'))
if a.gpu:
    shot=out/('room-'+a.window+'.ppm')
    shot.unlink(missing_ok=True)
    env.update(BT3_GS='gpu',BT3_WINDOW=a.window,
               BT3_UI_SHOT_MS='7000:'+str(shot))
logpath=out/'host.log'
log=logpath.open('w')
process=subprocess.Popen([str(root/'port/build/bt3.exe')],cwd=root,env=env,stdout=log,stderr=log)
clients=[]

class Client:
    def __init__(self,playing,bad=False):
        self.s=socket.create_connection(('127.0.0.1',a.port),timeout=5)
        self.buf=b'';self.c=None;self.ready=[];self.role=None;self.notice=None;self.rejected=None
        self.closed=False;self.s.setblocking(False);clients.append(self)
        self.send(1,struct.pack('!9I',0x544e4554,4,1,identity>>32,
                  (identity&0xffffffff)^int(bad),1 if playing else 2,
                  data_identity>>32,data_identity&0xffffffff,0))
    def send(self,kind,data=b''):
        self.s.sendall(struct.pack('!II',len(data)+8,kind)+data)
    def pump(self):
        if self.closed:return
        try:
            data=self.s.recv(65536)
            if not data:self.close();return
            self.buf+=data
        except BlockingIOError:return
        except (ConnectionResetError,ConnectionAbortedError):self.close();return
        while len(self.buf)>=8:
            size,kind=struct.unpack_from('!II',self.buf)
            assert 8<=size<=4096
            if len(self.buf)<size:break
            data=self.buf[8:size];self.buf=self.buf[size:]
            if kind==2:self.c=list(struct.unpack('!'+'i'*(len(data)//4),data))
            if kind==3:self.ready=list(struct.unpack('!11I',data))
            if kind==16:self.role=struct.unpack('!I',data)[0]
            if kind==17:self.notice=data
            if kind==10:self.rejected=data
            assert kind!=4,'Match started without host approval'
    def switch(self,playing,fighter=-1):
        self.send(16,struct.pack('!Iii',self.c[0],int(playing),fighter))
    def edit(self,c):self.send(11,struct.pack('!'+'i'*len(c),*c))
    def mark_ready(self):
        rd=[self.c[0]]+[0]*10
        if self.role<10:rd[1+self.role]=1
        self.send(3,struct.pack('!11I',*rd))
    def close(self):
        self.closed=True;self.s.close()

def wait(predicate,seconds=6):
    end=time.monotonic()+seconds
    while time.monotonic()<end:
        for c in clients:c.pump()
        if predicate():return
        assert process.poll() is None,'Host crashed'
        time.sleep(.01)
    raise AssertionError('Room state did not arrive')

def join(playing):
    c=Client(playing);wait(lambda:c.c is not None and c.role is not None);return c

try:
    end=time.monotonic()+180
    while 'Room open.' not in logpath.read_text(errors='replace'):
        assert process.poll() is None and time.monotonic()<end
        time.sleep(.1)
    data_identity=int(re.search(r'BT4 resources ([0-9a-f]+)',logpath.read_text()).group(1),16)
    identity=14695981039346656037
    for byte in (root/'port/build/bt3.exe').read_bytes():identity=((identity^byte)*1099511628211)&0xffffffffffffffff
    bad=Client(True,True);wait(lambda:bad.rejected is not None);assert b'does not match' in bad.rejected;bad.close()
    players=[join(True) for _ in range(9)]
    wait(lambda:all(c.c[0]==players[-1].c[0] for c in players))
    assert [c.role for c in players]==list(range(1,10))
    assert sorted(players[-1].c[29:39])==list(range(10))
    overflow=Client(True);wait(lambda:overflow.rejected is not None or overflow.closed);overflow.close()
    watchers=[join(False) for _ in range(10)]
    overflow=Client(False);wait(lambda:overflow.rejected is not None or overflow.closed);overflow.close()
    last=players[-1]
    last.switch(False);wait(lambda:last.notice is not None)
    assert b'spectator seats' in last.notice and last.role==9
    watchers[-1].close();time.sleep(.1)
    revision=last.c[0];last.switch(False)
    wait(lambda:last.role==10 and all(c.closed or c.c[0]>revision for c in players+watchers))
    assert 9 not in last.c[29:39] and all(v==0 for v in last.ready[1:])
    promoted=watchers[0];promoted.switch(True)
    wait(lambda:promoted.role==9 and 9 in promoted.c[29:39])
    promoted.mark_ready();wait(lambda:promoted.ready[10]==1)
    slot=promoted.c[29:39].index(9)
    revised=promoted.c.copy();revised[9+slot]=0;revised[19+slot]=0
    promoted.edit(revised)
    wait(lambda:promoted.c[0]>revised[0] and promoted.ready[10]==0)
    # Neither promoted players nor observers can steal an occupied fighter.
    original=promoted.c.copy();stolen=original.copy()
    stolen[29+slot]=-1;stolen[29]=9;promoted.edit(stolen)
    fake=original.copy();fake[9]=55;last.edit(fake);last.mark_ready()
    time.sleep(.1)
    for c in clients:c.pump()
    assert promoted.c==original and last.role==10
    # Release a middle input slot, then refill it without renumbering anybody.
    last.close();time.sleep(.1)
    previous=players[3].role;players[3].switch(False)
    wait(lambda:players[3].role==10)
    players[3].switch(True)
    wait(lambda:players[3].role==previous)
    assert promoted.role==9 and players[0].role==1
    # Malformed spectator traffic cannot stop the room.
    watchers[1].s.sendall(struct.pack('!II',9000,7));wait(lambda:watchers[1].closed)
    replacement=join(False);assert replacement.c[29:39].count(9)==1
    # A player departure frees the seat; another spectator can occupy it.
    previous=players[0].role;revision=replacement.c[0];players[0].close()
    wait(lambda:replacement.c[0]>revision and previous not in replacement.c[29:39])
    replacement.switch(True);wait(lambda:replacement.role==previous)
    assert all(v==0 for v in replacement.ready[1:])
    assert 'Match started.' not in logpath.read_text(errors='replace')
    if a.gpu:
        w,h=map(int,a.window.split('x'))
        wait(lambda:shot.exists() and shot.stat().st_size>w*h*3,seconds=12)
    print('PASS: ten players + ten spectators, promotion/demotion, full-room rejection, stable seats, readiness reset, ownership and reconnect')
finally:
    for c in clients:c.close()
    if process.poll() is None:process.terminate()
    process.wait();log.close()
