#include "XrdSimulatedXAttr.hh"
#include "XrdVersion.hh"

#include "XrdSys/XrdSysError.hh"

namespace XrdGlobal
{
    extern XrdSysError Log;
}

XrdVERSIONINFO(XrdSysGetXAttrObject, XrdSimulatedXAttr);

extern "C"
{
    XrdSysXAttr* XrdSysGetXAttrObject(XrdSysError  *errP, const char   *config_fn, const char   *parms)
    {
        return new XrdSimulatedXAttr();
    }
}

int XrdSimulatedXAttr::Del(const char *Aname, const char *Path, int fd)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return -ENOTSUP;
}

void XrdSimulatedXAttr::Free(AList *aPL)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
}

int XrdSimulatedXAttr::Get(const char *Aname, void *Aval, int Avsz, const char *Path,  int fd)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return -ENOTSUP;
}

int XrdSimulatedXAttr::List(AList **aPL, const char *Path, int fd, int getSz)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return -ENOTSUP;
}

int XrdSimulatedXAttr::Set(const char *Aname, const void *Aval, int Avsz, const char *Path,  int fd,  int isNew)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return -ENOTSUP;
}
