#include "XrdOssMirageXAttrWrapper.hh"
#include "XrdOssMirageWrapper.hh"

#include "XrdVersion.hh"

XrdVERSIONINFO(XrdSysAddXAttrObject, XrdOssMirageXAttrWrapper);

extern "C"
{
    XrdSysXAttr *XrdSysAddXAttrObject(XrdSysError *errP, const char *config_fn,
                                      const char *parms, XrdOucEnv *envP,
                                      XrdSysXAttr *attrP)
    {
        return new XrdOssMirageXAttrWrapper(errP, config_fn, parms, envP, attrP);
    }
}

XrdOssMirageXAttrWrapper::XrdOssMirageXAttrWrapper(XrdSysError *errP, const char *config_fn, const char *parms, XrdOucEnv *envP, XrdSysXAttr *attrP) :
    wrapped(*attrP)
{
    // Fix the immutable prefix from the configuration at instantiation time.
    mirage_prefix(parms);
}

XrdSysXAttr &XrdOssMirageXAttrWrapper::route(const char *path)
{
    return is_mirage_path(path) ? static_cast<XrdSysXAttr &>(mirage_xattr) : wrapped;
}

int XrdOssMirageXAttrWrapper::Del(const char *Aname, const char *Path, int fd)
{
    return route(Path).Del(Aname, Path, fd);
}

void XrdOssMirageXAttrWrapper::Free(AList *aPL)
{
    // XrdOssMirage never returns an AList from List(), so anything that needs
    // freeing was produced by the wrapped plugin.
    wrapped.Free(aPL);
}

int XrdOssMirageXAttrWrapper::Get(const char *Aname, void *Aval, int Avsz, const char *Path, int fd)
{
    return route(Path).Get(Aname, Aval, Avsz, Path, fd);
}

int XrdOssMirageXAttrWrapper::List(AList **aPL, const char *Path, int fd, int getSz)
{
    return route(Path).List(aPL, Path, fd, getSz);
}

int XrdOssMirageXAttrWrapper::Set(const char *Aname, const void *Aval, int Avsz, const char *Path, int fd, int isNew)
{
    return route(Path).Set(Aname, Aval, Avsz, Path, fd, isNew);
}

void XrdOssMirageXAttrWrapper::setOss(XrdOssMirage &oss)
{
    mirage_xattr.setOss(oss);
}
