#ifndef  __XRDFILECACHE_LOG_HH
#define  __XRDFILECACHE_LOG_HH
//----------------------------------------------------------------------------------
// Copyright (c) 2014 by Board of Trustees of the Leland Stanford, Jr., University
// Author: Alja Mrak-Tadel, Matevz Tadel, Brian Bockelman
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

#include <fstream>
#include <iostream>

#define xfcMsg(level, format, ...) \
   if (level >= Factory::GetInstance().RefConfiguration().m_logLevel) XrdFileCache::strprintf(level, format, ## __VA_ARGS__)

#define xfcMsgIO(level, io, format, ...) \
   if (level >= Factory::GetInstance().RefConfiguration().m_logLevel ) XrdFileCache::strprintfIO(level, io, format, ## __VA_ARGS__)

class XrdOucCacheIO;

namespace  XrdFileCache
{
   enum LogLevel {
      kDump,
      kDebug,
      kInfo,
      kWarning,
      kError
   };

   const char* levelName(LogLevel);
   void strprintf(LogLevel level, const char* fmt, ...);
   void strprintfIO(LogLevel level,  XrdOucCacheIO* io, const char* fmt, ...);
}

#endif
