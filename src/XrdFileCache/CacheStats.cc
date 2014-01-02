#include "CacheStats.hh"
#include "Context.hh"

using namespace XrdFileCache;

void CacheStats::Dump() const
{
   printf("CacheStats::Dump() bCachedPrefetch = %lld, bPrefetch = %lld\n", BytesCachedPrefetch, BytesPrefetch);
}
