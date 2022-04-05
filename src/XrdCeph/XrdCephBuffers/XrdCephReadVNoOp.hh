#ifndef __IXRD_CEPH_READV_NOOP_HH__
#define __IXRD_CEPH_READV_NOOP_HH__
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
     * @brief Passthrough implementation. Convertes the ReadV requests to extents and makes the request.
     * Does not change how the readV implementation is done, just implements a version with Extents
     * More for functionality testing, or to allow easier access to readV statistics. 
     */
    class XrdCephReadVNoOp : virtual public IXrdCephReadVAdapter {
    // nothing more than readV in, and readV out
    public:
        XrdCephReadVNoOp() {}
        virtual ~XrdCephReadVNoOp() {}

    virtual std::vector<ExtentHolder> convert(const ExtentHolder &extentsHolderInput) override;

    protected:
    };



}

#endif
