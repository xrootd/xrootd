#include "XrdOssMirageWrapperFile.hh"
#include "XrdOssMirageWrapper.hh"

XrdOssMirageWrapperFile::XrdOssMirageWrapperFile(XrdOssMirage &mirage, std::unique_ptr<XrdOssDF> wrapped_file, const char *tident) :
    XrdOssWrapDF(*wrapped_file),
    mirage(mirage),
    wrapped_file(std::move(wrapped_file)),
    active(this->wrapped_file.get())
{
}

int XrdOssMirageWrapperFile::Open(const char *path, int Oflag, mode_t Mode, XrdOucEnv &env)
{
    if (is_mirage_path(path))
    {
        mirage_file.reset(mirage.newFile(tident));
        active = mirage_file.get();
    }
    
    return active->Open(path, Oflag, Mode, env);
}

int XrdOssMirageWrapperFile::StatRet(struct stat *buff)
{
    return active->StatRet(buff);
}

int XrdOssMirageWrapperFile::Fchmod(mode_t mode)
{
    return active->Fchmod(mode);
}

int XrdOssMirageWrapperFile::Fstat(struct stat *buf)
{
    return active->Fstat(buf);
}

int XrdOssMirageWrapperFile::Fsync()
{
    return active->Fsync();
}

int XrdOssMirageWrapperFile::Fsync(XrdSfsAio *aiop)
{
    return active->Fsync(aiop);
}

int XrdOssMirageWrapperFile::Ftruncate(unsigned long long flen)
{
    return active->Ftruncate(flen);
}

ssize_t XrdOssMirageWrapperFile::pgRead(void *buffer, off_t offset, size_t rdlen, uint32_t *csvec, uint64_t opts)
{
    return active->pgRead(buffer, offset, rdlen, csvec, opts);
}

int XrdOssMirageWrapperFile::pgRead(XrdSfsAio *aioparm, uint64_t opts)
{
    return active->pgRead(aioparm, opts);
}

ssize_t XrdOssMirageWrapperFile::pgWrite(void *buffer, off_t offset, size_t wrlen, uint32_t *csvec, uint64_t opts)
{
    return active->pgWrite(buffer, offset, wrlen, csvec, opts);
}

int XrdOssMirageWrapperFile::pgWrite(XrdSfsAio *aioparm, uint64_t opts)
{
    return active->pgWrite(aioparm, opts);
}

ssize_t XrdOssMirageWrapperFile::Read(off_t offset, size_t size)
{
    return active->Read(offset, size);
}

ssize_t XrdOssMirageWrapperFile::Read(void *buffer, off_t offset, size_t size)
{
    return active->Read(buffer, offset, size);
}

int XrdOssMirageWrapperFile::Read(XrdSfsAio *aiop)
{
    return active->Read(aiop);
}

ssize_t XrdOssMirageWrapperFile::ReadRaw(void *buffer, off_t offset, size_t size)
{
    return active->ReadRaw(buffer, offset, size);
}

ssize_t XrdOssMirageWrapperFile::Write(const void *buffer, off_t offset, size_t size)
{
    return active->Write(buffer, offset, size);
}

int XrdOssMirageWrapperFile::Write(XrdSfsAio *aiop)
{
    return active->Write(aiop);
}

int XrdOssMirageWrapperFile::Close(long long *retsz)
{
    return active->Close(retsz);
}

int XrdOssMirageWrapperFile::getFD()
{
    return active->getFD();
}
