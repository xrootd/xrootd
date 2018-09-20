//----------------------------------------------------------------------------------
// 1. The Original Code is Mozilla code.
//    The Initial Developer of the Original Code is Mozilla Foundation.
//    Portions created by the Initial Developer are Copyright (C) 2010 the Initial Developer. All Rights Reserved.
//    Contributor(s):  Taras Glek <tglek@mozilla.com>
// 2. Created from the OSX-specific code from Mozilla's mozilla::fallocation() function.
//    Adaptation (C) 2015,2016 R.J.V. Bertin for KDE, project.
//
// Original license allows modification / redistribution under LGPL 2.1 or higher.
//----------------------------------------------------------------------------------
// XRootD is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// XRootD is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with XRootD.  If not, see <http://www.gnu.org/licenses/>.
//----------------------------------------------------------------------------------

// Implementation of posix_allocate for OSX.

#ifdef __APPLE__

extern int posix_fallocate(int fd, off_t offset, off_t len);

#else

#include <fcntl.h>

#endif
