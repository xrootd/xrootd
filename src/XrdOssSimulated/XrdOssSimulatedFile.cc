#include "XrdOssSimulatedFile.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSfs/XrdSfsAio.hh"
#include "XrdSys/XrdSysXAttr.hh"
#include "XrdSys/XrdSysFAttr.hh"

#include <fcntl.h>

#include <algorithm>
#include <mutex>
#include <shared_mutex>

namespace XrdGlobal
{
    extern XrdSysError Log;
}

int XrdOssSimulatedFile::StatRet(struct stat *buff)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return 0;
}

int XrdOssSimulatedFile::Clone(XrdOssDF& srcFile)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return 0;
}

int XrdOssSimulatedFile::Clone(const std::vector<XrdOucCloneSeg> &cVec)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return 0;
}

int XrdOssSimulatedFile::Fchmod(mode_t mode)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return -ENOTSUP;
}

int XrdOssSimulatedFile::Fstat(struct stat *buf)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);

    buf->st_size = entry->size;

    return XrdOssOK;
}

int XrdOssSimulatedFile::Fsync()
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return XrdOssOK;
}

int XrdOssSimulatedFile::Fsync(XrdSfsAio *aiop)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return XrdOssOK;
}

int XrdOssSimulatedFile::Ftruncate(unsigned long long flen)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    entry->size = flen;
    return XrdOssOK;
}

off_t XrdOssSimulatedFile::getMmap(void **addr)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    *addr = 0; 
    return 0;
}

int XrdOssSimulatedFile::Open(const char *path, int Oflag, mode_t Mode, XrdOucEnv &env)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);

    std::scoped_lock lock(oss->mutex);

    if (!oss->entries.contains(path))
        return -ENOENT;

    entry = &oss->entries[path];

    if ((Oflag & O_ACCMODE) == O_RDONLY)
        file_lock = std::shared_lock<std::shared_mutex>(*this->entry->mutex, std::try_to_lock);
    else
        file_lock = std::unique_lock<std::shared_mutex>(*this->entry->mutex, std::try_to_lock);

    if (!std::visit([](auto &l) { return l.owns_lock(); }, file_lock))
        return -EBUSY;

    return XrdOssOK;
}

ssize_t XrdOssSimulatedFile::pgRead (void* buffer, off_t offset, size_t rdlen, uint32_t* csvec, uint64_t opts)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return 0;
}

int XrdOssSimulatedFile::pgRead (XrdSfsAio* aioparm, uint64_t opts)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return 0;
}

ssize_t XrdOssSimulatedFile::pgWrite(void* buffer, off_t offset, size_t wrlen, uint32_t* csvec, uint64_t opts)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return 0;
}

int XrdOssSimulatedFile::pgWrite(XrdSfsAio* aioparm, uint64_t opts)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return 0;
}

ssize_t XrdOssSimulatedFile::Read(off_t offset, size_t size)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return XrdOssOK;
}

ssize_t XrdOssSimulatedFile::Read(void *buffer, off_t offset, size_t size)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);

    std::size_t read = std::min(size, entry->size - offset);
    std::memset(buffer, '0', read);

    return read;
}

int XrdOssSimulatedFile::Read(XrdSfsAio *aiop)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);

    std::size_t read = std::min(aiop->sfsAio.aio_nbytes, entry->size - aiop->sfsAio.aio_offset);
    std::memset(const_cast<void *>(aiop->sfsAio.aio_buf), '1', read);

    aiop->Result = read;
    aiop->doneRead();

    return XrdOssOK;
}

ssize_t XrdOssSimulatedFile::ReadRaw(void *buffer, off_t offset, size_t size)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return 0;
}

ssize_t XrdOssSimulatedFile::Write(const void *buffer, off_t offset, size_t size)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);

    entry->size += size;

    return size;
}

int XrdOssSimulatedFile::Write(XrdSfsAio *aiop)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);

    entry->size += aiop->sfsAio.aio_nbytes;
    aiop->Result = aiop->sfsAio.aio_nbytes;
    aiop->doneWrite();

    return XrdOssOK;
}

int XrdOssSimulatedFile::Close(long long *retsz)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);

    std::visit([](auto &l) {
        if (l.owns_lock())
            l.unlock();
    }, file_lock);

    return XrdOssOK;
}
