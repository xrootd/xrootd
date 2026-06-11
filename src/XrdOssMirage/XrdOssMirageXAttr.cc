#include "XrdOssMirageXAttr.hh"
#include "XrdVersion.hh"

#include <algorithm>
#include <string_view>

using namespace std::literals;

XrdVERSIONINFO(XrdSysGetXAttrObject, XrdOssMirageXAttr);

extern "C"
{
    XrdSysXAttr* XrdSysGetXAttrObject(XrdSysError  *errP, const char   *config_fn, const char   *parms)
    {
        return new XrdOssMirageXAttr();
    }
}

int XrdOssMirageXAttr::Del(const char *Aname, const char *Path, int fd)
{
    if (this->oss == nullptr)
        return -ENOTSUP;

    const auto opt = oss->get_entry_write(Path);
    if (!opt.has_value())
        return -EINVAL;

    auto entry = opt.value();

    const std::string_view name{Aname};

    if (name == "U.open.return_code"sv)
        entry->open.return_code = {};
    else if (name == "U.read.return_code"sv)
        entry->read.return_code = {};
    else if (name == "U.read.return_position"sv)
        entry->read.return_position = {};
    else if (name == "U.write.return_code"sv)
        entry->write.return_code = {};
    else if (name == "U.write.return_position"sv)
        entry->write.return_position = {};
    else if (name == "U.pattern"sv)
        entry->pattern = {};
    else
        return -EINVAL;

    return 0;
}

void XrdOssMirageXAttr::Free(AList *aPL)
{
}

int XrdOssMirageXAttr::Get(const char *Aname, void *Aval, int Avsz, const char *Path, int fd)
{
    if (this->oss == nullptr)
        return -ENOTSUP;

    const auto opt = oss->get_entry_read(Path);
    if (!opt.has_value())
        return -EINVAL;

    const auto entry = opt.value();

    const std::string_view name{Aname};
    std::string value{};

    if (name == "U.open.return_code"sv)
        value = std::to_string(entry.open.return_code);
    else if (name == "U.read.return_code"sv)
        value = std::to_string(entry.read.return_code);
    else if (name == "U.read.return_position"sv)
        value = std::to_string(entry.read.return_position);
    else if (name == "U.write.return_code"sv)
        value = std::to_string(entry.write.return_code);
    else if (name == "U.write.return_position"sv)
        value = std::to_string(entry.write.return_position);
    else if (name == "U.pattern"sv)
        value = entry.pattern;
    else
        return -EINVAL;

    const int num_bytes = std::min(static_cast<std::size_t>(Avsz), value.size());
    std::copy_n(value.begin(), num_bytes, static_cast<char *>(Aval));

    return num_bytes;
}

int XrdOssMirageXAttr::List(AList **aPL, const char *Path, int fd, int getSz)
{
    return -ENOTSUP;
}

int XrdOssMirageXAttr::Set(const char *Aname, const void *Aval, int Avsz, const char *Path,  int fd,  int isNew)
{
    if (this->oss == nullptr)
        return -ENOTSUP;

    const auto opt = oss->get_entry_write(Path);
    if (!opt.has_value())
        return -EINVAL;

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

void XrdOssMirageXAttr::setOss(XrdOssMirage &oss)
{
    if (this->oss == nullptr)
        this->oss = &oss;
}
