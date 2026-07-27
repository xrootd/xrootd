#ifndef __XRD_OSS_MIRAGE_XATTR_HH__
#define __XRD_OSS_MIRAGE_XATTR_HH__

#include "XrdOssMirage.hh"

#include <XrdOss/XrdOss.hh>

#include <XrdSys/XrdSysXAttr.hh>

#include <tuple>

using TupleChecksum = std::tuple<std::string, std::string, std::size_t>;

struct TupleHash {
    template <typename... Ts>
    std::size_t operator()(const std::tuple<Ts...>& t) const {
        std::size_t seed = 0;
        std::apply([&](const auto&... args) {
            ((seed = seed * 31 + std::hash<std::decay_t<decltype(args)>>{}(args)), ...);
        }, t);
        return seed;
    }
};

class XrdOssMirageXAttr : public XrdSysXAttr
{
private:
    XrdOssMirage *oss{nullptr};

    std::unordered_map<TupleChecksum, std::string, TupleHash> checksum{};

public:
    XrdOssMirageXAttr() = default;
    virtual ~XrdOssMirageXAttr() = default;

    virtual int  Del(const char *Aname, const char *Path, int fd=-1) override;
    virtual void Free(AList *aPL) override;
    virtual int  Get(const char *Aname, void *Aval, int Avsz, const char *Path,  int fd=-1) override;
    virtual int  List(AList **aPL, const char *Path, int fd=-1, int getSz=0) override;
    virtual int  Set(const char *Aname, const void *Aval, int Avsz, const char *Path,  int fd=-1,  int isNew=0) override;

    void setOss(XrdOssMirage &oss);
};

#endif
