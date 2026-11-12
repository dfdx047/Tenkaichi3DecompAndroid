#!/usr/bin/env python3
"""Offline D3D9 SM3 shaders. The player needs no compiler or Python.

Windows build tools: GLSLANG_VALIDATOR and SPIRV_CROSS, plus the system
d3dcompiler_47.dll. The lowering handles the SM3 instruction/interface limits.
Normal builds verify checked-in bytecode against canonical sources and SPIR-V.
"""
from pathlib import Path
import os,re,subprocess,ctypes as c,json,hashlib,struct,shutil,sys
NAMES=('gs.vert','gs.frag','gsnz.frag','gswb.frag','vu0.vert','vu4.vert',
       'vu6.vert','fx.vert','outline.frag','alphakey.frag','dclut.frag')

def digest(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def inputs(root):
    return {p.name:digest(p) for p in sorted((root/'port/src/gs/shaders').glob('*.*'))
            if p.suffix in ('.vert','.frag')}

def generate(root,gen,header,manifest):
    glslang=os.environ.get('GLSLANG_VALIDATOR') or shutil.which('glslangValidator')
    cross=os.environ.get('SPIRV_CROSS') or shutil.which('spirv-cross')
    if not glslang or not cross:
        raise RuntimeError('SM3 generation needs GLSLANG_VALIDATOR and SPIRV_CROSS build tools')
    out=gen/'d3d9';out.mkdir(parents=True,exist_ok=True);env=os.environ.copy()
    compiler=c.WinDLL('d3dcompiler_47.dll',winmode=0x800)
    compile=compiler.D3DCompile;compile.restype=c.c_long
    compile.argtypes=[c.c_void_p,c.c_size_t,c.c_char_p,c.c_void_p,c.c_void_p,c.c_char_p,c.c_char_p,c.c_uint,c.c_uint,c.POINTER(c.c_void_p),c.POINTER(c.c_void_p)]
    def blob_bytes(blob):
        v=c.cast(blob,c.POINTER(c.POINTER(c.c_void_p))).contents
        ptr=c.WINFUNCTYPE(c.c_void_p,c.c_void_p)(v[3])(blob)
        size=c.WINFUNCTYPE(c.c_size_t,c.c_void_p)(v[4])(blob)
        data=c.string_at(ptr,size);c.WINFUNCTYPE(c.c_ulong,c.c_void_p)(v[2])(blob);return data
    records=[]
    for spv in sorted((gen).glob('*.spv')):
        name=spv.stem;stage=name.split('.')[1];src=root/'port/src/gs/shaders'/name
        if name not in NAMES:continue # GPU-only grading passes have no SM3 pipeline.
        if not src.exists():src=gen/name
        text=src.read_text();vertex=stage=='vert'
        if name=='fx.vert':
            text=text.replace('layout(location = 0) out vec2 vUv;', 'layout(location = 0) in vec2 inFx;\nlayout(location = 0) out vec2 vUv;')
            text=text.replace('vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2)', 'inFx')
        if vertex and name!='fx.vert':
            text=text.replace('void main()', 'layout(location = 2) out vec2 vDepth;\nvoid main()')
            at=text.rfind('}');text=text[:at]+'    vDepth = gl_Position.zw;\n'+text[at:]
        if name.startswith(('gs.frag','gsnz.frag','gswb.frag')):
            text=text.replace('ivec4 mode;', 'vec4 mode;').replace('int f = p.mode.w - 1;', 'int f = int(p.mode.w) - 1;')
            text=text.replace('layout(location = 0) in vec4 vColor;', 'layout(location = 2) in vec2 vDepth;\nlayout(location = 0) in vec4 vColor;')
            text=text.replace('gl_FragCoord.z', '(vDepth.x / vDepth.y)')
            text=text.replace('textureSize(tex, 0)', 'ivec2(sz.dim[0].xy)')
            text=text.replace('float(int(ab * 255.0 + 0.5) | 128)', 'max(floor(ab * 255.0 + 0.5), 128.0 + mod(floor(ab * 255.0 + 0.5), 128.0))')
            text=text.replace('texelFetch(dateTex, ivec2(gl_FragCoord.xy), 0)', 'textureLod(dateTex, (floor(gl_FragCoord.xy) + vec2(0.5)) / sz.dim[1].xy, 0.0)')
            text=text.replace('#version 450', '#version 450\nlayout(set = 3, binding = 1) uniform TexSizes { vec4 dim[3]; } sz;',1)
        if name=='dclut.frag':text=text.replace('texelFetch(clutTex, ivec2(idx, 0), 0)', 'textureLod(clutTex, vec2((float(idx) + 0.5) / 256.0, 0.5), 0.0)')
        if name=='outline.frag':
            text=text.replace('int c = (id << 3) & 0xFF;', 'int c = int(mod(float(id) * 8.0, 256.0));')
            text=text.replace('textureSize(aux, 0).x','sz.dim[0].x')
            text=text.replace('#version 450', '#version 450\nlayout(set = 3, binding = 1) uniform TexSizes { vec4 dim[3]; } sz;',1)
            start=text.index('    for (int k = 1;');opening=text.index('{',start);end=opening+1;depth=1
            while depth:
                if text[end]=='{':depth+=1
                elif text[end]=='}':depth-=1
                end+=1
            body=text[opening+1:end-1].replace('break;', 'edgeFound = true;')
            expanded='    bool edgeFound = false;\n'
            for step in range(1,9):
                expanded+='    if (!edgeFound && '+str(float(step))+' < width + 1.0) {\n'+body.replace('float(k)',str(float(step)))+'\n    }\n'
            text=text[:start]+expanded+text[end:]
        glsl=out/name;glsl.write_text(text);legacyspv=out/(name+'.spv');hlsl=out/(name+'.hlsl')
        row=dict(shader=name,compiled=False)
        for cmd in [[glslang,'-V','-S',stage,str(glsl),'-o',str(legacyspv)],
                    [cross,str(legacyspv),'--hlsl','--shader-model','30','--flatten-ubo','--output',str(hlsl)]]:
            r=subprocess.run(cmd,capture_output=True,text=True,env=env,creationflags=0x08000000)
            if r.returncode:row['error']=(r.stdout+r.stderr).strip();break
        else:
            text=hlsl.read_text()
            text=re.sub(r'uniform float4 U\[20\];','uniform float4 U[20] : register(c0);',text)
            text=re.sub(r'uniform float4 Params\[(\d+)\];',r'uniform float4 Params[\1] : register(c0);',text)
            text=text.replace('uniform float4 TexSizes[3];','uniform float4 TexSizes[3] : register(c5);')
            text=text.replace('uniform float4 gl_HalfPixel;','uniform float4 gl_HalfPixel : register(c20);')
            slots={'tex':0,'dateTex':1,'aux':0,'depthTex':0,'auxTex':1,'clutTex':2}
            for sampler,slot in slots.items():text=text.replace('uniform sampler2D '+sampler+';','uniform sampler2D '+sampler+' : register(s'+str(slot)+');')
            hlsl.write_text(text);data=text.encode();code=c.c_void_p();errors=c.c_void_p()
            hr=compile(data,len(data),name.encode(),None,None,b'main',b'vs_3_0' if vertex else b'ps_3_0',1<<15,0,c.byref(code),c.byref(errors))
            if errors:row['diagnostics']=blob_bytes(errors).decode(errors='replace')
            if hr<0:row['error']='D3DCompile HRESULT '+hex(hr&0xffffffff)
            else:
                binary=blob_bytes(code);(out/(name+'.bin')).write_bytes(binary)
                row.update(compiled=True,bytes=len(binary),sha256=hashlib.sha256(binary).hexdigest())
        records.append(row);print(json.dumps(row),flush=True)
    if len(records)!=len(NAMES) or not all(r['compiled'] for r in records):
        raise RuntimeError('SM3 shader generation failed: '+json.dumps(records))
    arrays=[];entries=[]
    for row in records:
        name=row['shader'];stem,stage=name.split('.');symbol='k'+stem.capitalize()+stage.capitalize()
        data=(out/(name+'.bin')).read_bytes();words=struct.unpack('<'+'I'*(len(data)//4),data)
        arrays.append('static const DWORD '+symbol+'[] = {'+','.join(hex(x) for x in words)+'};\n')
        entries.append('{"'+name+'",'+symbol+','+str(stage=='vert').lower()+'}')
    header.parent.mkdir(parents=True,exist_ok=True)
    header.write_text('/* Generated by port/tools/d3d9_shaders.py. */\n'+''.join(arrays)+
                     '\nstruct ShaderBlob { const char *name; const DWORD *code; bool vertex; };\n'+
                     'static const ShaderBlob kShaders[] = {'+','.join(entries)+'};\n')
    manifest.write_text(json.dumps(dict(source_sha256=inputs(root),
        generator_sha256=digest(Path(__file__)),header_sha256=digest(header),
        spv_sha256={n:digest(gen/(n+'.spv')) for n in NAMES},shaders=records),indent=2)+'\n')

def prepare(root,gen,windows,regenerate=False):
    if not windows: return
    folder=root/'port/src/gs/generated';header=folder/'shaders_d3d9.h';manifest=folder/'shaders_d3d9.json'
    if regenerate: generate(root,gen,header,manifest)
    if not header.exists() or not manifest.exists():
        raise RuntimeError('Missing SM3 shaders. Run port/tools/d3d9_shaders.py --generate')
    row=json.loads(manifest.read_text())
    if (row['source_sha256']!=inputs(root) or row['header_sha256']!=digest(header) or
        row['generator_sha256']!=digest(Path(__file__)) or
        row['spv_sha256']!={n:digest(gen/(n+'.spv')) for n in NAMES}):
        raise RuntimeError('Stale SM3 shaders. Run port/tools/d3d9_shaders.py --generate')

if __name__=='__main__':
    root=Path(__file__).resolve().parents[2]
    prepare(root,root/'port/build/gen/gs',True,'--generate' in sys.argv)
    print('D3D9 shader sources and bytecode verified')
