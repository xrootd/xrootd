
#include "XrdSfs/XrdSfsAio.hh"
#include "XrdSec/XrdSecEntity.hh"

#include "XrdThrottle.hh"

using namespace XrdThrottle;

File::File(const char *user, int monid, std::auto_ptr<XrdSfsFile> sfs)
   : m_sfs(sfs), // Guaranteed to be non-null by FileSystem::newFile
     m_uid(0)
{}

File::~File()
{}

int
File::open(const char                *fileName,
                 XrdSfsFileOpenMode   openMode,
                 mode_t               createMode,
           const XrdSecEntity        *client,
           const char                *opaque)
{
   m_uid = XrdThrottleManager::GetUid(client->name);
   return m_sfs->open(fileName, openMode, createMode, client, opaque);
}

int
File::close()
{
   return m_sfs->close();
}

int
File::fctl(const int               cmd,
           const char             *args,
                 XrdOucErrInfo    &out_error)
{
   return m_sfs->fctl(cmd, args, out_error);
}

const char *
File::FName()
{
   return m_sfs->FName();
}

int
File::getMmap(void **Addr, off_t &Size)
{  // We cannot monitor mmap-based reads, so we disable them.
   return SFS_ERROR;
}

int
File::read(XrdSfsFileOffset   fileOffset,
           XrdSfsXferSize     amount)
{
   return m_sfs->read(fileOffset, amount);
}

XrdSfsXferSize
File::read(XrdSfsFileOffset   fileOffset,
           char              *buffer,
           XrdSfsXferSize     buffer_size)
{
   return m_sfs->read(fileOffset, buffer, buffer_size);
}

int
File::read(XrdSfsAio *aioparm)
{  // We disable all AIO-based reads.
   aioparm->Result = this->read((XrdSfsFileOffset)aioparm->sfsAio.aio_offset,
                                          (char *)aioparm->sfsAio.aio_buf,
                                  (XrdSfsXferSize)aioparm->sfsAio.aio_nbytes);
   aioparm->doneRead();
   return SFS_OK;
}

XrdSfsXferSize
File::write(      XrdSfsFileOffset   fileOffset,
            const char              *buffer,
                  XrdSfsXferSize     buffer_size)
{
   return m_sfs->write(fileOffset, buffer, buffer_size);
}

int
File::write(XrdSfsAio *aioparm)
{
   aioparm->Result = this->write((XrdSfsFileOffset)aioparm->sfsAio.aio_offset,
                                           (char *)aioparm->sfsAio.aio_buf,
                                   (XrdSfsXferSize)aioparm->sfsAio.aio_nbytes);
   aioparm->doneRead();
   return SFS_OK;

   return m_sfs->write(aioparm);
}

int
File::sync()
{
   return m_sfs->sync();
}

int
File::sync(XrdSfsAio *aiop)
{
   aiop->Result = this->sync();
   aiop->doneWrite();
   return m_sfs->sync(aiop);
}

int
File::stat(struct stat *buf)
{
   return m_sfs->stat(buf);
}

int
File::truncate(XrdSfsFileOffset   fileOffset)
{
   return m_sfs->truncate(fileOffset);
}

int
File::getCXinfo(char cxtype[4], int &cxrsz)
{
   return m_sfs->getCXinfo(cxtype, cxrsz);
}

