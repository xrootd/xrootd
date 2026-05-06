#ifndef __XRD_SIMULATED_XATTR__
#define __XRD_SIMULATED_XATTR__

#include "XrdOssSimulated.hh"

#include <XrdOss/XrdOss.hh>

#include <XrdSys/XrdSysXAttr.hh>

class XrdOssSimulatedXAttr : public XrdSysXAttr
{
private:
    XrdOssSimulated *oss = nullptr;

public:
    XrdOssSimulatedXAttr() = default;
    virtual ~XrdOssSimulatedXAttr() = default;

    virtual int  Del(const char *Aname, const char *Path, int fd=-1) override;
    virtual void Free(AList *aPL) override;
    virtual int  Get(const char *Aname, void *Aval, int Avsz, const char *Path,  int fd=-1) override;
    virtual int  List(AList **aPL, const char *Path, int fd=-1, int getSz=0) override;
    virtual int  Set(const char *Aname, const void *Aval, int Avsz, const char *Path,  int fd=-1,  int isNew=0) override;

    void setOss(XrdOssSimulated &oss);
};

#endif
