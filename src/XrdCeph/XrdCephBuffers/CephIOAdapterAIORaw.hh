#ifndef __CEPH_IO_ADAPTER_AIORAW_HH__
#define __CEPH_IO_ADAPTER_AIORAW_HH__
//------------------------------------------------------------------------------
// Interface of the logic part of the buffering
// Intention to be able to abstract the underlying implementation and code against the inteface
// e.g. for different complexities of control.
// Couples loosely to IXrdCepgBufferData and anticipated to be called by XrdCephOssBufferedFile. 
// Should managage all of the IO and logic to give XrdCephOssBufferedFile only simple commands to call.
// implementations are likely to use (via callbacks?) CephPosix library code for actual reads and writes. 
//------------------------------------------------------------------------------

#include <sys/types.h> 
#include "IXrdCephBufferData.hh"
#include "ICephIOAdapter.hh"
#include "BufferUtils.hh"

#include <chrono>
#include <memory>
#include <atomic>
#include <condition_variable>
#include <mutex>

#include "XrdSfs/XrdSfsAio.hh"


namespace XrdCephBuffer {

    class CephBufSfsAio : virtual public XrdSfsAio
    {
    public:
    CephBufSfsAio();
        // Method to handle completed reads
        //
        virtual void doneRead() override;

        // Method to hand completed writes
        //
        virtual void doneWrite() override;

        // Method to recycle free object
        //
        virtual void Recycle() override{};
        std::mutex m_mutex;
        std::unique_lock<std::mutex> m_lock;
        std::condition_variable m_condVar;
        bool isDone() {return m_dataOpDone;}

    protected:
        bool m_dataOpDone {false};

    };

/**
 * @brief Implements a non-async read and write to ceph via aio ceph_posix calls
 * Using the standard ceph_posix_aio calls do the actual read and write operations.
 * No ownership is taken on the buffer that's passed via the constructor
 * Although using aio calls, we block here until the data has been read/written
 */
class CephIOAdapterAIORaw: public  virtual ICephIOAdapter {
    public:
        CephIOAdapterAIORaw(IXrdCephBufferData * bufferdata, int fd);
        virtual ~CephIOAdapterAIORaw();

        /**
         * @brief Take the data in the buffer and write to ceph at given offset
         * Issues a ceph_posix_pwrite for data in the buffer (from pos 0) into 
         * ceph at position offset with len count.
         * Returns -ve on error, else the number of bytes writen.
         * 
         * @param offset 
         * @param count 
         * @return ssize_t 
         */
        virtual ssize_t write(off64_t offset,size_t count) override;

        /**
         * @brief Issue a ceph_posix_pread to read to the buffer data from file offset and len count.
         * No range checking is currently provided here. The caller must provide sufficient space for the 
         * max len read.
         * Returns -ve errorcode on failure, else the number of bytes returned. 
         * 
         * @param offset 
         * @param count 
         * @return ssize_t 
         */
        virtual ssize_t read(off64_t offset,size_t count) override;

    private:
        IXrdCephBufferData * m_bufferdata; //!< no ownership of pointer (consider shared ptrs, etc)
        int m_fd;

        // timer and counter info
        std::atomic< long> m_stats_read_timer{0}, m_stats_write_timer{0};
        std::atomic< long> m_stats_read_bytes{0}, m_stats_write_bytes{0};
        std::atomic< long> m_stats_read_req{0},   m_stats_write_req{0};
        long m_stats_read_longest{0}, m_stats_write_longest{0};

};

}

#endif 
