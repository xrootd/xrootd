#include "XrdOssSimulatedXAttr.hh"
#include "XrdVersion.hh"

#include "XrdSys/XrdSysError.hh"

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

int XrdOssSimulatedXAttr::Get(const char *Aname, void *Aval, int Avsz, const char *Path,  int fd)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return -ENOTSUP;
}

int XrdOssSimulatedXAttr::List(AList **aPL, const char *Path, int fd, int getSz)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return -ENOTSUP;
}

int XrdOssSimulatedXAttr::Set(const char *Aname, const void *Aval, int Avsz, const char *Path,  int fd,  int isNew)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return -ENOTSUP;
}
