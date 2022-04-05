#ifndef __IXRD_CEPH_READV_ADAPTER_HH__
#define __IXRD_CEPH_READV_ADAPTER_HH__
//------------------------------------------------------------------------------
// Interface to the actual buffer data object used to store the data
// Intention to be able to abstract the underlying implementation and code against the inteface
// e.g. if choice of buffer data object
//------------------------------------------------------------------------------

#include <sys/types.h>
#include <vector>

#include "BufferUtils.hh"

#include <iostream> // #FIXME remove

namespace XrdCephBuffer
{

    /**
     * @brief Interface to the logic of dealing with readV requests 
     */
    class IXrdCephReadVAdapter
    {
    public:
        virtual ~IXrdCephReadVAdapter() {}

        /**
         * @brief Take in a set of extents representing the readV requests. return a vector of each combined read request.
         * Caller translates the readV request into a set of Extents (passed to an ExtentHolder).
         * The logic of the specific concrete implementation combines the set of readV requests into merged requests.
         * Output is a vector of those requests. Each ExtentHolder element holds the offset and len to be read, and also
         * the contained extents of the readVs.
         * The index of the readV element is not held, so the caller must ensure to match up appropriately.
         * 
         * @param extentsIn 
         * @return std::vector<ExtentHolder> 
         */
        virtual std::vector<ExtentHolder> convert(const ExtentHolder &extentsIn)  =0;

    protected:
    };

}

#endif
