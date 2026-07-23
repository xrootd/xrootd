#ifndef __XRD_OSS_MIRAGE_ENTRY_HH__
#define __XRD_OSS_MIRAGE_ENTRY_HH__

#include "XrdOss/XrdOss.hh"

#include <memory>
#include <unordered_map>
#include <vector>

class XrdOssMirageEntry
{
public:
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

    std::unordered_map<std::string, std::vector<char>> checksum{};

    const std::string& pattern() const
    {
        return _pattern;
    }

    void set_pattern(std::string pattern)
    {
        _pattern = pattern;
        checksum.clear();
    }

    std::size_t size()  const
    {
        return _size; 
    }

    void set_size(std::size_t size)
    {
        _size = size;
        checksum.clear();
    }

private:
    std::string _pattern{};
    std::size_t _size{};
};

using XrdOssMirageEntryPtr = std::shared_ptr<XrdOssMirageEntry>;

#endif
