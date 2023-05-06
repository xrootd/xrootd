#include "CephIOAdapterRaw.hh"
#include "../XrdCephPosix.hh"
#include "XrdOuc/XrdOucEnv.hh"

#include <iostream>
#include <chrono>
#include <ratio>

using namespace XrdCephBuffer;

using myclock = std::chrono::steady_clock;
//using myseconds = std::chrono::duration<float,

CephIOAdapterRaw::CephIOAdapterRaw(IXrdCephBufferData * bufferdata, int fd,
            bool useStriperlessReads) : 
  m_bufferdata(bufferdata),m_fd(fd), 
  m_useStriperlessReads(useStriperlessReads) {
}

CephIOAdapterRaw::~CephIOAdapterRaw() {
  // nothing to specifically to do; just print out some stats
  float read_speed{0}, write_speed{0};
  if (m_stats_read_req.load() > 0) {
    read_speed = m_stats_read_bytes.load() / m_stats_read_timer.load() * 1e-3;
  }
  if (m_stats_write_req.load() > 0) {
    write_speed = m_stats_write_bytes.load() / m_stats_write_timer.load() * 1e-3;
  }
  BUFLOG("CephIOAdapterRaw::Summary fd:" << m_fd
                                << " nwrite:" << m_stats_write_req << " byteswritten:" << m_stats_write_bytes << " write_s:"
                                << m_stats_write_timer * 1e-3 << " writemax_s" << m_stats_write_longest * 1e-3 
                                << " write_MBs:" << write_speed 
                                << " nread:" << m_stats_read_req << " bytesread:" << m_stats_read_bytes << " read_s:"
                                << m_stats_read_timer * 1e-3 << "  readmax_s:" << m_stats_read_longest * 1e-3 
                                << " read_MBs:" << read_speed 
                                << " striperlessRead: " << m_useStriperlessReads
                                );

}

ssize_t CephIOAdapterRaw::write(off64_t offset,size_t count) {
    const void* buf = m_bufferdata->raw();
    if (!buf) return -EINVAL;

    auto start = std::chrono::steady_clock::now();
    ssize_t rc = ceph_posix_pwrite(m_fd,buf,count,offset);
    auto end = std::chrono::steady_clock::now();
    auto int_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end-start);

    // BUFLOG("CephIOAdapterRaw::write fd:" << m_fd << " " << rc << " "
    //           <<  offset << " " << count << " " << rc << " " << int_ms.count() );

    if (rc < 0) return rc;
    m_stats_write_longest = std::max(m_stats_write_longest,int_ms.count()); 
    m_stats_write_timer.fetch_add(int_ms.count());
    m_stats_write_bytes.fetch_add(rc);
    ++m_stats_write_req;
    return rc;
}


ssize_t CephIOAdapterRaw::read(off64_t offset, size_t count) {
    void* buf = m_bufferdata->raw();
    if (!buf) {
      return -EINVAL;
    }
    ssize_t rc {0};

    // no check is made whether the buffer has sufficient capacity
    auto start = std::chrono::steady_clock::now();
    rc = ceph_posix_maybestriper_pread(m_fd,buf,count,offset, m_useStriperlessReads);
    auto end = std::chrono::steady_clock::now();
    //auto elapsed = end-start;
    auto int_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end-start);

    if (rc < 0) {
      BUFLOG("CephIOAdapterRaw::read: Error in read: " << rc );
      return rc;
    }

    m_stats_read_longest = std::max(m_stats_read_longest,int_ms.count()); 
    m_stats_read_timer.fetch_add(int_ms.count());
    m_stats_read_bytes.fetch_add(rc);
    ++m_stats_read_req;

    // BUFLOG("CephIOAdapterRaw::read fd:" << m_fd << " " << rc << " " << offset
    //          << " " << count << " " << rc << " " << int_ms.count() );

    if (rc>=0) {
      m_bufferdata->setLength(rc);
      m_bufferdata->setStartingOffset(offset);
      m_bufferdata->setValid(true);
    }
    return rc;
}

