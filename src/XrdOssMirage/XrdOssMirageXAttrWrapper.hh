#ifndef __XRD_OSS_MIRAGE_XATTR_WRAPPER_HH__
#define __XRD_OSS_MIRAGE_XATTR_WRAPPER_HH__

#include "XrdOssMirageXAttr.hh"

#include <XrdSys/XrdSysXAttr.hh>

class XrdOucEnv;
class XrdSysError;

class XrdOssMirageXAttrWrapper : public XrdSysXAttr {
private:
    XrdOssMirageXAttr  mirage_xattr;
    XrdSysXAttr       &wrapped;

    XrdSysXAttr &route(const char *path);

public:
    XrdOssMirageXAttrWrapper(XrdSysError *errP, const char *config_fn, const char *parms, XrdOucEnv *envP, XrdSysXAttr *attrP);
    virtual ~XrdOssMirageXAttrWrapper() = default;

    virtual int  Del(const char *Aname, const char *Path, int fd=-1) override;
    virtual void Free(AList *aPL) override;
    virtual int  Get(const char *Aname, void *Aval, int Avsz, const char *Path, int fd=-1) override;
    virtual int  List(AList **aPL, const char *Path, int fd=-1, int getSz=0) override;
    virtual int  Set(const char *Aname, const void *Aval, int Avsz, const char *Path, int fd=-1, int isNew=0) override;

    void setOss(XrdOssMirage &oss);
};

#endif
