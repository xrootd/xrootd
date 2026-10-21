#ifndef __XRD_OSS_MIRAGE_ENTRY_HH__
#define __XRD_OSS_MIRAGE_ENTRY_HH__

#include "XrdOss/XrdOss.hh"

#include <sys/types.h>

#include <cstdint>
#include <limits>
#include <memory>

struct XrdOssMirageEntry
{
    static constexpr std::uint64_t max_file_size{static_cast<std::uint64_t>(std::numeric_limits<off_t>::max())};

    struct
    {
       int return_code{XrdOssOK};
    } open;

    struct
    {
       int return_code{XrdOssOK};
    } close;

    struct
    {
        int return_code{XrdOssOK};
        std::uint64_t return_position{};
    } read;

    struct
    {
        int return_code{XrdOssOK};
        std::uint64_t return_position{};
    } write;

    std::string pattern{};
    std::uint64_t size{};
};

using XrdOssMirageEntryPtr = std::shared_ptr<XrdOssMirageEntry>;

#endif
