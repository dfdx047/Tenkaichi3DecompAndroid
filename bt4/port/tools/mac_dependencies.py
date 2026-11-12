"""Resolve and validate the complete non-system Mach-O dependency graph."""
from pathlib import Path
import subprocess


def inspect_macho(file):
    def output(*args):
        return subprocess.check_output(list(map(str, args)), text=True)
    dependencies = [line.strip().split(' (', 1)[0]
                    for line in output('otool', '-L', file).splitlines()[1:]]
    identity_lines = output('otool', '-D', file).splitlines()[1:]
    identity = identity_lines[0].strip() if identity_lines else None
    lines = output('otool', '-l', file).splitlines()
    rpaths = []
    for index, line in enumerate(lines):
        if line.strip() == 'cmd LC_RPATH':
            for value in lines[index + 1:index + 4]:
                if value.strip().startswith('path '):
                    rpaths.append(value.strip()[5:].rsplit(' (offset ', 1)[0])
                    break
    return [name for name in dependencies if name != identity], rpaths


def system_library(name):
    return name.startswith(('/usr/lib/', '/System/Library/'))


def expand_path(name, loader, executable):
    for prefix, root in (('@loader_path', loader.parent),
                         ('@executable_path', executable.parent)):
        if name == prefix or name.startswith(prefix + '/'):
            return root / name[len(prefix):].lstrip('/')
    return Path(name) if Path(name).is_absolute() else None


def collect_dependencies(executables, seeds, inspect=inspect_macho):
    """Return source paths and rewrites, including nested @rpath dependencies.

    Inspect originals before rewriting them. LC_RPATH entries inherited from
    the executable are carried through the graph; a sibling lookup supports
    Homebrew libraries whose @rpath is supplied by the original curl loader.
    Missing dependencies and ambiguous basename collisions stop packaging.
    """
    executables = [Path(file).resolve() for file in executables]
    seeds = [Path(file) for file in seeds]
    sources, changes, done = {}, {}, set()
    root = executables[0]
    inherited = []
    for exe in executables:
        _, paths = inspect(exe)
        inherited.extend(path for value in paths
                         if (path := expand_path(value, exe, exe)) is not None)
    queue = [(file, file, ()) for file in executables]

    def add(source, exe, paths, name=None):
        name = name or source.name
        source = source.resolve()
        previous = sources.get(name)
        if previous is not None and previous != source:
            raise RuntimeError('Conflicting bundled library: ' + name)
        sources[name] = source
        queue.append((source, exe, tuple(paths)))

    for seed in seeds:
        add(seed, root, inherited)
    while queue:
        file, exe, parent_paths = queue.pop(0)
        if file in done:
            continue
        done.add(file)
        names, paths = inspect(file)
        search = [path for name in paths
                  if (path := expand_path(name, file, exe)) is not None]
        search.extend(parent_paths)
        search.extend(inherited)
        changes[file] = []
        for name in names:
            if system_library(name):
                continue
            if name.startswith('@rpath/'):
                relative = name[len('@rpath/'):]
                candidates = [folder / relative for folder in search]
                candidates.append(file.parent / relative)
            else:
                candidate = expand_path(name, file, exe)
                candidates = [candidate] if candidate is not None else []
            source = next((value.resolve() for value in candidates if value.is_file()), None)
            if source is None:
                raise RuntimeError(str(file) + ': unresolved dependency ' + name)
            alias = Path(name).name
            changes[file].append((name, alias))
            add(source, exe, search, alias)
    return sources, changes


def verify_dependencies(out, inspect=inspect_macho):
    """Every redistributed Mach-O must load only system or bundled libraries."""
    out = Path(out).resolve()
    executable = out / 'BT4decomp'
    files = [executable, out / 'BT4decomp-setup', *sorted((out / 'lib').glob('*.dylib'))]
    for file in files:
        names, _ = inspect(file)
        for name in names:
            if system_library(name):
                continue
            dependency = expand_path(name, file, executable)
            if dependency is None:
                raise RuntimeError(str(file) + ': non-portable dependency ' + name)
            dependency = dependency.resolve()
            if not dependency.is_relative_to(out) or not dependency.is_file():
                raise RuntimeError(str(file) + ': missing bundled dependency ' + name)
    return files
