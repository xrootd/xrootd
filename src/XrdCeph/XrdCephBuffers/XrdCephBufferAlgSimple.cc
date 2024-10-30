//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

#include <sys/types.h> 
#include "XrdCephBufferAlgSimple.hh"

#include "../XrdCephPosix.hh"
#include <XrdOuc/XrdOucEnv.hh>
#include <fcntl.h>
#include <sys/stat.h>
#include <iostream>
#include <thread>

#include "XrdSfs/XrdSfsAio.hh"


using namespace XrdCephBuffer;


XrdCephBufferAlgSimple::XrdCephBufferAlgSimple(std::unique_ptr<IXrdCephBufferData> buffer, 
                                               std::unique_ptr<ICephIOAdapter> cephio, int fd,
                                               bool useStriperlessReads):
m_bufferdata(std::move(buffer)), m_cephio(std::move(cephio)), m_fd(fd),
m_useStriperlessReads(useStriperlessReads) {

}

XrdCephBufferAlgSimple::~XrdCephBufferAlgSimple() {
    int prec = std::cout.precision();
    float bytesBuffered = m_stats_bytes_fromceph - m_stats_bytes_bypassed;
    float cacheUseFraction = bytesBuffered > 0 ? (1.*(m_stats_bytes_toclient-m_stats_bytes_bypassed)/bytesBuffered) : 1. ;

    BUFLOG("XrdCephBufferAlgSimple::Destructor, fd=" << m_fd 
            << ", retrieved_bytes="  << m_stats_bytes_fromceph 
            << ", bypassed_bytes=" << m_stats_bytes_bypassed
            << ", delivered_bytes=" << m_stats_bytes_toclient 
            << std::setprecision(4)
            << ", cache_hit_frac=" << cacheUseFraction << std::setprecision(prec));
    m_fd = -1;
}


ssize_t XrdCephBufferAlgSimple::read_aio (XrdSfsAio *aoip) {
    // Currently this is not supported, and callers using this should recieve the appropriate error code
    //return -ENOSYS;
    
    ssize_t rc(-ENOSYS);
    if (!aoip) {
        return -EINVAL; 
    }

    volatile void  * buf  = aoip->sfsAio.aio_buf;
    size_t blen  = aoip->sfsAio.aio_nbytes;
    off_t offset = aoip->sfsAio.aio_offset;

    // translate the aio read into a simple sync read.
    // hopefully don't get too many out of sequence reads to effect the caching
    rc = read(buf, offset, blen);

    aoip->Result = rc;
    aoip->doneRead();

    return rc;
    
}

ssize_t XrdCephBufferAlgSimple::write_aio(XrdSfsAio *aoip) {
    // Currently this is not supported, and callers using this should recieve the appropriate error code
   // return -ENOSYS;
    
    ssize_t rc(-ENOSYS);
        if (!aoip) {
             return -EINVAL; 
         }

        // volatile void  * buf  = aoip->sfsAio.aio_buf;
        // size_t blen  = aoip->sfsAio.aio_nbytes;
        // off_t offset = aoip->sfsAio.aio_offset;
    size_t blen  = aoip->sfsAio.aio_nbytes;
    off_t offset = aoip->sfsAio.aio_offset;

    rc = write(const_cast<const void*>(aoip->sfsAio.aio_buf), offset, blen);
    aoip->Result = rc;
    aoip->doneWrite();
    return rc;
    
}


