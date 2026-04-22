#include "XrdSimulatedFile.hh"

int XrdSimulatedFile::StatRet(struct stat *buff)
{
    return 0;
}

int XrdSimulatedFile::Clone(XrdOssDF& srcFile)
{
    return 0;
}

int XrdSimulatedFile::Clone(const std::vector<XrdOucCloneSeg> &cVec)
{
    return 0;
}

int XrdSimulatedFile::Fchmod(mode_t mode)
{
    return 0;
}

void XrdSimulatedFile::Flush() 
{
}

int XrdSimulatedFile::Fstat(struct stat *buf)
{
    return 0;
}

int XrdSimulatedFile::Fsync()
{
    return 0;
}

int XrdSimulatedFile::Fsync(XrdSfsAio *aiop)
{
    return 0;
}

int XrdSimulatedFile::Ftruncate(unsigned long long flen)
{
    return 0;
}

off_t XrdSimulatedFile::getMmap(void **addr)
{
    *addr = 0; 
    return 0;
}

int XrdSimulatedFile::Open(const char *path, int Oflag, mode_t Mode, XrdOucEnv &env)
{
    return 0;
}

ssize_t XrdSimulatedFile::pgRead (void* buffer, off_t offset, size_t rdlen, uint32_t* csvec, uint64_t opts)
{
    return 0;
}

int XrdSimulatedFile::pgRead (XrdSfsAio* aioparm, uint64_t opts)
{
    return 0;
}

ssize_t XrdSimulatedFile::pgWrite(void* buffer, off_t offset, size_t wrlen, uint32_t* csvec, uint64_t opts)
{
    return 0;
}

int XrdSimulatedFile::pgWrite(XrdSfsAio* aioparm, uint64_t opts)
{
    return 0;
}

ssize_t XrdSimulatedFile::Read(off_t offset, size_t size)
{
    return 0;
}

ssize_t XrdSimulatedFile::Read(void *buffer, off_t offset, size_t size)
{
    return 0;
}

int XrdSimulatedFile::Read(XrdSfsAio *aiop)
{
    return 0;
}

ssize_t XrdSimulatedFile::ReadRaw(void *buffer, off_t offset, size_t size)
{
    return 0;
}

ssize_t XrdSimulatedFile::Write(const void *buffer, off_t offset, size_t size)
{
    return 0;
}

int XrdSimulatedFile::Write(XrdSfsAio *aiop)
{
    return 0;
}

int XrdSimulatedFile::Close(long long *retsz)
{
    return 0;
}
