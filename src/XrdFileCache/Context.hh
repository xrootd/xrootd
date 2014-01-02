#ifndef  __XRDFILECACHE_CONTEXT_HH
#define  __XRDFILECACHE_CONTEXT_HH

#include <fstream>
#include <iostream>

class XrdOucCacheIO;


#define aMsg(level, format, ...)\
   if (level >= Dbg) XrdFileCache::strprintf(level, format, ##__VA_ARGS__)

#define aMsgIO(level, io, format, ...) \
   if (level >= Dbg) XrdFileCache::strprintfIO(level, io, format, ##__VA_ARGS__)



namespace  XrdFileCache
{ 
enum LogLevel {
   kDump,
   kDebug,
   kInfo,
   kWarning,
   kError
};

extern LogLevel Dbg;
extern const char* InfoExt;
extern const int InfoExtLen;
extern const bool IODisablePrefetch;
extern const long long PrefetchDefaultBufferSize;

const char* levelName(LogLevel);
void strprintf(LogLevel level, const char* fmt, ...);
void strprintfIO(LogLevel level,  XrdOucCacheIO* io, const char* fmt, ...);
}

#endif
