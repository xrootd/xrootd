#ifndef __XRD_OSS_MIRAGE_WRAPPER_BASE_HH__
#define __XRD_OSS_MIRAGE_WRAPPER_BASE_HH__

#include "XrdOssMirage.hh"

#include <string_view>

//
// Base class for a mirage file, a mirage directory, and the mirage wrapper.
// Each one holds a reference to the XrdOssMirage instance whose prefix path
// decides if a given path belongs to the mirage back end or to the stacked
// plugin.
//
class XrdOssMirageWrapperBase {
public:
    explicit XrdOssMirageWrapperBase(XrdOssMirage &mirage) noexcept : mirage(mirage) {}

    bool is_mirage_path(std::string_view path) const noexcept;

protected:
    XrdOssMirage &mirage;
};

#endif
