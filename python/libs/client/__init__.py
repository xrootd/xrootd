from __future__ import absolute_import, division, print_function

from .glob_funcs import glob, iglob
from .filesystem import DirectoryEntry, FileSystem, RemoveTreeResult
from .file import File
from .stream import open  # noqa: F401
from pyxrootd.client import setXAttrAdler32_cpp as setXAttrAdler32
from .url import URL
from .copyprocess import CopyProcess
from .tape import TapeClient
from .env import EnvPutString
from .env import EnvGetString
from .env import EnvDelString
from .env import EnvPutInt
from .env import EnvGetInt
from .env import EnvDelInt
from ._version import __version__
from .env import EnvGetDefault
from .env import SetLogLevel
from .env import SetLogMask
from .responses import XRootDError
from .responses import XRootDNotFoundError
from .responses import XRootDAuthorizationError
from .responses import XRootDTimeoutError
from .responses import XRootDChecksumError
from .responses import XRootDOperationError
from .responses import raise_on_error

import XRootD.client.finalize

# Keep wildcard imports compatible with callers using the built-in open.
# The remote opener remains available explicitly as client.open.
__all__ = [name for name in globals()
           if not name.startswith('_') and name != 'open']
