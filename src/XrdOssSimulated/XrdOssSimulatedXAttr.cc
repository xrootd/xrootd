#include "XrdOssSimulatedXAttr.hh"
#include "XrdVersion.hh"

#include <algorithm>
#include <span>
#include <string_view>

using namespace std::literals;

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
    return -ENOTSUP;
}

void XrdOssSimulatedXAttr::Free(AList *aPL)
{
}

int XrdOssSimulatedXAttr::Get(const char *Aname, void *Aval, int Avsz, const char *Path, int fd)
{
    if (this->oss == nullptr)
        return -ENOTSUP;

    const auto opt = oss->getEntry(Path);
    if (!opt.has_value())
        return -ENOENT;

    const auto entry = opt.value();

    const std::string_view name{Aname};
    std::string value{};

    if (name == "U.open.return_code"sv)
        value = std::to_string(entry->open.return_code);
    else if (name == "U.read.return_code"sv)
        value = std::to_string(entry->read.return_code);
    else if (name == "U.read.return_position"sv)
        value = std::to_string(entry->read.return_position);
    else if (name == "U.write.return_code"sv)
        value = std::to_string(entry->write.return_code);
    else if (name == "U.write.return_position"sv)
        value = std::to_string(entry->write.return_position);
    else if (name == "U.pattern"sv)
        value = entry->pattern;
    else
        return -EINVAL;

    if (value.empty())
        return -EINVAL;

    const std::span output(static_cast<char *>(Aval), Avsz);

    const int num_bytes = std::min(output.size(), value.size());
    std::copy_n(value.begin(), num_bytes, output.begin());

    return num_bytes;
}

int XrdOssSimulatedXAttr::List(AList **aPL, const char *Path, int fd, int getSz)
{
    return -ENOTSUP;
}

int XrdOssSimulatedXAttr::Set(const char *Aname, const void *Aval, int Avsz, const char *Path,  int fd,  int isNew)
{
    if (this->oss == nullptr)
        return -ENOTSUP;

    const auto opt = oss->getEntry(Path);
    if (!opt.has_value())
        return -ENOENT;

    auto entry = opt.value();

    const std::string_view name{Aname};
    const std::string value(static_cast<const char *>(Aval), Avsz);

    try
    {
        if (name == "U.open.return_code"sv)
            entry->open.return_code = std::stoi(value);
        else if (name == "U.read.return_code"sv)
            entry->read.return_code = std::stoi(value);
        else if (name == "U.read.return_position"sv)
            entry->read.return_position = std::stoll(value);
        else if (name == "U.write.return_code"sv)
            entry->write.return_code = std::stoi(value);
        else if (name == "U.write.return_position"sv)
            entry->write.return_position = std::stoll(value);
        else if (name == "U.pattern"sv)
            entry->pattern = value;
        else
            return -EINVAL;
    }
    catch(std::out_of_range &)
    {
        return -EINVAL;
    }

    return 0;
}

void XrdOssSimulatedXAttr::setOss(XrdOssSimulated &oss)
{
    if (this->oss == nullptr)
        this->oss = &oss;
}
