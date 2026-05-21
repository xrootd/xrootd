#include "XrdOssMirageFile.hh"
#include "XrdSfs/XrdSfsAio.hh"
#include "XrdSys/XrdSysXAttr.hh"
#include "XrdSys/XrdSysFAttr.hh"

#include <fcntl.h>

#include <algorithm>
#include <mutex>
#include <span>

XrdOssMirageFile::XrdOssMirageFile(XrdOssMirage &oss) :
    oss(oss),
    entry(&std::get<XrdOssMirageEntry>(entry_storage))
{
}

int XrdOssMirageFile::StatRet(struct stat *buff)
{
    return -ENOTDIR;
}

int XrdOssMirageFile::Clone(XrdOssDF& srcFile)
{
    return -ENOTSUP;
}

int XrdOssMirageFile::Clone(const std::vector<XrdOucCloneSeg> &cVec)
{
    return -ENOTSUP;
}

int XrdOssMirageFile::Fchmod(mode_t mode)
{
    return -ENOTSUP;
}

int XrdOssMirageFile::Fstat(struct stat *buf)
{
    buf->st_size = entry->size;
    return XrdOssOK;
}

int XrdOssMirageFile::Fsync()
{
    return XrdOssOK;
}

int XrdOssMirageFile::Fsync(XrdSfsAio *aiop)
{
    return -ENOTSUP;
}

int XrdOssMirageFile::Ftruncate(unsigned long long flen)
{
    entry->size = flen;
    return XrdOssOK;
}

int XrdOssMirageFile::Open(const char *path, int Oflag, mode_t Mode, XrdOucEnv &env)
{
    if ((Oflag & O_ACCMODE) == O_RDONLY)
    {
        const auto opt = oss.getEntryRead(path);
        if (!opt.has_value())
            return -ENOENT;

        entry_storage = opt.value();
        entry = &std::get<XrdOssMirageEntry>(entry_storage);
    }
    else
    {
        const auto opt = oss.getEntryWrite(path);
        if (!opt.has_value())
            return -ENOENT;

        entry_storage = opt.value();
        entry = &*std::get<XrdOssMirageEntryPtr>(entry_storage);
    }

    if (entry->open.return_code != XrdOssOK)
        return -entry->open.return_code;

    return XrdOssOK;
}

ssize_t XrdOssMirageFile::pgRead (void* buffer, off_t offset, size_t rdlen, uint32_t* csvec, uint64_t opts)
{
    return -ENOTSUP;
}

int XrdOssMirageFile::pgRead (XrdSfsAio* aioparm, uint64_t opts)
{
    return -ENOTSUP;
}

ssize_t XrdOssMirageFile::pgWrite(void* buffer, off_t offset, size_t wrlen, uint32_t* csvec, uint64_t opts)
{
    return -ENOTSUP;
}

int XrdOssMirageFile::pgWrite(XrdSfsAio* aioparm, uint64_t opts)
{
    return -ENOTSUP;
}

ssize_t XrdOssMirageFile::Read(off_t offset, size_t size)
{
    return XrdOssOK;
}

ssize_t XrdOssMirageFile::Read(void *buffer, off_t offset, size_t size)
{
    if (entry->read.return_code != XrdOssOK && 
        entry->read.return_position >= static_cast<std::size_t>(offset) && 
        entry->read.return_position <= static_cast<std::size_t>(offset + size))
        return -entry->read.return_code;

    const std::span output(static_cast<char *>(buffer), size);

    const std::size_t num_bytes = std::min(output.size(), entry->size - offset);

    if (entry->pattern.size() == 1)
        std::fill_n(output.begin(), num_bytes, entry->pattern.front());

    if (entry->pattern.size() > 1)
        std::generate(output.begin(), output.begin() + num_bytes, [i = offset % entry->pattern.size(), this] () mutable {
            return entry->pattern[i++ % entry->pattern.size()]; 
        });

    return num_bytes;
}

int XrdOssMirageFile::Read(XrdSfsAio *aiop)
{
    return -ENOTSUP;
}

ssize_t XrdOssMirageFile::ReadRaw(void *buffer, off_t offset, size_t size)
{
    return Read(buffer, offset, size);
}

ssize_t XrdOssMirageFile::Write(const void *buffer, off_t offset, size_t size)
{
    if (entry->write.return_code != XrdOssOK && 
        entry->write.return_position >= static_cast<std::size_t>(offset) && 
        entry->write.return_position <= static_cast<std::size_t>(offset + size))
        return -entry->write.return_code;

    entry->size += size;
    return size;
}

int XrdOssMirageFile::Write(XrdSfsAio *aiop)
{
    return -ENOTSUP;
}

int XrdOssMirageFile::Close(long long *retsz)
{
    if (std::holds_alternative<XrdOssMirageEntryPtr>(entry_storage))
        std::get<XrdOssMirageEntryPtr>(entry_storage).reset();
    return XrdOssOK;
}
