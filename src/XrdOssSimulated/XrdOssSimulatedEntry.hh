#ifndef __XRD_OSS_SIMULATED_ENTRY_HH__
#define __XRD_OSS_SIMULATED_ENTRY_HH__

#include "XrdOss/XrdOss.hh"

#include <memory>

struct XrdOssSimulatedEntry
{
    int open_return_code{XrdOssOK};
    int read_return_code{XrdOssOK};
    std::size_t read_return_position{};
    int write_return_code{XrdOssOK};
    std::size_t write_return_position{};
    std::string pattern{};
    std::size_t size{};
};

using XrdOssSimulatedEntryPtr = std::shared_ptr<XrdOssSimulatedEntry>;

#endif
