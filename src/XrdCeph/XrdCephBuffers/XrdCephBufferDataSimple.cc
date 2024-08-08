//------------------------------------------------------------------------------
//! is a simple implementation of IXrdCephBufferData using std::vector<char> representation for the buffer
//------------------------------------------------------------------------------

#include "XrdCephBufferDataSimple.hh"
#include "BufferUtils.hh"
//#include "XrdCeph/XrdCephBuffers/IXrdCephBufferData.hh"
#include <errno.h>
#include <memory.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <iostream>


using  namespace XrdCephBuffer;

std::atomic<long> XrdCephBufferDataSimple::m_total_memory_used {0}; //!< total memory of all these buffers
std::atomic<long> XrdCephBufferDataSimple::m_total_memory_nbuffers {0}; //!< total number of buffers actively open



XrdCephBufferDataSimple::XrdCephBufferDataSimple(size_t bufCapacity):
 m_bufferSize(bufCapacity), m_buffer(bufCapacity,0), m_externalOffset(0),m_bufLength(0) {
    m_valid = true;

    // update global statistics
    m_total_memory_used.fetch_add(bufCapacity);
    ++m_total_memory_nbuffers;
    BUFLOG("XrdCephBufferDataSimple:  Global: " << m_total_memory_nbuffers.load() << " " << m_total_memory_used.load());
}

XrdCephBufferDataSimple::~XrdCephBufferDataSimple() {
    m_valid = false;
    // obtain the actual capacity here, as this is the real number of bytes to be released
    auto cap = m_buffer.capacity();
    m_buffer.clear();
    m_buffer.reserve(0); // just to be paranoid and realse memory immediately

    // update global statistics
    m_total_memory_used.fetch_add(-cap);
    --m_total_memory_nbuffers;
    BUFLOG("XrdCephBufferDataSimple~:  Global: " << m_total_memory_nbuffers.load() << " " << m_total_memory_used.load());

}


size_t XrdCephBufferDataSimple::capacity() const {
    // return defined buffered size, which might in principle be different
    // to the actual size of the buffer allocated in memory
    return m_bufferSize;
}

size_t XrdCephBufferDataSimple::length() const   {
    return m_bufLength;
}
void   XrdCephBufferDataSimple::setLength(size_t len) {
    m_bufLength = len;
}
bool XrdCephBufferDataSimple::isValid() const {
    return m_valid;
}
void XrdCephBufferDataSimple::setValid(bool isValid) {
    m_valid = isValid;
}


off_t XrdCephBufferDataSimple::startingOffset() const  {
    return m_externalOffset;
}
off_t XrdCephBufferDataSimple::setStartingOffset(off_t offset) {
    m_externalOffset = offset;
    return m_externalOffset;
}

ssize_t XrdCephBufferDataSimple::invalidate() {
    m_externalOffset = 0;
    m_bufLength      = 0;
    m_valid = false;
    //m_buffer.clear();  // do we really need to clear the elements ?
    return 0;
}



ssize_t XrdCephBufferDataSimple::readBuffer(void* buf, off_t offset, size_t blen) const {
    // read from the internal buffer to buf (at pos 0), from offset for blen, or max length possible
    // returns -ve value on error, else the actual number of bytes read

    if (!m_valid) {
        return -EINVAL;
    }
    if (offset < 0) {
        return -EINVAL;
    }
    if (offset > (ssize_t) m_bufLength) {
        return 0;
    }
    ssize_t readlength = blen;
    if (offset + blen > m_bufLength) {
        readlength = m_bufLength - offset;
    }
    //std::cout << readlength << " " << blen << " " << m_bufLength << " " << offset << std::endl;
    if (readlength <0) {
        return -EINVAL;
    }

    if (readlength == 0) {
        return 0;
    }
    
    const char* rawbufstart = m_buffer.data();

    long int_ns{0};
    {auto t = Timer_ns(int_ns);
        // std::copy(rawbufstart + offset, rawbufstart+offset+readlength, reinterpret_cast<char*>(buf) );
        memcpy(reinterpret_cast<char*>(buf), rawbufstart + offset, readlength);
    } // end Timer
    // BUFLOG("XrdCephBufferDataSimple::readBuffer: " << offset << " " << readlength << " " << int_ns );

    return readlength;
}


ssize_t XrdCephBufferDataSimple::writeBuffer(const void* buf, off_t offset, size_t blen, off_t externalOffset) {
    // write data from buf (from pos 0), with length blen, into the buffer at position offset (local to the internal buffer)
    
    // #TODO Add test to see if it's in use
    //invalidate();

    if (offset < 0) {
        BUFLOG("XrdCephBufferDataSimple::writeBuffer: offset <0");
        return -EINVAL;
    }

    ssize_t cap = capacity();
    if ((ssize_t)blen > cap) {
        BUFLOG("XrdCephBufferDataSimple::writeBuffer: blen > cap:" << blen << " > " << cap);
        return -EINVAL;
    }
    if ((ssize_t)offset > cap) {
        BUFLOG("XrdCephBufferDataSimple::writeBuffer: offset > cap:" << offset << " > " << cap);
        return -EINVAL;
    }
    if (ssize_t(offset + blen) > cap) {
        BUFLOG("XrdCephBufferDataSimple::writeBuffer: (offset + blen) > cap: (" << offset << " + " << blen << ") >" << cap);
        return -EINVAL;
    }

    // std::vector<char>::iterator itstart = m_buffer.begin();
    size_t readBytes = blen;
    char* rawbufstart = m_buffer.data();


    long int_ns{0};
    {auto t = Timer_ns(int_ns); // brace for timer start/stop scoping
      //std::copy((char*)buf, (char*)buf +readBytes ,itstart + offset );
      memcpy(rawbufstart + offset, buf, readBytes);

    } // end Timer

    // BUFLOG("XrdCephBufferDataSimple::writeBuffer: " << offset << " " << readBytes << " " << int_ns);



    m_externalOffset = externalOffset;
    // Decide to set the length of the maximum value that has be written
    // note; unless invalidate is called, then this value may not be correctly set ... 
    m_bufLength      = std::max(offset+blen, m_bufLength);
    m_valid          = true;
 

    return readBytes;
} 
