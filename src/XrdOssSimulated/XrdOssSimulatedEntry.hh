#ifndef __XRD_OSS_SIMULATED_ENTRY_HH__
#define __XRD_OSS_SIMULATED_ENTRY_HH__

#include "XrdOss/XrdOss.hh"

#include <memory>
#include <shared_mutex>

struct XrdOssSimulatedEntry
{
    int open_return_code{XrdOssOK};
    std::size_t size{};
};

using XrdOssSimulatedEntryPtr = std::shared_ptr<XrdOssSimulatedEntry>;

#endif
