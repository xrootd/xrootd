
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
    bool buffer_accepted = false;
    int retval = size;
    if (offset < m_offset) {
        return SFS_ERROR;
    }
    if (offset == m_offset) {
        retval = m_fh->write(offset, buf, size);
        buffer_accepted = true;
        if (retval != SFS_ERROR) {
            m_offset += retval;
        }
        // If there are no in-use buffers, then we don't need to
        // do any accounting.
        if (m_avail_count == m_buffers.size()) {
            return retval;
        }
    }
    // Even if we already accepted the current data, always
    // iterate through available buffers and try to write as
    // much out to disk as possible.
    Entry *avail_entry;
    bool buffer_was_written;
    size_t avail_count = 0;
    do {
        avail_count = 0;
        avail_entry = nullptr;
        buffer_was_written = false;
        for (Entry &entry : m_buffers) {
            // Always try to dump from memory.
            if (entry.Write(*this) > 0) {
                buffer_was_written = true;
            }
            if (entry.Available()) { // Empty buffer
                if (!avail_entry) {avail_entry = &entry;}
                avail_count ++;
            }
            else if (!buffer_accepted && entry.Accept(offset, buf, size)) {
                buffer_accepted = true;
            }
        }
    } while ((avail_count != m_buffers.size()) && buffer_was_written);
    m_avail_count = avail_count;

    if (!buffer_accepted) {  // No place for this data in allocated buffers
        if (!avail_entry) {  // No available buffers to allocate.
            return SFS_ERROR;
        }
        if (!avail_entry->Accept(offset, buf, size)) {  // Empty buffer cannot accept?!?
            return SFS_ERROR;
        }
        m_avail_count --;
    }

    // If we have low buffer occupancy, then release memory.
    if ((m_buffers.size() > 2) && (m_avail_count * 2 > m_buffers.size())) {
        for (Entry &entry : m_buffers) {
            entry.ShrinkIfUnused();
        }
    }

    return retval;
}

int
Stream::Read(off_t offset, char *buf, size_t size)
{
    return m_fh->read(offset, buf, size);
}
