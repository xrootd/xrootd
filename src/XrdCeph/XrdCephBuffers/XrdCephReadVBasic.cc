
#include "XrdCephReadVBasic.hh"
#include "BufferUtils.hh"

using namespace XrdCephBuffer;


XrdCephReadVBasic::~XrdCephReadVBasic() {

    size_t totalBytes = m_usedBytes + m_wastedBytes;
    float goodFrac_pct = totalBytes > 0 ? m_usedBytes/(totalBytes*100.) : 0;
    BUFLOG("XrdCephReadVBasic: Summary: "
            << " Used: " <<  m_usedBytes << " Wasted: " << m_wastedBytes << " goodFrac: "
            << goodFrac_pct
            );
}

std::vector<ExtentHolder> XrdCephReadVBasic::convert(const ExtentHolder &extentsHolderInput)
{
    std::vector<ExtentHolder> outputs;

    const ExtentContainer &extentsIn = extentsHolderInput.extents();

    ExtentContainer::const_iterator it_l   = extentsIn.begin();
    ExtentContainer::const_iterator it_r   = extentsIn.begin();
    ExtentContainer::const_iterator it_end = extentsIn.end();

    // Shortcut the process if range is small
    if ((it_end->end() - it_l->begin()) <= m_minSize) {
        ExtentHolder tmp(extentsIn);
        outputs.push_back(tmp);
        BUFLOG("XrdCephReadVBasic: Combine all extents: "
                << tmp.size() << " "  
                << it_l->begin() << " " << it_end->end() );
        return outputs;
    }
    size_t usedBytes(0);
    size_t wastedBytes(0);

    // outer loop over extents 
    while (it_r != it_end)
    {
        ExtentHolder tmp;
        int counter(0);
        it_l = it_r;
        // inner loop over each internal extent range
        while (it_r != it_end) {
            if ((it_r->end() - it_l->begin()) > m_maxSize) break; // start a new holder
            tmp.push_back(*it_r); // just put it into an extent
            ++it_r;
            ++counter;
        }
        outputs.push_back(tmp);
        usedBytes += tmp.bytesContained();
        wastedBytes += tmp.bytesMissing();
    }
    m_usedBytes += usedBytes;
    m_wastedBytes += wastedBytes;
    BUFLOG("XrdCephReadVBasic: In size: " << extentsHolderInput.size() << " " 
            << extentsHolderInput.extents().size() << " " << outputs.size() << " " 
            << " useful bytes: " << usedBytes << " wasted bytes:" << wastedBytes);
            

    return outputs;
} // convert
