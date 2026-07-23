#include "XrdOssMirageXAttr.hh"
#include "XrdVersion.hh"

#include <algorithm>
#include <stdexcept>
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
        entry->set_pattern({});
    else
        return -ENODATA;

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

    if (name.compare(0, 7, "XrdCks.") == 0)
    {
        if (entry.checksum.find(name.data()) == entry.checksum.end())
            return -ENODATA;

        auto value = entry.checksum.at(name.data());

        const int num_bytes = std::min(static_cast<std::size_t>(Avsz), value.size());
        std::copy_n(value.begin(), num_bytes, static_cast<char *>(Aval));

        return num_bytes;
    }

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
        value = entry.pattern();
    else
        return -ENODATA;

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
    if (isNew == MIRAGE_MAGIC)
    {
        static std::once_flag xattr_injection_flag;
        std::call_once(xattr_injection_flag, [this, Aval]() noexcept
            {
                auto value = static_cast<XrdOss *>(const_cast<void *>(Aval));
                if (XrdOssMirage * const oss = dynamic_cast<XrdOssMirage *>(value); oss != nullptr)
                    this->oss = oss;
            });

        return 0;
    }

    if (this->oss == nullptr)
        return -ENOTSUP;

    const auto opt = oss->get_entry_write(Path);
    if (!opt.has_value())
        return -EINVAL;

    auto entry = opt.value();

    const std::string_view name{Aname};

    if (name.compare(0, 7, "XrdCks.") == 0)
    {
        const std::vector<char> value(static_cast<const char *>(Aval), static_cast<const char *>(Aval) + Avsz);
        entry->checksum.insert_or_assign(name.data(), value);
        return 0;
    }

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
            entry->set_pattern(value);
        else
            return -ENODATA;
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
