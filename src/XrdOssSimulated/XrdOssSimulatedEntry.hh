#ifndef __XRD_OSS_SIMULATED_ENTRY_HH__
#define __XRD_OSS_SIMULATED_ENTRY_HH__

#include <memory>
#include <shared_mutex>

struct XrdOssSimulatedEntry
{
    std::size_t size;
    std::unique_ptr<std::shared_mutex> mutex = std::make_unique<std::shared_mutex>();
};

#endif
