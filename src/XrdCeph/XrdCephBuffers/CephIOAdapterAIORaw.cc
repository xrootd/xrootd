#include "CephIOAdapterAIORaw.hh"
#include "../XrdCephPosix.hh"
#include "XrdOuc/XrdOucEnv.hh"

#include <iostream>
#include <chrono>
#include <ratio>
#include <functional>
#include <memory>
#include <thread>
#include <chrono>

using namespace XrdCephBuffer;

using myclock = std::chrono::steady_clock;
//using myseconds = std::chrono::duration<float,

namespace
{
  static void aioReadCallback(XrdSfsAio *aiop, size_t rc)
  {
    // as in XrdCephOssFile
    aiop->Result = rc;
    aiop->doneRead();
  }
  static void aioWriteCallback(XrdSfsAio *aiop, size_t rc)
  {
    aiop->Result = rc;
    aiop->doneWrite();
  }

} // anonymous namespace

CephBufSfsAio::CephBufSfsAio() : m_lock(m_mutex)
{
}

void CephBufSfsAio::doneRead()
{
  //BUFLOG("DoneRead");
  m_dataOpDone = true;
  m_lock.unlock();
  m_condVar.notify_all();
}

void CephBufSfsAio::doneWrite()
{
  //BUFLOG("DoneWrite");
  m_dataOpDone = true;
  m_lock.unlock();
  m_condVar.notify_all();
}

CephIOAdapterAIORaw::CephIOAdapterAIORaw(IXrdCephBufferData *bufferdata, int fd) : m_bufferdata(bufferdata), m_fd(fd)
{
}

CephIOAdapterAIORaw::~CephIOAdapterAIORaw()
{
  // nothing to specifically to do; just print out some stats
  float read_speed{0}, write_speed{0};
  if (m_stats_read_req.load() > 0) {
    read_speed = m_stats_read_bytes.load() / m_stats_read_timer.load() * 1e-3;
  }
  if (m_stats_write_req.load() > 0) {
    write_speed = m_stats_write_bytes.load() / m_stats_write_timer.load() * 1e-3;
  }
  BUFLOG("CephIOAdapterAIORaw::Summary fd:" << m_fd
                                            << " nwrite:" << m_stats_write_req << " byteswritten:" << m_stats_write_bytes << " write_s:"
                                            << m_stats_write_timer * 1e-3 << " writemax_s" << m_stats_write_longest * 1e-3 
                                            << " write_MBs:" << write_speed 
                                            << " nread:" << m_stats_read_req << " bytesread:" << m_stats_read_bytes << " read_s:"
                                            << m_stats_read_timer * 1e-3 << "  readmax_s:" << m_stats_read_longest * 1e-3 
                                            << " read_MBs:" << read_speed );
}

ssize_t CephIOAdapterAIORaw::write(off64_t offset, size_t count)
{
  void *buf = m_bufferdata->raw();
  if (!buf) {
    BUFLOG("CephIOAdapterAIORaw::write null buffer was provided.")
    return -EINVAL; 
  }
  //BUFLOG("Make aio");
  std::unique_ptr<XrdSfsAio> aiop = std::unique_ptr<XrdSfsAio>(new CephBufSfsAio());
  aiocb &sfsAio = aiop->sfsAio;
  // set the necessary parameters for the read, e.g. buffer pointer, offset and length
  sfsAio.aio_buf = buf;
  sfsAio.aio_nbytes = count;
  sfsAio.aio_offset = offset;
  // need the concrete object for the blocking / wait
  CephBufSfsAio *ceph_aiop = dynamic_cast<CephBufSfsAio *>(aiop.get());

  long dt_ns{0};
  ssize_t rc{0};
  { // brace is for timer RAII
    XrdCephBuffer::Timer_ns timer(dt_ns);
    rc = ceph_aio_write(m_fd, aiop.get(), aioWriteCallback);

    if (rc < 0) {
      BUFLOG("CephIOAdapterAIORaw::write ceph_aio_write returned rc:" << rc)
      return rc;
    }

    while (!ceph_aiop->isDone())
    {
      ceph_aiop->m_condVar.wait(ceph_aiop->m_lock, std::bind(&CephBufSfsAio::isDone, ceph_aiop));
    }
  } // timer brace

  // cleanup
  rc = ceph_aiop->Result;
  if (rc < 0) {
      BUFLOG("CephIOAdapterAIORaw::write ceph_aiop->Result returned rc:" << rc)
  }

  // BUFLOG("CephIOAdapterAIORaw::write fd:" << m_fd << " off:"
  //                                         << offset << " len:" << count << " rc:" << rc << " ms:" << dt_ns / 1000000);

  m_stats_write_longest = std::max(m_stats_write_longest, dt_ns / 1000000);
  m_stats_write_timer.fetch_add(dt_ns / 1000000);
  m_stats_write_bytes.fetch_add(rc);
  ++m_stats_write_req;
  return rc;
}

ssize_t CephIOAdapterAIORaw::read(off64_t offset, size_t count)
{
  void *buf = m_bufferdata->raw();
  if (!buf)
  {
    BUFLOG("CephIOAdapterAIORaw::read null buffer was provided.")
    return -EINVAL;
  }

  std::unique_ptr<XrdSfsAio> aiop = std::unique_ptr<XrdSfsAio>(new CephBufSfsAio());
  aiocb &sfsAio = aiop->sfsAio;
  // set the necessary parameters for the read, e.g. buffer pointer, offset and length
  sfsAio.aio_buf = buf;
  sfsAio.aio_nbytes = count;
  sfsAio.aio_offset = offset;
  // need the concrete object for the blocking / wait
  CephBufSfsAio *ceph_aiop = dynamic_cast<CephBufSfsAio *>(aiop.get());

  long dt_ns{0};
  ssize_t rc{0};
  { // timer brace RAII
    XrdCephBuffer::Timer_ns timer(dt_ns);
    // no check is made whether the buffer has sufficient capacity
    //      rc = ceph_posix_pread(m_fd,buf,count,offset);
    //BUFLOG("Submit aio read: ");
    rc = ceph_aio_read(m_fd, aiop.get(), aioReadCallback);

    if (rc < 0)
      return rc;

    // now block until the read is done
    // take the lock on the aio object
    // while(!ceph_aiop->isDone()) { ceph_aiop->m_condVar.wait(lock,std::bind(&CephBufSfsAio::isDone,ceph_aiop) ); }
    while (!ceph_aiop->isDone())
    {
      ceph_aiop->m_condVar.wait(ceph_aiop->m_lock, std::bind(&CephBufSfsAio::isDone, ceph_aiop));
    }
  } // timer brace

  // cleanup
  rc = ceph_aiop->Result;

  m_stats_read_longest = std::max(m_stats_read_longest, dt_ns / 1000000);
  m_stats_read_timer.fetch_add(dt_ns * 1e-6);
  m_stats_read_bytes.fetch_add(rc);
  ++m_stats_read_req;

  // BUFLOG("CephIOAdapterAIORaw::read fd:" << m_fd << " " << offset
  //                                        << " " << count << " " << rc << " " << dt_ns * 1e-6);

  if (rc >= 0)
  {
    m_bufferdata->setLength(rc);
    m_bufferdata->setStartingOffset(offset);
    m_bufferdata->setValid(true);
  }
  return rc;
}
