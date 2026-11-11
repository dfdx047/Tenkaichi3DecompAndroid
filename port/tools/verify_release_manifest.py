"""Check every published file digest inside a release ZIP without running it."""
import argparse
import hashlib
from pathlib import Path, PurePosixPath
import re
import zipfile

def verify(manifest, package):
    expected = {}
    for line in manifest.read_text(encoding="utf-8").splitlines():
        match = re.fullmatch(r"([a-f0-9]{64})  (.+)", line)
        if not match:
            raise ValueError("Malformed SHA-256 manifest entry")
        digest, name = match.groups()
        path = PurePosixPath(name)
        if path.is_absolute() or ".." in path.parts or "\\" in name or name in expected:
            raise ValueError("Unsafe or duplicate manifest path")
        expected[name] = digest
    if not expected:
        raise ValueError("Empty manifest")
    with zipfile.ZipFile(package) as archive:
        entries = [entry for entry in archive.infolist() if not entry.is_dir()]
        names = [entry.filename for entry in entries]
        if len(names) != len(set(names)) or set(names) != set(expected):
            raise ValueError("Archive file list differs from the manifest")
        for entry in entries:
            digest = hashlib.sha256()
            with archive.open(entry) as source:
                for block in iter(lambda: source.read(1024 * 1024), b""):
                    digest.update(block)
            if digest.hexdigest() != expected[entry.filename]:
                raise ValueError("SHA-256 mismatch: " + entry.filename)
    return len(expected)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=Path)
    parser.add_argument("package", type=Path)
    args = parser.parse_args()
    print("Verified", verify(args.manifest, args.package), "files; no executable was run")
