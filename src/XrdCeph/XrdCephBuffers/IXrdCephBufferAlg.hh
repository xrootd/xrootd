#ifndef __IXRD_CEPH_BUFFER_ALG_HH__
#define __IXRD_CEPH_BUFFER_ALG_HH__
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

class XrdSfsAio;

namespace XrdCephBuffer {

/**
 * @brief Interface to a holder of the main logic decisions of the buffering algortithm, decoupled from the buffer resource itself.
 * Main work of the buffering is done in the classes that inherit from the interace, of how and when and why to buffer and flush the data
 * The physical representation of the buffer is not written here to allow for some flexibility of changing the internals of the buffer if needed. 
 * Anticipate that a non-async and async will be the main distinct use cases.
 */
class IXrdCephBufferAlg {
    public:
        virtual ~IXrdCephBufferAlg() {}

        virtual ssize_t read_aio (XrdSfsAio *aoip)  = 0; //!< possible aio based code 
        virtual ssize_t write_aio(XrdSfsAio *aoip) = 0; //!< possible aio based code

        virtual ssize_t read (volatile void *buff, off_t offset, size_t blen)  = 0; //!< read data through the buffer
        virtual ssize_t write(const void *buff, off_t offset, size_t blen) = 0; //!< write data through the buffer
        virtual ssize_t flushWriteCache() = 0;  //!< remember to flush the cache on final writes


    protected:
        

    private:

};

}

#endif 
