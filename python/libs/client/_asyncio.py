# Copyright (c) 2026 by the XRootD developers
# This file is part of the XRootD software suite and is distributed under the
# terms of the GNU Lesser General Public License, version 3 or later.
"""Version boundary for the optional asyncio and fsspec interfaces.

Keep this module and all packaged sources parseable by Python 3.6 so the
classic bindings can still be installed and byte-compiled on AlmaLinux 8.
"""

import sys

if sys.version_info < (3, 11):
    raise ImportError(
        'XRootD.client.aio, asyncstream and fsspec require Python >= 3.11; '
        'the classic XRootD.client bindings and callback API remain '
        'available on Python >= 3.6.')

# The version check must run before loading optional feature dependencies.
import asyncio  # noqa: E402,F401
