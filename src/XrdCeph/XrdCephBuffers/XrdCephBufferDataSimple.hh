#ifndef __XRD_CEPH_BUFFER_DATA_SIMPLE_HH__
#define __XRD_CEPH_BUFFER_DATA_SIMPLE_HH__
//------------------------------------------------------------------------------
//! is a simple implementation of IXrdCephBufferData using std::vector<char> representation for the buffer
//------------------------------------------------------------------------------

#include <sys/types.h> 
#include "IXrdCephBufferData.hh"
#include "BufferUtils.hh"
#include <vector>
#include <atomic>
#include <chrono>

namespace XrdCephBuffer {

/**
 * @brief Implementation of a buffer using a simple vector<char>
 * Simplest implementation of a buffer using vector<char> for underlying memory.
 * Capacity is reserved on construction and released back at destruction.
 * Does very little itself, except to provide access methods
 * 
 */
class XrdCephBufferDataSimple :  public virtual IXrdCephBufferData
 {
    public:
        XrdCephBufferDataSimple(size_t bufCapacity);
        virtual ~XrdCephBufferDataSimple();
        virtual size_t capacity() const override;//! total available space
        virtual size_t length() const  override;//! Currently occupied and valid space, which may be less than capacity
        virtual void   setLength(size_t len) override;//! Currently occupied and valid space, which may be less than capacity
        virtual bool isValid() const override;
        virtual void setValid(bool isValid) override;

        virtual  off_t startingOffset() const override;
        virtual  off_t setStartingOffset(off_t offset) override;


        virtual ssize_t readBuffer(void* buf, off_t offset, size_t blen) const override; //! copy data from the internal buffer to buf

        virtual ssize_t invalidate() override; //! set cache into an invalid state; do this before writes to be consistent
        virtual ssize_t writeBuffer(const void* buf, off_t offset, size_t blen, off_t externalOffset=0) override; //! write data into the buffer, store the external offset if provided

        virtual const void* raw() const override {return capacity() > 0 ? &(m_buffer[0]) : nullptr;}
        virtual void* raw() override {return capacity() > 0 ? &(m_buffer[0]) : nullptr;}


    protected:
        size_t m_bufferSize; //! the buffer size
        bool m_valid = false;
        std::vector<char> m_buffer; // actual physical buffer
        off_t m_externalOffset = 0; //! what does the first byte of the buffer map to for external offsets
        size_t m_bufLength = 0;  //! length of valid stored data; might be less than the capacity

        // timer and counter info
        std::atomic< long> m_stats_read_timer{0}, m_stats_write_timer{0};
        std::atomic< long> m_stats_read_bytes{0}, m_stats_write_bytes{0};
        std::atomic< long> m_stats_read_req{0},   m_stats_write_req{0};
        long m_stats_read_longest{0}, m_stats_write_longest{0};

        // staric vars to store the total useage of memory across this class
        static std::atomic<long> m_total_memory_used;
        static std::atomic<long> m_total_memory_nbuffers;

}; // XrdCephBufferDataSimple

} // namespace 
#endif 
