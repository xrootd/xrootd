
#include "stream.hh"

#include "XrdSfs/XrdSfsInterface.hh"

using namespace TPC;

Stream::~Stream()
{
    m_fh->close();
}


int
Stream::Stat(struct stat* buf)
{
    return m_fh->stat(buf);
}

int
Stream::Write(off_t offset, const char *buf, size_t size)
{
    return m_fh->write(offset, buf, size);
}

int
Stream::Read(off_t offset, char *buf, size_t size)
{
    return m_fh->read(offset, buf, size);
}
