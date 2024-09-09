#ifndef __IXRD_CEPH_READV_BASIC_HH__
#define __IXRD_CEPH_READV_BASIC_HH__
//------------------------------------------------------------------------------
// Interface to the actual buffer data object used to store the data
// Intention to be able to abstract the underlying implementation and code against the inteface
// e.g. if choice of buffer data object
//------------------------------------------------------------------------------

#include <sys/types.h>
#include <vector>

#include "BufferUtils.hh"
#include "IXrdCephReadVAdapter.hh"

namespace XrdCephBuffer
{

    /**
     * @brief Combine requests into single reads accoriding to some basic rules. 
     * Read a minimum amount of data (2MiB default), keep adding chunks until the used fraction is lower than some threshold, or 64MiB is reached.
     * Calling code unraveles the correct ranges for each
     */


    class XrdCephReadVBasic : virtual public IXrdCephReadVAdapter {
    // nothing more than readV in, and readV out
    public:
        XrdCephReadVBasic() {}
        virtual ~XrdCephReadVBasic();

    virtual std::vector<ExtentHolder> convert(const ExtentHolder &extentsHolderInput) override;

    protected:
        ssize_t m_minSize = 2*1024*1024;
        ssize_t m_maxSize = 16*1024*1024;

    private:
        size_t m_usedBytes = 0;
        size_t m_wastedBytes = 0;
        

    };



}

#endif