ssize_t XrdCephBufferAlgSimple::read(volatile void *buf,   off_t offset, size_t blen)  {
    // Set a lock for any attempt at a simultaneous operation
    // Use recursive, as flushCache also calls the lock and don't want to deadlock
    // No call to flushCache should happen in a read, but be consistent
    // BUFLOG("XrdCephBufferAlgSimple::read: preLock: " << std::hash<std::thread::id>{}(std::this_thread::get_id()) << " " << offset << " " << blen);
    const std::lock_guard<std::recursive_mutex> lock(m_data_mutex); // 
    // BUFLOG("XrdCephBufferAlgSimple::read: postLock: " << std::hash<std::thread::id>{}(std::this_thread::get_id()) << " " << offset << " " << blen);

    // BUFLOG("XrdCephBufferAlgSimple::read status:" 
    //     << "\n\tRead off/len/end: " << offset << "/" << blen << "/(" << (offset+blen) <<")"
    //     << "\n\tBuffer: start/length/end/cap: " << m_bufferStartingOffset << "/" << m_bufferLength << "/" 
    //     << (m_bufferStartingOffset + m_bufferLength) << "/" << m_bufferdata->capacity()
    //     );
    if (blen == 0) return 0;

    /**
     * If the requested read is larger than the buffer size, just bypass the cache.
     * Invalidate the cache in anycase
     */
    if (blen >= m_bufferdata->capacity()) {
        //BUFLOG("XrdCephBufferAlgSimple::read: Readthrough cache: fd: " << m_fd 
        //         << " " << offset << " " << blen);
        // larger than cache, so read through, and invalidate the cache anyway
        m_bufferdata->invalidate();
        m_bufferLength =0; // ensure cached data is set to zero length
        // #FIXME JW: const_cast is probably a bit poor.
        
        ssize_t rc = ceph_posix_maybestriper_pread (m_fd, const_cast<void*>(buf), blen, offset, m_useStriperlessReads);
        if (rc > 0) {
            m_stats_bytes_fromceph += rc;
            m_stats_bytes_toclient += rc;
            m_stats_bytes_bypassed += rc;
        }
        return rc;
    }

    ssize_t rc(-1);
    size_t bytesRemaining = blen; // track how many bytes still need to be read
    off_t offsetDelta = 0;
    size_t bytesRead = 0;
    /**
     * In principle, only should ever have the first loop, however, in the case a read request
     * passes over the boundary of the buffer, two reads will be needed; the first to read 
     * out the current buffer, and a second, to read the partial data from the refilled buffer
     */
    while (bytesRemaining > 0) { 
        // BUFLOG("In loop: " << "  " << offset << " + " << offsetDelta << "; " << blen << " : " << bytesRemaining << " " << m_bufferLength);

        bool loadCache = false;
        // run some checks to see if we need to fill the cache. 
        if (m_bufferLength == 0) {
            // no data in buffer
            loadCache = true;
        } else if (offset < m_bufferStartingOffset) {
            // offset before any cache data 
            loadCache = true;
        } else if (offset >=  (off_t) (m_bufferStartingOffset + m_bufferLength) ) {
            // offset is beyond the stored data
            loadCache = true;
        } else if ((offset - m_bufferStartingOffset + offsetDelta) >= (off_t)m_bufferLength) {
            // we have now read to the end of the buffers data
            loadCache = true;
        }

        /**
         * @brief If we need to load data in the cache, do it here.
         * 
         */
        if (loadCache) {
            // BUFLOG("XrdCephBufferAlgSimple::read: preLock: " << std::hash<std::thread::id>{}(std::this_thread::get_id()) << " " << "Filling the cache");
            m_bufferdata->invalidate();
            m_bufferLength =0; // set lengh of data stored to 0
            rc = m_cephio->read(offset + offsetDelta, m_bufferdata->capacity()); // fill the cache
            // BUFLOG("LoadCache ReadToCache: " << rc << " " << offset + offsetDelta << " " << m_bufferdata->capacity() );
            if (rc < 0) {
                BUFLOG("LoadCache Error: " << rc);
                return rc;// TODO return correct errors
            }
            m_stats_bytes_fromceph += rc;
            m_bufferStartingOffset = offset + offsetDelta;
            m_bufferLength = rc;
            if (rc == 0) {
                // We should be at the end of file, with nothing more to read, and nothing that could be returned
                // break out of the loop.
                break;
            }
        }


        //now read as much data as possible
        off_t bufPosition = offset  + offsetDelta - m_bufferStartingOffset; 
        rc =  m_bufferdata->readBuffer( (void*) &(((char*)buf)[offsetDelta]) , bufPosition , bytesRemaining);
        // BUFLOG("Fill result: " << offsetDelta << " " << bufPosition << " " << bytesRemaining << " " << rc)
        if (rc < 0 ) {
            BUFLOG("Reading from Cache Failed: " << rc << "  " << offset << " " 
                    << offsetDelta << "  " << m_bufferStartingOffset << " " 
                    << bufPosition << " " 
                    << bytesRemaining );
            return rc; // TODO return correct errors
        }
        if (rc == 0) {
            // no bytes returned; much be at end of file
            //BUFLOG("No bytes returned: " << rc << "  " << offset << " + " << offsetDelta << "; " << blen << " : " << bytesRemaining);
            break; // leave the loop even though bytesremaing is probably >=0.
            //i.e. requested a full buffers worth, but only a fraction of the file is here.
        }
        m_stats_bytes_toclient += rc;
        // BUFLOG("End of loop: " << rc << "  " << offset << " + " << offsetDelta << "; " << blen << " : " << bytesRemaining);
        offsetDelta    += rc; 
        bytesRemaining -= rc;
        bytesRead      += rc;

    } // while bytesremaing

    return bytesRead;
}

