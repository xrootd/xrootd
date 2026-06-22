
#include <sstream>

#include "XrdHttpTpcStream.hh"

#include "XrdSfs/XrdSfsInterface.hh"
#include "XrdSys/XrdSysError.hh"

using namespace TPC;

Stream::~Stream()
{
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

    // If there are outstanding buffers to reorder, finalization failed; the
    // check has to happen before the buffers are released.
    bool all_buffers_returned = m_avail_count == m_buffers.size();
    m_buffers.clear();

    if (m_fh->close() == SFS_ERROR) {
        std::stringstream ss;
        const char *msg = m_fh->error.getErrText();
        if (!msg || (*msg == '\0')) {msg = "(no error message provided)";}
        ss << "Failure when closing file handle: " << msg << " (code=" << m_fh->error.getErrInfo() << ")";
        m_error_buf = ss.str();
        return false;
    }

    return all_buffers_returned;
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
    ssize_t retval = size;
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
    // Even if we already accepted the current data, always iterate through the
    // buffers and try to write as much out to disk as possible.
    //
    // Accepting data can complete a buffer, and flushing a buffer advances
    // m_offset, which can in turn let another buffer accept more data or become
    // writable.  Alternate between the two until neither makes progress.  When
    // size == 0 we force a flush even if things are not MB-aligned.
    ssize_t buffers_flushed;
    do {
        bytes_accepted += AcceptIntoBuffers(offset + bytes_accepted,
                                            buf + bytes_accepted,
                                            size - bytes_accepted);
        buffers_flushed = FlushBuffers(size == 0);
        if (buffers_flushed == SFS_ERROR) {return SFS_ERROR;}
    } while ((buffers_flushed > 0) && (bytes_accepted != size));

    if (bytes_accepted != size && size) {  // No place for this data in the buffers currently in use
        Entry *avail_entry = FirstAvailableBuffer();
        if (!avail_entry) {  // No available buffers to allocate; logic error, should not happen.
            DumpBuffers();
            m_error_buf = "No empty buffers available to place unordered data.";
            return SFS_ERROR;
        }
        if (avail_entry->Accept(offset + bytes_accepted, buf + bytes_accepted, size - bytes_accepted) != size - bytes_accepted) {  // Empty buffer cannot accept?!?
            m_error_buf = "Empty re-ordering buffer was unable to to accept data; internal logic error.";
            return SFS_ERROR;
        }
        // The buffer we just filled may already be complete and contiguous with
        // m_offset; flush it now instead of waiting for a later callback to
        // notice, as every curl handle may be idle by then.
        if (FlushBuffers(false) == SFS_ERROR) {return SFS_ERROR;}
    }

    // If we have low buffer occupancy, then release memory.
    if ((m_buffers.size() > 2) && (m_avail_count * 2 > m_buffers.size())) {
        for (auto &entry : m_buffers) {
            entry->ShrinkIfUnused();
        }
    }

    return retval;
}


size_t
Stream::AcceptIntoBuffers(off_t offset, const char *buf, size_t size)
{
    size_t bytes_accepted = 0;
    if (!size) {return 0;}
    for (auto &entry : m_buffers) {
        // Empty buffers are deliberately skipped here: they are only handed out
        // as a last resort by Write() so that buffer occupancy keeps tracking
        // the number of transfers in flight.
        if (entry->Available()) {continue;}
        bytes_accepted += entry->Accept(offset + bytes_accepted,
                                        buf + bytes_accepted,
                                        size - bytes_accepted);
        if (bytes_accepted == size) {break;}
    }
    return bytes_accepted;
}


ssize_t
Stream::FlushBuffers(bool force)
{
    ssize_t buffers_flushed = 0;
    bool buffer_was_written;
    do {
        size_t avail_count = 0;
        buffer_was_written = false;
        for (auto &entry : m_buffers) {
            ssize_t retval = entry->Write(*this, force);
            if (retval == SFS_ERROR) {
                if (!m_error_buf.size()) {m_error_buf = "Unknown filesystem write failure.";}
                return SFS_ERROR;
            }
            if (retval > 0) {
                buffer_was_written = true;
                buffers_flushed ++;
            }
            if (entry->Available()) {avail_count ++;}
        }
        m_avail_count = avail_count;
        // Writing a buffer advances m_offset, which may have made a buffer we
        // already walked past contiguous with the stream; go around again.
    } while (buffer_was_written && (m_avail_count != m_buffers.size()));
    return buffers_flushed;
}


Stream::Entry *
Stream::FirstAvailableBuffer()
{
    for (auto &entry : m_buffers) {
        if (entry->Available()) {return entry.get();}
    }
    return nullptr;
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
    for (const auto &entry : m_buffers) {
        std::stringstream ss;
        ss << "Buffer " << idx << ": Offset=" << entry->GetOffset() << ", Size="
           << entry->GetSize() << ", Capacity=" << entry->GetCapacity();
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
