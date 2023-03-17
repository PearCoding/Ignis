import os
from pathlib import Path

# Handle correct dll loading
script_dir = Path(os.path.realpath(os.path.dirname(__file__)))
added_dirs = []
if hasattr(os, "add_dll_directory"):
    # We use Windows and require to use add_dll_directory
    added_dirs.append(os.add_dll_directory(str(script_dir)))
    for p in os.environ["PATH"].split(';'):
        if len(p) > 0:
            added_dirs.append(os.add_dll_directory(p))

# Import actual module
from .pyignis import *  # nopep8
from .pyignis import __version__, __doc__  # nopep8

# Cleanup dll stuff
for d in added_dirs:
    d.close()

del script_dir
del added_dirs

# Remove access to internal module
if "pyignis" in globals():
    del globals()["pyignis"]