ssize_t XrdCephBufferAlgSimple::write (const void *buf, off_t offset, size_t blen) {
    // Set a lock for any attempt at a simultaneous operation
    // Use recursive, as flushCache also calls the lock and don't want to deadlock
    const std::lock_guard<std::recursive_mutex> lock(m_data_mutex); 

    // take the data in buf and put it into the cache; when the cache is full, write to underlying storage
    // remember to flush the cache at the end of operations ... 
    ssize_t rc(-1);
    ssize_t bytesWrittenToStorage(0);

    if (blen == 0) {
        return 0; // nothing to write; are we done?
    }

    /** 
     * We expect the next write to be in order and well defined. 
     * Determine the expected offset, and compare against offset provided
     * Expected offset is the end of the buffer. 
     * m_bufferStartingOffset is the represented offset in ceph that buffer[0] represents
    */
    off_t expected_offset = (off_t)(m_bufferStartingOffset + m_bufferLength);

    if ((offset != expected_offset) && (m_bufferLength > 0) ) {
        // for the moment we just log that there is some non expected offset value 
        // TODO, might be dangerous to flush the cache on non-aligned writes ... 
        BUFLOG("Non expected offset: " << rc << "  " << offset << "  " << expected_offset);
        // rc = flushWriteCache();
        // if (rc < 0) {
        //     return rc; // TODO return correct errors
        // }
    } // mismatched offset

    //! We should be equally careful if the offset of the buffer start is not aligned sensibly.
    //! Log this only for now, but #TODO, this should be come an error condition for over cautitious behaviour.
    if ( (m_bufferStartingOffset % m_bufferdata->capacity()) != 0 ) {
        BUFLOG(" Non aligned offset?" << m_bufferStartingOffset << " "
        <<  m_bufferdata->capacity() << " " <<  m_bufferStartingOffset % m_bufferdata->capacity() );
    }

    // Commmented out below. It would be good to pass writes, which are larger than the buffer size,
    // straight-through. However if the ranges are not well aligned, this could be an issue.
    // And, what then to do about a possible partial filled buffer?

    // if (blen >= m_bufferdata->capacity()) {
    //     // TODO, might be dangerous to flush the cache on non-aligned writes ... 
    //     // flush the cache now, if needed
    //     rc = flushWriteCache();
    //     if (rc < 0) {
    //         return rc; // TODO return correct errors
    //     }
    //     bytesWrittenToStorage += rc;

    //     // Size is larger than the buffer; send the write straight through
    //     std::clog << "XrdCephBufferAlgSimple::write: Readthrough cache: fd: " << m_fd 
    //               << " " << offset << " " << blen << std::endl;
    //     // larger than cache, so read through, and invalidate the cache anyway
    //     m_bufferdata->invalidate();
    //     m_bufferLength=0;
    //     m_bufferStartingOffset=0;
    //     rc =  ceph_posix_pwrite(m_fd, buf, blen, offset);
    //     if (rc < 0) {
    //         return rc; // TODO return correct errors
    //     }
    //     bytesWrittenToStorage += rc;
    //     return rc;
    // }

    /**
     * @brief Provide some sanity checking for the write to the buffer.
     * We call an error on this conditions as there is no immediate solution that is satisfactory.
     */
    if ((offset != expected_offset) && (m_bufferLength > 0) ) {
        BUFLOG("Error trying to write out of order: expeted at: " << expected_offset 
        << " got offset" << offset << " of len " << blen);
        return -EINVAL; 
    }
    if (offset < 0) {
        BUFLOG("Got a negative offset: " << offset);
        return -EINVAL; 
    }


    size_t bytesRemaining = blen; //!< track how many bytes left to write
    size_t bytesWritten = 0;

    /** Typically would expect only one loop, i.e. the write request is smaller than the buffer.
     * If bigger, or the request stradles the end of the buffer, will need another loop
     */
    while (bytesRemaining > 0) { 
        /** 
         * If the cache is already full, lets flush to disk now
        */
        if (m_bufferLength == m_bufferdata->capacity()) {
            rc = flushWriteCache();
            if (rc < 0) {
               return rc; 
            }
            bytesWrittenToStorage += rc;
        } // at capacity; 

        if (m_bufferLength == 0) {
            // cache is currently empty, so set the 'reference' to the external offset now
            m_bufferStartingOffset = offset + bytesWritten;
        }
        //add data to the cache from buf, from buf[offsetDelta] to the cache at position m_bufferLength
        // make sure to write only as many bytes as left in the cache.
        size_t nBytesToWrite = std::min(bytesRemaining, m_bufferdata->capacity()-m_bufferLength);
        const void* bufAtOffset = (void*)((char*)buf +  bytesWritten); // nasty cast as void* doesn't do arithmetic
        if (nBytesToWrite == 0) {
            BUFLOG( "Wanting to write 0 bytes; why is that?");
        }
        rc = m_bufferdata->writeBuffer(bufAtOffset, m_bufferLength, nBytesToWrite, 0); 
        if (rc < 0) {
            BUFLOG( "WriteBuffer step failed: " << rc << "  " << m_bufferLength << "  " << blen << "  " << offset );
            return rc; // pass the error condidition upwards
        }
        if (rc != (ssize_t)nBytesToWrite) {
            BUFLOG( "WriteBuffer returned unexpected number of bytes: " << rc << "  Expected: " << nBytesToWrite << " " 
             << m_bufferLength << "  " << blen << "  " << offset );
            return -EBADE; // is bad exchange error best errno here?
        }

        // lots of repetition here; #TODO try to reduce 
        m_bufferLength += rc;
        bytesWritten   += rc;
        bytesRemaining -= rc;

    } // while byteRemaining

    /**
     * @brief Check again if we can write data into the storage
     */
    if (m_bufferLength == m_bufferdata->capacity()){
        rc = flushWriteCache();
        if (rc < 0)
        {
            return rc; // TODO return correct errors
        }
        bytesWrittenToStorage += rc;
    } // at capacity;

    //BUFLOG( "WriteBuffer " << bytesWritten << " " << bytesWrittenToStorage << "  " << offset << "  " << blen << " " );
    return bytesWritten;
}



