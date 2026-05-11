#ifndef __XRD_OSS_SIMULATED_ENTRY_HH__
#define __XRD_OSS_SIMULATED_ENTRY_HH__

#include "XrdOss/XrdOss.hh"

#include <memory>

struct XrdOssSimulatedEntry
{
    struct
    {
       int return_code{XrdOssOK};
    } open;

    struct
    {
        int return_code{XrdOssOK};
        std::size_t return_position{};
        unsigned int bandwidth_limiter{};
    } read;

    struct
    {
        int return_code{XrdOssOK};
        std::size_t return_position{};
        unsigned int bandwidth_limiter{};
    } write;

    std::string pattern{};
    std::size_t size{};
};

using XrdOssSimulatedEntryPtr = std::shared_ptr<XrdOssSimulatedEntry>;

#endif
