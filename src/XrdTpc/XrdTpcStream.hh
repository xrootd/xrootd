
/**
 * The "stream" interface is a simple abstraction of a file handle.
 *
 * The abstraction layer is necessary to do the necessary buffering
 * of multi-stream writes where the underlying filesystem only
 * supports single-stream writes.
 */

#include <memory>
#include <vector>
#include <string>

#include <cstring>

struct stat;

class XrdSfsFile;
class XrdSysError;

namespace TPC {
class Stream {
public:
    Stream(std::unique_ptr<XrdSfsFile> fh, size_t max_blocks, size_t buffer_size, XrdSysError &log)
        : m_open_for_write(false),
          m_avail_count(max_blocks),
          m_fh(std::move(fh)),
          m_offset(0),
          m_log(log)
    {
        m_buffers.reserve(max_blocks);
        for (size_t idx=0; idx < max_blocks; idx++) {
            m_buffers.push_back(new Entry(buffer_size));
        }
        m_open_for_write = true;
    }

    ~Stream();

    int Stat(struct stat *);

    int Read(off_t offset, char *buffer, size_t size);

    // Writes a buffer of a given size to an offset.
    // This will often keep the buffer in memory in to present the underlying
    // filesystem with a single stream of data (required for HDFS); further,
    // it will also buffer to align the writes on a 1MB boundary (required
    // for some RADOS configurations).  When force is set to true, it will
    // skip the buffering and always write (this should only be done at the
    // end of a stream!).
    //
    // Returns the number of bytes written; on error, returns -1 and sets
    // the error code and error message for the stream
    ssize_t Write(off_t offset, const char *buffer, size_t size, bool force);

    size_t AvailableBuffers() const {return m_avail_count;}

    void DumpBuffers() const;

    // Flush and finalize the stream.  If all data has been sent to the underlying
    // file handle, close() will be invoked on the file handle.
    //
    // Further write operations on this stream will result in an error.
    // If any memory buffers remain, an error occurs.
    //
    // Returns true on success; false otherwise.
    bool Finalize();

    std::string GetErrorMessage() const {return m_error_buf;}

private:

    class Entry {
    public:
        Entry(size_t capacity) :
            m_offset(-1),
            m_capacity(capacity),
            m_size(0)
        {}

        bool Available() const {return m_offset == -1;}

        int Write(Stream &stream, bool force) {
            if (Available() || !CanWrite(stream)) {return 0;}
            // Only full buffer writes are accepted unless the stream forces a flush
            // (i.e., we are at EOF) because the multistream code uses buffer occupancy
            // to determine how many streams are currently in-flight.  If we do an early
            // write, then the buffer will be empty and the multistream code may decide
            // to start another request (which we don't have the capacity to serve!).
            if (!force && (m_size != m_capacity)) {
                return 0;
            }
            ssize_t retval = stream.WriteImpl(m_offset, &m_buffer[0], m_size);
            // Currently the only valid negative value is SFS_ERROR (-1); checking for
            // all negative values to future-proof the code.
            if ((retval < 0) || (static_cast<size_t>(retval) != m_size)) {
                return -1;
            }
            m_offset = -1;
            m_size = 0;
            m_buffer.clear();
            return retval;
        }

        size_t Accept(off_t offset, const char *buf, size_t size) {
            // Validate acceptance criteria.
            if ((m_offset != -1) && (offset != m_offset + static_cast<ssize_t>(m_size))) {
                return 0;
            }
            size_t to_accept = m_capacity - m_size;
            if (to_accept == 0) {return 0;}
            if (size > to_accept) {
                size = to_accept;
            }

            // Inflate the underlying buffer if needed.
            ssize_t new_bytes_needed = (m_size + size) - m_buffer.size();
            if (new_bytes_needed > 0) {
                m_buffer.resize(m_capacity);
            }

            // Finally, do the copy.
            memcpy(&m_buffer[0] + m_size, buf, size);
            m_size += size;
            if (m_offset == -1) {
                m_offset = offset;
            }
            return size;
        }

        void ShrinkIfUnused() {
           if (!Available()) {return;}
#if __cplusplus > 199711L
           m_buffer.shrink_to_fit();
#endif
        }

        void Move(Entry &other) {
            m_buffer.swap(other.m_buffer);
            m_offset = other.m_offset;
            m_size = other.m_size;
        }

        off_t GetOffset() const {return m_offset;}
        size_t GetCapacity() const {return m_capacity;}
        size_t GetSize() const {return m_size;}

    private:

        Entry(const Entry&) = delete;

        bool CanWrite(Stream &stream) const {
            return (m_size > 0) && (m_offset == stream.m_offset);
        }

        off_t m_offset;  // Offset within file that m_buffer[0] represents.
        size_t m_capacity;
        size_t m_size;  // Number of bytes held in buffer.
        std::vector<char> m_buffer;
    };

    ssize_t WriteImpl(off_t offset, const char *buffer, size_t size);

    bool m_open_for_write;
    size_t m_avail_count;
    std::unique_ptr<XrdSfsFile> m_fh;
    off_t m_offset;
    std::vector<Entry*> m_buffers;
    XrdSysError &m_log;
    std::string m_error_buf;
};
}
