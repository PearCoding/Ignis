def __handle_dll():
    import os
    from pathlib import Path

    # Handle correct dll loading
    script_dir = Path(os.path.realpath(os.path.dirname(__file__)))
    added_dirs = []
    if hasattr(os, "add_dll_directory"):
        # We use Windows and require to use add_dll_directory
        added_dirs.append(os.add_dll_directory(str(script_dir)))

        bin_dir = script_dir.parent.parent.joinpath("bin").absolute()
        if bin_dir.exists():
            added_dirs.append(os.add_dll_directory(str(bin_dir)))

        for p in os.environ["PATH"].split(';'):
            if p and os.path.exists(p):
                added_dirs.append(os.add_dll_directory(p))
    return added_dirs


__added_dirs = __handle_dll()

# Import actual module
from .pyignis import *  # nopep8
from .pyignis import __version__, __doc__  # nopep8

# Cleanup dll stuff
for d in __added_dirs:
    d.close()

del __added_dirs

# Remove access to internal module
if "pyignis" in globals():
    del globals()["pyignis"]

if "__handle_dll" in globals():
    del globals()["__handle_dll"]
