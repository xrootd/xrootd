#include "XrdOssMirageWrapperDir.hh"
#include "XrdOssMirageWrapper.hh"

XrdOssMirageWrapperDir::XrdOssMirageWrapperDir(XrdOssMirage &mirage, std::unique_ptr<XrdOssDF> wrapped_dir, const char *tident) :
    XrdOssWrapDF(*wrapped_dir),
    mirage(mirage),
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
