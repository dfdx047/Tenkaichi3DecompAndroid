"""Mach-O dependency fixtures; runnable without macOS or Homebrew."""
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from mac_dependencies import collect_dependencies, inspect_macho, verify_dependencies


class DependencyTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.graph = {}
        self.exe = self.file('package/BT4decomp')
        self.setup = self.file('package/BT4decomp-setup')

    def file(self, name, deps=(), rpaths=()):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.touch()
        self.graph[path] = (list(deps), list(rpaths))
        return path

    def inspect(self, file):
        return self.graph[Path(file).resolve()]

    def collect(self, *seeds):
        return collect_dependencies([self.exe, self.setup], seeds, self.inspect)

    def test_brotli_rpath_sibling_is_bundled_transitively(self):
        common = self.file('brew/brotli/lib/libbrotlicommon.1.dylib')
        decoder = self.file('brew/brotli/lib/libbrotlidec.1.dylib',
                            ['@rpath/libbrotlicommon.1.dylib'])
        curl = self.file('brew/curl/lib/libcurl.4.dylib', [str(decoder)])
        sources, changes = self.collect(curl)
        self.assertEqual(sources[common.name], common)
        self.assertEqual(changes[decoder], [('@rpath/' + common.name, common.name)])
        self.assertEqual(len(sources), 3)

    def test_library_rpath_and_loader_path(self):
        dependency = self.file('sdk/dependencies/libhelper.dylib')
        seed = self.file('sdk/lib/libmedia.dylib', ['@rpath/libhelper.dylib'],
                         ['@loader_path/../dependencies'])
        sources, _ = self.collect(seed)
        self.assertEqual(sources[dependency.name], dependency)

    def test_rpath_inherited_from_executable(self):
        dependency = self.file('sdk/lib/libhelper.dylib')
        seed = self.file('other/libmedia.dylib', ['@rpath/libhelper.dylib'])
        self.graph[self.exe] = ([], [str(dependency.parent)])
        self.assertIn(dependency.name, self.collect(seed)[0])

    def test_cycles_terminate_and_keep_requested_names(self):
        first = self.file('sdk/liba.dylib', ['@loader_path/libb.dylib'])
        second = self.file('sdk/libb.dylib', ['@loader_path/liba.dylib'])
        sources, _ = self.collect(first)
        self.assertEqual(sources, {first.name: first, second.name: second})

    def test_missing_transitive_dependency_stops_packaging(self):
        seed = self.file('sdk/libbrotlidec.1.dylib', ['@rpath/libbrotlicommon.1.dylib'])
        with self.assertRaisesRegex(RuntimeError, 'unresolved dependency @rpath/libbrotlicommon'):
            self.collect(seed)

    def test_library_name_collision_stops_packaging(self):
        one = self.file('one/libhelper.dylib')
        two = self.file('two/libhelper.dylib')
        with self.assertRaisesRegex(RuntimeError, 'Conflicting bundled library'):
            self.collect(one, two)

    def test_bundle_validation_checks_nested_libraries(self):
        decoder = self.file('package/lib/libbrotlidec.1.dylib',
                            ['@loader_path/libbrotlicommon.1.dylib'])
        self.graph[self.exe] = (['@executable_path/lib/' + decoder.name], [])
        with self.assertRaisesRegex(RuntimeError, 'missing bundled dependency'):
            verify_dependencies(self.exe.parent, self.inspect)
        common = self.file('package/lib/libbrotlicommon.1.dylib', ['/usr/lib/libSystem.B.dylib'])
        self.assertIn(common, verify_dependencies(self.exe.parent, self.inspect))

    def test_bundle_rejects_external_and_unresolved_rpath_libraries(self):
        for name in ['/opt/homebrew/lib/libbrotlicommon.1.dylib', '@rpath/libbrotlicommon.1.dylib',
                     '@loader_path/../../outside.dylib']:
            with self.subTest(name=name):
                self.graph[self.exe] = ([name], [])
                with self.assertRaises(RuntimeError):
                    verify_dependencies(self.exe.parent, self.inspect)

    def test_otool_parser_omits_install_id_and_reads_rpath(self):
        outputs = {
            '-L': 'libcurl:\n\t@rpath/libcurl.4.dylib (compatibility version 1.0.0)\n'
                  '\t@rpath/libbrotlidec.1.dylib (compatibility version 1.0.0)\n',
            '-D': 'libcurl:\n@rpath/libcurl.4.dylib\n',
            '-l': 'Load command 0\n          cmd LC_RPATH\n      cmdsize 48\n'
                  '         path @loader_path/../lib (offset 12)\n',
        }
        with patch('mac_dependencies.subprocess.check_output',
                   side_effect=lambda args, **kw: outputs[args[1]]):
            self.assertEqual(inspect_macho(self.exe),
                             (['@rpath/libbrotlidec.1.dylib'], ['@loader_path/../lib']))


if __name__ == '__main__':
    unittest.main()
