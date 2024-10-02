
#include "XrdCephReadVNoOp.hh"
#include "BufferUtils.hh"

using namespace XrdCephBuffer;

std::vector<ExtentHolder> XrdCephReadVNoOp::convert(const ExtentHolder &extentsHolderInput)
{
    std::vector<ExtentHolder> outputs;

    const ExtentContainer &extentsIn = extentsHolderInput.extents();

    for (ExtentContainer::const_iterator it = extentsIn.begin(); it != extentsIn.end(); ++it)
    {
        ExtentHolder tmp;
        tmp.push_back(*it);
        outputs.push_back(tmp);
    } // for
    // each element in the output contains one element, the

    return outputs;
} // convert
