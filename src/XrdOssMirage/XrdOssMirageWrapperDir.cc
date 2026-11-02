#include "XrdOssMirageWrapperDir.hh"

XrdOssMirageWrapperDir::XrdOssMirageWrapperDir(XrdOssMirage &mirage, std::unique_ptr<XrdOssDF> wrapped_dir, const char *tident) :
    XrdOssWrapDF(*wrapped_dir),
    XrdOssMirageWrapperBase(mirage),
    wrapped_dir(std::move(wrapped_dir)),
    active(this->wrapped_dir.get())
{
}

int XrdOssMirageWrapperDir::Opendir(const char *path, XrdOucEnv &env)
{
    if (is_mirage_path(path))
    {
        mirage_dir.reset(mirage.newDir(tident));
        active = mirage_dir.get();
    }
    
    return active->Opendir(path, env);
}

int XrdOssMirageWrapperDir::Readdir(char *buff, int blen)
{
    return active->Readdir(buff, blen);
}

int XrdOssMirageWrapperDir::StatRet(struct stat *buff)
{
    return active->StatRet(buff);
}

int XrdOssMirageWrapperDir::Close(long long *retsz)
{
    return active->Close(retsz);
}

void XrdOssMirageWrapperDir::Flush()
{
    active->Flush();
}

int XrdOssMirageWrapperDir::Fctl(int cmd, int alen, const char *args, char **resp)
{
    return active->Fctl(cmd, alen, args, resp);
}

bool XrdOssMirageWrapperDir::getErrMsg(std::string &eText)
{
    return active->getErrMsg(eText);
}

int XrdOssMirageWrapperDir::getFD()
{
    return active->getFD();
}
