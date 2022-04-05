#ifndef __XRD_CEPH_BUFFER_ALG_SIMPLE_HH__
#define __XRD_CEPH_BUFFER_ALG_SIMPLE_HH__
//------------------------------------------------------------------------------
// Implementation of the logic section of buffer code
//------------------------------------------------------------------------------

#include <sys/types.h> 
#include <mutex>
#include <memory>

#include "IXrdCephBufferAlg.hh"
#include "ICephIOAdapter.hh"
#include "BufferUtils.hh"


namespace XrdCephBuffer {

/** Non-async buffering code for non-aio read operations.
 * Create a single buffer of a given size.
 * For reads, if data in the buffer read and return the available bytes;
 *   if no useful data in the buffer fill the full buffer and return the requested read.
 * If the data is partially in the buffer for the range requested, return only that subset; 
 * client should check and make an additional call for the data not returned.
 * if 0 bytes are returned, it should be assumed it is at the end of the file.
 */

class XrdCephBufferAlgSimple : public virtual  IXrdCephBufferAlg {
    public:
        XrdCephBufferAlgSimple(std::unique_ptr<IXrdCephBufferData> buffer, std::unique_ptr<ICephIOAdapter> cephio, int fd ); 
        virtual ~XrdCephBufferAlgSimple();

        virtual ssize_t read_aio (XrdSfsAio *aoip) override;
        virtual ssize_t write_aio(XrdSfsAio *aoip) override;


        virtual ssize_t read (volatile void *buff, off_t offset, size_t blen) override;
        virtual ssize_t write(const void *buff, off_t offset, size_t blen) override;
        virtual ssize_t flushWriteCache() override; 

        // #REVIEW
        virtual const IXrdCephBufferData *buffer() const {return m_bufferdata.get();}
        virtual IXrdCephBufferData *buffer() {return m_bufferdata.get();}

    protected:
        virtual ssize_t rawRead (void *buff,       off_t offset, size_t blen) ; // read from the storage, at its offset
        virtual ssize_t rawWrite(void *buff,       off_t offset, size_t blen) ; // write to the storage, to its offset posiiton

    private:
        std::unique_ptr<IXrdCephBufferData> m_bufferdata; //! this algorithm takes ownership of the buffer, and will delete it on destruction
        std::unique_ptr<ICephIOAdapter>     m_cephio ; // no ownership is taken here
        int m_fd = -1;

        off_t m_bufferStartingOffset = 0;
        size_t m_bufferLength = 0;

        std::recursive_mutex m_data_mutex; // any data access method on the buffer will use this

        long m_stats_bytes_fromceph{0}; //! number of bytes requested from ceph, to fill the buffers, etc.
        long m_stats_bytes_bypassed{0}; //! number of bytes specifically bypassed
        long m_stats_bytes_toclient{0}; //! number of bytes requested by the client
};  

}

#endif 
