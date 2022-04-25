/**
 * This file is part of XrdClHttp
 */

#ifndef __HTTP_FILE_PLUG_IN_UTIL_
#define __HTTP_FILE_PLUG_IN_UTIL_

#include <cstdint>
#include <limits>

namespace XrdCl {

class Log;

// Topic id for the logger
static const uint64_t kLogXrdClHttp = std::numeric_limits<std::uint64_t>::max();

void SetUpLogging(Log* logger);

}

#endif // __HTTP_FILE_PLUG_IN_UTIL_