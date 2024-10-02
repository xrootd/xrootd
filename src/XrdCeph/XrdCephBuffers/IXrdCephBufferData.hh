#ifndef __IXRD_CEPH_BUFFER_DATA_HH__
#define __IXRD_CEPH_BUFFER_DATA_HH__
//------------------------------------------------------------------------------
// Interface to the actual buffer data object used to store the data
// Intention to be able to abstract the underlying implementation and code against the inteface
// e.g. if choice of buffer data object 
//------------------------------------------------------------------------------

#include <sys/types.h> 

namespace XrdCephBuffer {

/**
 * @brief Interface to the Buffer's  physical representation.
 * Allow an interface to encapsulate the requirements of a buffer's memory, without worrying about the details.
 * Various options exist for the specific buffer implemented, and are left to the sub-classes.
 */
class IXrdCephBufferData {
    public:
        virtual ~IXrdCephBufferData(){}
        virtual size_t capacity() const = 0;//! total available space
        virtual size_t length() const  = 0;//! Currently occupied and valid space, which may be less than capacity
        virtual void   setLength(size_t len) =0 ;//! Currently occupied and valid space, which may be less than capacity
        virtual bool isValid() const =0;
        virtual void setValid(bool isValid) =0;

        virtual  off_t startingOffset() const = 0;
        virtual  off_t setStartingOffset(off_t offset) = 0;

        virtual ssize_t invalidate() = 0; //! set cache into an invalid state

       virtual ssize_t readBuffer(void* buf, off_t offset, size_t blen) const = 0; //! copy data from the internal buffer to buf

       virtual ssize_t writeBuffer(const void* buf, off_t offset, size_t blen,off_t externalOffset)  = 0; //! write data into the buffer, store the external offset

        virtual const void* raw() const = 0;  // const accessor to the 'raw' or underlying object
        virtual void* raw() = 0; // accessor to the 'raw' or underlying object


    protected:
    
};

}

#endif 
