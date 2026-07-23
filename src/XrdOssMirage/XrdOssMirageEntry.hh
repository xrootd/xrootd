#ifndef __XRD_OSS_MIRAGE_ENTRY_HH__
#define __XRD_OSS_MIRAGE_ENTRY_HH__

#include "XrdOss/XrdOss.hh"

#include <memory>
#include <unordered_map>
#include <vector>

struct XrdOssMirageEntry
{
    struct
    {
       int return_code{XrdOssOK};
    } open;

    struct
    {
        int return_code{XrdOssOK};
        std::size_t return_position{};
    } read;

    struct
    {
        int return_code{XrdOssOK};
        std::size_t return_position{};
    } write;

    std::string pattern{};
    std::size_t size{};

    std::unordered_map<std::string, std::vector<char>> checksum{};
};

using XrdOssMirageEntryPtr = std::shared_ptr<XrdOssMirageEntry>;

#endif
