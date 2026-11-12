import fs from 'node:fs';
import assert from 'node:assert/strict';
const [wasm,eventsFile,traceFile,wireFile]=process.argv.slice(2);
const a=new WebAssembly.Instance(new WebAssembly.Module(fs.readFileSync(wasm)),{}).exports;
assert.equal(typeof a.Kernel_ImportArmed,'function','Rebuild the production WASM kernel before testing');
const input=new Uint8Array(a.memory.buffer,a.Kernel_Input(),512),output=new Uint8Array(a.memory.buffer,a.Kernel_Output(),512);
const events=fs.readFileSync(eventsFile),trace=Buffer.alloc(events.length/520*20),wire=[];
assert.equal(events.length%520,0);let at=0;
for(let offset=0;offset<events.length;offset+=520){
 const op=events.readUInt32LE(offset),member=events.readUInt32LE(offset+4);let value=0;
 input.set(events.subarray(offset+8,offset+520));
 switch(op){
 case 1:value=a.Kernel_Command(member);break;
 case 2:value=(a.Kernel_DeadlineFrame()<<1)|a.Kernel_TakeFrame();break;
 case 3:a.Kernel_Leave(member);break;
 case 4:input.fill(0);input.set(new TextEncoder().encode('ABC234'));a.Kernel_Snapshot(member,0,a.Kernel_Input());break;
 case 5:value=a.Kernel_ImportArmed(a.Kernel_Input());break;
 case 6:a.Kernel_Abort();break;
 case 7:value=a.Kernel_ImportArmed(0);break;
 default:throw new Error('Unknown event '+op);
 }
 trace.writeUInt32LE(op,at);trace.writeUInt32LE(value>>>0,at+4);trace.writeUInt32LE(a.Kernel_Phase()>>>0,at+8);
 trace.writeUInt32LE(a.Kernel_Epoch()>>>0,at+12);trace.writeUInt32LE(a.Kernel_PendingFrame()>>>0,at+16);at+=20;
 if(op===2||op===4)wire.push(Buffer.from(output));
}
fs.writeFileSync(traceFile,trace);fs.writeFileSync(wireFile,Buffer.concat(wire));
