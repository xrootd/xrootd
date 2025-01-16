
#ifndef __XRDOSSSTATS_CONFIG_H
#define __XRDOSSSTATS_CONFIG_H

#include <chrono>
#include <string>

namespace XrdOssStats {

namespace detail {

enum LogMask {
    Debug =   0x01,
    Info =    0x02,
    Warning = 0x04,
    Error =   0x08,
    All =     0xff
};

std::string LogMaskToString(int mask);

bool ParseDuration(const std::string &duration, std::chrono::steady_clock::duration &result, std::string &errmsg);

} // detail

} // namespace XrdOssStats

#endif // __XRDOSSSTATS_CONFIG_H