ssize_t XrdCephBufferAlgSimple::flushWriteCache()  {
    // Set a lock for any attempt at a simultaneous operation
    // Use recursive, as write (and read) also calls the lock and don't want to deadlock
    const std::lock_guard<std::recursive_mutex> lock(m_data_mutex); // 
    // BUFLOG("flushWriteCache: " << m_bufferStartingOffset << " " << m_bufferLength);
    ssize_t rc(-1);
    if (m_bufferLength == 0) {
            BUFLOG("Empty buffer to flush: ");
            rc = 0; // not an issue
    }

    if (m_bufferLength > 0) {
        rc = m_cephio->write(m_bufferStartingOffset, m_bufferLength);
        if (rc < 0) {
            BUFLOG("WriteBuffer write step failed: " << rc);
        }
    } // some bytes to write

    // reset values
    m_bufferLength=0;
    m_bufferStartingOffset=0;
    m_bufferdata->invalidate();
    // return bytes written, or errorcode if failure
    return rc;
}


ssize_t XrdCephBufferAlgSimple::rawRead (void *buf,       off_t offset, size_t blen) {
    return -ENOSYS;
}

ssize_t XrdCephBufferAlgSimple::rawWrite(void *buf,       off_t offset, size_t blen) {
    return -ENOSYS;
}
