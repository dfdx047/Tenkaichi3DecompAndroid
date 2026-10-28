"""Compatibility entry point for the native lobby permission checks."""
import runpy
from pathlib import Path
runpy.run_path(str(Path(__file__).with_name('tag_online_roles.py')), run_name='__main__')
