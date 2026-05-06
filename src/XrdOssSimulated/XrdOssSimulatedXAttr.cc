#include "XrdOssSimulatedXAttr.hh"
#include "XrdVersion.hh"

#include "XrdSys/XrdSysError.hh"

#include <string_view>

using namespace std::literals;

namespace XrdGlobal
{
    extern XrdSysError Log;
}

XrdVERSIONINFO(XrdSysGetXAttrObject, XrdOssSimulatedXAttr);

extern "C"
{
    XrdSysXAttr* XrdSysGetXAttrObject(XrdSysError  *errP, const char   *config_fn, const char   *parms)
    {
        return new XrdOssSimulatedXAttr();
    }
}

int XrdOssSimulatedXAttr::Del(const char *Aname, const char *Path, int fd)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return -ENOTSUP;
}

void XrdOssSimulatedXAttr::Free(AList *aPL)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
}

int XrdOssSimulatedXAttr::Get(const char *Aname, void *Aval, int Avsz, const char *Path, int fd)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);

    if (this->oss == nullptr)
        return -ENOTSUP;

    auto opt = oss->getEntry(Path);
    if (!opt.has_value())
        return -ENOENT;

    auto entry = opt.value();

    auto name = std::string_view(Aname);

    std::string value{};

    if (name == "U.code"sv)
        value = std::to_string(entry->open_return_code);
    else if (name == "U.pattern"sv)
        value = entry->pattern;
    else
        return -EINVAL;

    if (value.empty())
        return -EINVAL;

    int written = std::min(static_cast<int>(value.size()), Avsz);
    std::memcpy(Aval, value.c_str(), written);

    return written;
}

int XrdOssSimulatedXAttr::List(AList **aPL, const char *Path, int fd, int getSz)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return -ENOTSUP;
}

int XrdOssSimulatedXAttr::Set(const char *Aname, const void *Aval, int Avsz, const char *Path,  int fd,  int isNew)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);

    if (this->oss == nullptr)
        return -ENOTSUP;

    auto opt = oss->getEntry(Path);
    if (!opt.has_value())
        return -ENOENT;

    auto entry = opt.value();

    auto name = std::string_view(Aname);
    auto value = std::string(static_cast<const char *>(Aval), Avsz);

    if (name == "U.code"sv)
        entry->open_return_code = std::stoi(value);
    else if (name == "U.pattern"sv)
        entry->pattern = value;
    else
        return -EINVAL;

    return 0;
}

void XrdOssSimulatedXAttr::setOss(XrdOssSimulated &oss)
{
    if (this->oss == nullptr)
        this->oss = &oss;
}
