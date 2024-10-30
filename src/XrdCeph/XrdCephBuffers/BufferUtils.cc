
#include "BufferUtils.hh"
#include <algorithm> // std::max

using namespace XrdCephBuffer;

#ifdef CEPHBUFDEBUG
// to synchronise logging statements 
  std::mutex cephbuf_iolock;
#endif 

// ------------------------------------------------------ //
//         Extent       //

bool Extent::in_extent(off_t pos) const
{
    return ((pos > begin()) && (pos < end()));
}

bool Extent::isContiguous(const Extent &rhs) const
{
    // does the rhs connect directly to the end of the first
    if (end() != rhs.begin())
        return false;
    return true;
}

bool Extent::allInExtent(off_t pos, size_t len) const
{
    // is all the range in this extent
    if ((pos < begin()) || (pos >= end()))
        return false;

    if (off_t(pos + len) > end())
        return false;
    return true;
}
bool Extent::someInExtent(off_t pos, size_t len) const
{ // is some of the range in this extent
    if ((off_t(pos + len) < begin()) || (pos >= end()))
        return false;
    return true;
}

Extent Extent::containedExtent(off_t pos, size_t len) const
{
    // return the subset of input range that is in this extent
    off_t subbeg = std::max(begin(), pos);
    off_t subend = std::min(end(), off_t(pos + len));

    return Extent(subbeg, subend - subbeg);
}
Extent Extent::containedExtent(const Extent &rhs) const
{
    return containedExtent(rhs.begin(), rhs.len());
}

bool Extent::operator<(const Extent &rhs) const
{
    // comparison primarily on begin values
    // use end values if begin values are equal.

    if (begin() > rhs.begin()) return false;
    if (begin() < rhs.begin()) return true;
    if (end()   < rhs.end() )  return true;
    return false; 
}
bool Extent::operator==(const Extent &rhs) const
{
    // equivalence based only on start and end
    if (begin() != rhs.begin())
        return false;
    if (end() != rhs.end())
        return false;
    return true;
}

// ------------------------------------------------------ //
//         ExtentHolder       //

ExtentHolder::ExtentHolder() {}

ExtentHolder::ExtentHolder(size_t elements)
{
    m_extents.reserve(elements);
}

ExtentHolder::ExtentHolder(const ExtentContainer &extents)
{
    m_extents.reserve(extents.size());
    for (ExtentContainer::const_iterator vit = m_extents.cbegin(); vit != m_extents.cend(); ++vit) {
        push_back(*vit);
    }

}
ExtentHolder::~ExtentHolder()
{
    m_extents.clear();
}

void ExtentHolder::push_back(const Extent & in) {
    if (size()) {
        m_begin = std::min(m_begin, in.begin());
        m_end   = std::max(m_end, in.end());
    } else {
        m_begin = in.begin();
        m_end = in.end();
    }
    return m_extents.push_back(in);
}



Extent ExtentHolder::asExtent() const {
    // if (!size()) return Extent(0,0);
    // ExtentContainer se = getSortedExtents();
    // off_t b = se.front().begin();
    // off_t e = se.back().end();

    return Extent(m_begin, m_end-m_begin);

}

size_t ExtentHolder::bytesContained() const {
    size_t nbytes{0};
    for (ExtentContainer::const_iterator vit = m_extents.cbegin(); vit != m_extents.cend(); ++vit) {
        nbytes += vit->len();
    }
    return nbytes;
}

size_t ExtentHolder::bytesMissing() const {
    size_t bytesUsed = bytesContained();
    size_t totalRange = asExtent().len(); //might be expensive to call
    return totalRange - bytesUsed;
}   


void ExtentHolder::sort() {
        std::sort(m_extents.begin(), m_extents.end());
}


ExtentContainer ExtentHolder::getSortedExtents() const {
    ExtentContainer v;
    v.assign(m_extents.begin(), m_extents.end() );
    std::sort(v.begin(), v.end());
    return v;
}

ExtentContainer ExtentHolder::getExtents() const {
    ExtentContainer v;
    v.assign(m_extents.begin(), m_extents.end() );
    return v;
}

// ------------------------------------------------------ //
//         Timer ns       //

Timer_ns::Timer_ns(long &output) : m_output_val(output)
{
    m_start = std::chrono::steady_clock::now();
}

Timer_ns::~Timer_ns()
{
    auto end = std::chrono::steady_clock::now();
    m_output_val = std::chrono::duration_cast<std::chrono::nanoseconds>(end - m_start).count();
}
