#include "XrdSimulatedFile.hh"
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

int XrdSimulatedFile::StatRet(struct stat *buff)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return 0;
}

int XrdSimulatedFile::Clone(XrdOssDF& srcFile)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return 0;
}

int XrdSimulatedFile::Clone(const std::vector<XrdOucCloneSeg> &cVec)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return 0;
}

int XrdSimulatedFile::Fchmod(mode_t mode)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return -ENOTSUP;
}

int XrdSimulatedFile::Fstat(struct stat *buf)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);

    buf->st_size = entry->size;

    return XrdOssOK;
}

int XrdSimulatedFile::Fsync()
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return XrdOssOK;
}

int XrdSimulatedFile::Fsync(XrdSfsAio *aiop)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return XrdOssOK;
}

int XrdSimulatedFile::Ftruncate(unsigned long long flen)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    entry->size = flen;
    return XrdOssOK;
}

off_t XrdSimulatedFile::getMmap(void **addr)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    *addr = 0; 
    return 0;
}

int XrdSimulatedFile::Open(const char *path, int Oflag, mode_t Mode, XrdOucEnv &env)
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

ssize_t XrdSimulatedFile::pgRead (void* buffer, off_t offset, size_t rdlen, uint32_t* csvec, uint64_t opts)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return 0;
}

int XrdSimulatedFile::pgRead (XrdSfsAio* aioparm, uint64_t opts)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return 0;
}

ssize_t XrdSimulatedFile::pgWrite(void* buffer, off_t offset, size_t wrlen, uint32_t* csvec, uint64_t opts)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return 0;
}

int XrdSimulatedFile::pgWrite(XrdSfsAio* aioparm, uint64_t opts)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return 0;
}

ssize_t XrdSimulatedFile::Read(off_t offset, size_t size)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return 0;
}

ssize_t XrdSimulatedFile::Read(void *buffer, off_t offset, size_t size)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);

    std::size_t read = std::min(size, entry->size - offset);

    std::memset(buffer, '0', read);

    XrdGlobal::Log.Say(std::to_string(read).c_str());

    return read;
}

int XrdSimulatedFile::Read(XrdSfsAio *aiop)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);

    return 0;
}

ssize_t XrdSimulatedFile::ReadRaw(void *buffer, off_t offset, size_t size)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return 0;
}

ssize_t XrdSimulatedFile::Write(const void *buffer, off_t offset, size_t size)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);

    entry->size += size;

    return size;
}

int XrdSimulatedFile::Write(XrdSfsAio *aiop)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);

    entry->size += aiop->sfsAio.aio_nbytes;
    aiop->Result = aiop->sfsAio.aio_nbytes;
    aiop->doneWrite();

    return XrdOssOK;
}

int XrdSimulatedFile::Close(long long *retsz)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);

    std::visit([](auto &l) {
        if (l.owns_lock())
            l.unlock();
    }, file_lock);

    return XrdOssOK;
}
