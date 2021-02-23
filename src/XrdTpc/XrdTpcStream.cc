
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
    m_open_for_write = false;

    for (std::vector<Entry*>::iterator buffer_iter = m_buffers.begin();
        buffer_iter != m_buffers.end();
        buffer_iter++) {
        delete *buffer_iter;
        *buffer_iter = NULL;
    }

    if (m_fh->close() == SFS_ERROR) {
        std::stringstream ss;
        const char *msg = m_fh->error.getErrText();
        if (!msg || (*msg == '\0')) {msg = "(no error message provided)";}
        ss << "Failure when closing file handle: " << msg << " (code=" << m_fh->error.getErrInfo() << ")";
        m_error_buf = ss.str();
        return false;
    }

    // If there are outstanding buffers to reorder, finalization failed
    return m_avail_count == m_buffers.size();
}


int
Stream::Stat(struct stat* buf)
{
    return m_fh->stat(buf);
}

ssize_t
Stream::Write(off_t offset, const char *buf, size_t size, bool force)
{
/*
 *  NOTE: these lines are useful for debuggin the state of the buffer
 *  management code; too expensive to compile in and have a runtime switch.
    std::stringstream ss;
    ss << "Offset=" << offset << ", Size=" << size << ", force=" << force;
    m_log.Emsg("Stream::Write", ss.str().c_str());
    DumpBuffers();
*/
    if (!m_open_for_write) {
        if (!m_error_buf.size()) {m_error_buf = "Logic error: writing to a buffer not opened for write";}
        return SFS_ERROR;
    }
    size_t bytes_accepted = 0;
    int retval = size;
    if (offset < m_offset) {
        if (!m_error_buf.size()) {m_error_buf = "Logic error: writing to a prior offset";}
        return SFS_ERROR;
    }
    // If this is write is appending to the stream and
    // MB-aligned, then we write it to disk; otherwise, the
    // data will be buffered.
    if (offset == m_offset && (force || (size && !(size % (1024*1024))))) {
        retval = WriteImpl(offset, buf, size);
        bytes_accepted = retval;
            // On failure, we don't care about flushing buffers from memory --
            // the stream is now invalid.
        if (retval < 0) {
            return retval;
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
            // Always try to dump from memory; when size == 0, then we are
            // going to force a flush even if things are not MB-aligned.
            int retval2 = (*entry_iter)->Write(*this, size == 0);
            if (retval2 == SFS_ERROR) {
                if (!m_error_buf.size()) {m_error_buf = "Unknown filesystem write failure.";}
                return retval2;
            }
            buffer_was_written |= retval2 > 0;
            if ((*entry_iter)->Available()) { // Empty buffer
                if (!avail_entry) {avail_entry = *entry_iter;}
                avail_count ++;
            }
            else if (bytes_accepted != size && size) {
                size_t new_accept = (*entry_iter)->Accept(offset + bytes_accepted, buf + bytes_accepted, size - bytes_accepted);
                    // Partial accept; buffer should be writable which means we should free it up
                    // for next iteration
                if (new_accept && new_accept != size - bytes_accepted) {
                    int retval3 = (*entry_iter)->Write(*this, false);
                    if (retval3 == SFS_ERROR) {
                        if (!m_error_buf.size()) {m_error_buf = "Unknown filesystem write failure.";}
                        return SFS_ERROR;
                    }
                    buffer_was_written = true;
                }
                bytes_accepted += new_accept;
            }
        }
    } while ((avail_count != m_buffers.size()) && buffer_was_written);
    m_avail_count = avail_count;

    if (bytes_accepted != size && size) {  // No place for this data in allocated buffers
        if (!avail_entry) {  // No available buffers to allocate; logic error, should not happen.
            DumpBuffers();
            m_error_buf = "No empty buffers available to place unordered data.";
            return SFS_ERROR;
        }
        if (avail_entry->Accept(offset + bytes_accepted, buf + bytes_accepted, size - bytes_accepted) != size - bytes_accepted) {  // Empty buffer cannot accept?!?
            m_error_buf = "Empty re-ordering buffer was unable to to accept data; internal logic error.";
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


ssize_t Stream::WriteImpl(off_t offset, const char *buf, size_t size)
{
    ssize_t retval;
    if (size == 0) {return 0;}
    retval = m_fh->write(offset, buf, size);
    if (retval != SFS_ERROR) {
        m_offset += retval;
    } else {
        std::stringstream ss;
        const char *msg = m_fh->error.getErrText();
        if (!msg || (*msg == '\0')) {msg = "(no error message provided)";}
        ss << msg << " (code=" << m_fh->error.getErrInfo() << ")";
        m_error_buf = ss.str();
    }
    return retval;
}


void
Stream::DumpBuffers() const
{
    m_log.Emsg("Stream::DumpBuffers", "Beginning dump of stream buffers.");
    {
        std::stringstream ss;
        ss << "Stream offset: " << m_offset;
        m_log.Emsg("Stream::DumpBuffers", ss.str().c_str());
    }
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
