#include "XrdOssMirageWrapperBase.hh"

bool XrdOssMirageWrapperBase::is_mirage_path(std::string_view path) const noexcept
{
    const std::string &prefix = mirage.get_prefix_path();
    return path.size() >= prefix.size() && path.compare(0, prefix.size(), prefix) == 0;
}
