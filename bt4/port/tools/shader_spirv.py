"""Use verified SPIR-V matching the checked-in DirectX shader binaries."""
import hashlib,json,os

def source_hash(data):
    return hashlib.sha256(data.replace(b"\r\n",b"\n")).hexdigest()

def prepare(root,gen,source):
    if os.environ.get("BT3_REGENERATE_SPIRV")=="1":return False
    cache=root/"port/src/gs/generated/spirv"
    record=json.loads((cache/"manifest.json").read_text())
    entry=record.get(source.name)
    if entry is None or entry["source_sha256"]!=source_hash(source.read_bytes()):
        raise RuntimeError("Shader source changed: regenerate SPIR-V and DirectX shaders: "+source.name)
    data=(cache/(source.name+".spv")).read_bytes()
    if data[:4]!=b"\x03\x02\x23\x07" or hashlib.sha256(data).hexdigest()!=entry["spv_sha256"]:
        raise RuntimeError("Invalid checked-in SPIR-V: "+source.name)
    target=gen/(source.name+".spv")
    if not target.exists() or target.read_bytes()!=data:target.write_bytes(data)
    return True
