#ifndef __ICEPH_IO_ADAPTER_HH__
#define __ICEPH_IO_ADAPTER_HH__
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

namespace XrdCephBuffer {

/**
 * @brief Manage the actual IO operations that read and write the data into Ceph via librados striper.
 * Likely to be provided with a buffer in the concreate implementation's constructor.
 * Attempt to decouple the low level IO operations from the buffer implementation.
 * However, ight coupling might be strictly necessary, making this class a bit redundant.
 * Consider to refactor if this proves to be the case ... 
 * 
 */
class ICephIOAdapter {
    public: 
        virtual ~ICephIOAdapter() {}
        virtual ssize_t write(off64_t offset,size_t count) = 0; //!< write from buffer into ceph
        virtual ssize_t read(off64_t offset,size_t count)  = 0; //!< read from ceph into the buffer

};

}

#endif 
