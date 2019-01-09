
#include <sstream>

#include "XrdTpcStream.hh"

#include "XrdSfs/XrdSfsInterface.hh"
#include "XrdSys/XrdSysError.hh"

using namespace TPC;

Stream::~Stream()
{
    for (std::vector<Entry*>::iterator buffer_iter = m_buffers.begin();
        buffer_iter != m_buffers.end();
        buffer_iter++) {
        delete *buffer_iter;
        *buffer_iter = NULL;
    }
    m_fh->close();
}


bool
Stream::Finalize()
{
    // Do not close twice
    if (!m_open_for_write) {
        return false;
    }
    for (std::vector<Entry*>::iterator buffer_iter = m_buffers.begin();
        buffer_iter != m_buffers.end();
        buffer_iter++) {
        delete *buffer_iter;
        *buffer_iter = NULL;
    }
    m_fh->close();
    m_open_for_write = false;
    // If there are outstanding buffers to reorder, finalization failed
    return m_avail_count == m_buffers.size();
}


int
Stream::Stat(struct stat* buf)
{
    return m_fh->stat(buf);
}

int
Stream::Write(off_t offset, const char *buf, size_t size)
{
    if (!m_open_for_write) return SFS_ERROR;
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
        avail_entry = NULL;
        buffer_was_written = false;
        for (std::vector<Entry*>::iterator entry_iter = m_buffers.begin();
             entry_iter != m_buffers.end();
             entry_iter++) {
            // Always try to dump from memory.
            if ((*entry_iter)->Write(*this) > 0) {
                buffer_was_written = true;
            }
            if ((*entry_iter)->Available()) { // Empty buffer
                if (!avail_entry) {avail_entry = *entry_iter;}
                avail_count ++;
            }
            else if (!buffer_accepted && (*entry_iter)->Accept(offset, buf, size)) {
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
        for (std::vector<Entry*>::iterator entry_iter = m_buffers.begin();
             entry_iter != m_buffers.end();
             entry_iter++) {
            (*entry_iter)->ShrinkIfUnused();
        }
    }

    return retval;
}


void
Stream::DumpBuffers() const
{
    m_log.Emsg("Stream::DumpBuffers", "Beginning dump of stream buffers.");
    size_t idx = 0;
    for (std::vector<Entry*>::const_iterator entry_iter = m_buffers.begin();
         entry_iter!= m_buffers.end();
         entry_iter++) {
        std::stringstream ss;
        ss << "Buffer " << idx << ": Offset=" << (*entry_iter)->GetOffset() << ", Size="
           << (*entry_iter)->GetSize() << ", Capacity=" << (*entry_iter)->GetCapacity();
        m_log.Emsg("Stream::DumpBuffers", ss.str().c_str());
        idx ++;
    }
    m_log.Emsg("Stream::DumpBuffers", "Finish dump of stream buffers.");
}


int
Stream::Read(off_t offset, char *buf, size_t size)
{
    return m_fh->read(offset, buf, size);
}
