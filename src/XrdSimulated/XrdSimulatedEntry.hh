#ifndef __XRD_SIMULATED_ENTRY_HH__
#define __XRD_SIMULATED_ENTRY_HH__

#include <memory>
#include <shared_mutex>

struct XrdSimulatedEntry
{
    std::unique_ptr<std::shared_mutex> mutex = std::make_unique<std::shared_mutex>();
};

#endif
