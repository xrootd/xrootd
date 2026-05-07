#include "XrdOssSimulatedFile.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSfs/XrdSfsAio.hh"
#include "XrdSys/XrdSysXAttr.hh"
#include "XrdSys/XrdSysFAttr.hh"

#include <fcntl.h>

#include <algorithm>
#include <mutex>
#include <span>

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
    return -ENOTSUP;
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

    auto opt = oss.getEntry(path);
    if (!opt.has_value())
        return -ENOENT;

    entry = opt.value();

    if (entry->open_return_code != XrdOssOK)
        return -entry->open_return_code;

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
    return -ENOTSUP;
}

ssize_t XrdOssSimulatedFile::pgWrite(void* buffer, off_t offset, size_t wrlen, uint32_t* csvec, uint64_t opts)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return 0;
}

int XrdOssSimulatedFile::pgWrite(XrdSfsAio* aioparm, uint64_t opts)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return -ENOTSUP;
}

ssize_t XrdOssSimulatedFile::Read(off_t offset, size_t size)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return XrdOssOK;
}

ssize_t XrdOssSimulatedFile::Read(void *buffer, off_t offset, size_t size)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);

    std::span output(static_cast<char *>(buffer), size);

    std::size_t num_bytes = std::min(output.size(), entry->size - offset);

    if (entry->pattern.size() == 1)
        std::fill_n(output.begin(), num_bytes, entry->pattern.front());

    if (entry->pattern.size() > 1)
    {
        if (read_cache.empty())
        {
            read_cache.resize(((output.size() / entry->pattern.size()) + 1) * entry->pattern.size());
            std::generate(read_cache.begin(), read_cache.end(), [i = 0, this] () mutable {
                return entry->pattern[i++ % entry->pattern.size()]; 
            });
        }

        std::copy_n(read_cache.begin() + (offset % entry->pattern.size()), num_bytes, output.begin());
    }

    return num_bytes;
}

int XrdOssSimulatedFile::Read(XrdSfsAio *aiop)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);
    return -ENOTSUP;
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
    return -ENOTSUP;
}

int XrdOssSimulatedFile::Close(long long *retsz)
{
    XrdGlobal::Log.Say(__PRETTY_FUNCTION__);

    return XrdOssOK;
}
