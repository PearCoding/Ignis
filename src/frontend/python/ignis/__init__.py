from .pyignis import *
from .pyignis import __version__, __doc__


if "pyignis" in globals():
    del globals()["pyignis"]
