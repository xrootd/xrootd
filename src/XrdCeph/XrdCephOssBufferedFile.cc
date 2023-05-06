//------------------------------------------------------------------------------
// Copyright (c) 2014-2015 by European Organization for Nuclear Research (CERN)
// Author: Sebastien Ponce <sebastien.ponce@cern.ch>
//------------------------------------------------------------------------------
// This file is part of the XRootD software suite.
//
// XRootD is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// XRootD is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with XRootD.  If not, see <http://www.gnu.org/licenses/>.
//
// In applying this licence, CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
//------------------------------------------------------------------------------

#include <sys/types.h>
#include <unistd.h>
#include <sstream>
#include <iostream>
#include <fcntl.h>
#include <iomanip>
#include <new>
#include <ctime>
#include <chrono>
#include <thread>

#include "XrdCeph/XrdCephPosix.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdOuc/XrdOucTrace.hh"
#include "XrdSfs/XrdSfsAio.hh"
#include "XrdCeph/XrdCephOssFile.hh"

#include "XrdCeph/XrdCephOssBufferedFile.hh"
#include "XrdCeph/XrdCephBuffers/XrdCephBufferAlgSimple.hh"
#include "XrdCeph/XrdCephBuffers/XrdCephBufferDataSimple.hh"
#include "XrdCeph/XrdCephBuffers/CephIOAdapterRaw.hh"
#include "XrdCeph/XrdCephBuffers/CephIOAdapterAIORaw.hh"

#include <thread>

using namespace XrdCephBuffer;
using namespace std::chrono_literals;

extern XrdSysError XrdCephEroute;
extern XrdOucTrace XrdCephTrace;


XrdCephOssBufferedFile::XrdCephOssBufferedFile(XrdCephOss *cephoss,XrdCephOssFile *cephossDF, 
                                                size_t buffersize,const std::string& bufferIOmode,
                                                size_t maxNumberSimulBuffers):
                  XrdCephOssFile(cephoss), m_cephoss(cephoss), m_xrdOssDF(cephossDF), 
                  m_maxCountReadBuffers(maxNumberSimulBuffers),
                  m_maxBufferRetrySleepTime_ms(1000), 
                  m_bufsize(buffersize),
                  m_bufferIOmode(bufferIOmode)     
{

}

XrdCephOssBufferedFile::~XrdCephOssBufferedFile() {
    // XrdCephEroute.Say("XrdCephOssBufferedFile::Destructor");

  // remember to delete the inner XrdCephOssFile object
  if (m_xrdOssDF) {
    delete m_xrdOssDF;
    m_xrdOssDF = nullptr;
  }

}


int XrdCephOssBufferedFile::Open(const char *path, int flags, mode_t mode, XrdOucEnv &env) {

  int rc = m_xrdOssDF->Open(path, flags, mode, env);
  if (rc < 0) {
    return rc;
  }
  m_fd = m_xrdOssDF->getFileDescriptor();
  BUFLOG("XrdCephOssBufferedFile::Open got fd: " << m_fd << " " << path);
  m_flags = flags; // e.g. for write/read knowledge
  m_path  = path; // good to keep the path for final stats presentation


  // start the timer
  //m_timestart = std::chrono::steady_clock::now();
  m_timestart = std::chrono::system_clock::now();
  // return the file descriptor 
  return rc;
}

int XrdCephOssBufferedFile::Close(long long *retsz) {
  // if data is still in the buffer and we are writing, make sure to write it
  if (m_bufferAlg && (m_flags & (O_WRONLY|O_RDWR)) != 0) {
    ssize_t rc = m_bufferAlg->flushWriteCache();
    if (rc < 0) {
        LOGCEPH( "XrdCephOssBufferedFile::Close: flush Error fd: " << m_fd << " rc:" << rc );
        // still try to close the file
        ssize_t rc2 = m_xrdOssDF->Close(retsz);
        if (rc2 < 0) {
          LOGCEPH( "XrdCephOssBufferedFile::Close: Close error after flush Error fd: " << m_fd << " rc:" << rc2 );
        }
        return rc; // return the original flush error
    } else {
      LOGCEPH( "XrdCephOssBufferedFile::Close: Flushed data on close fd: " << m_fd << " rc:" << rc );
    }
  } // check for write
  const std::chrono::time_point<std::chrono::system_clock> now =
         std::chrono::system_clock::now();
  const std::time_t t_s = std::chrono::system_clock::to_time_t(m_timestart);
  const std::time_t t_c = std::chrono::system_clock::to_time_t(now);

  auto t_dur = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_timestart).count();

  LOGCEPH("XrdCephOssBufferedFile::Summary: {\"fd\":" << m_fd << ", \"Elapsed_time_ms\":" << t_dur 
          << ", \"path\":\"" << m_path  
          << "\", read_B:"   << m_bytesRead.load() 
          << ", readV_B:"     << m_bytesReadV.load() 
          << ", readAIO_B:"   << m_bytesReadAIO.load() 
          << ", writeB:"     << m_bytesWrite.load()
          << ", writeAIO_B:" << m_bytesWriteAIO.load()
          << ", startTime:\"" << std::put_time(std::localtime(&t_s), "%F %T") << "\", endTime:\"" 
          << std::put_time(std::localtime(&t_c), "%F %T") << "\""
          << ", nBuffersRead:" << m_bufferReadAlgs.size()
          << "}"); 

  return m_xrdOssDF->Close(retsz);
}


ssize_t XrdCephOssBufferedFile::ReadV(XrdOucIOVec *readV, int rnum) {
  // don't touch readV in the buffering method
  ssize_t rc = m_xrdOssDF->ReadV(readV,rnum);
  if (rc > 0) m_bytesReadV.fetch_add(rc);
  return rc;
}

ssize_t XrdCephOssBufferedFile::Read(off_t offset, size_t blen) {
  return m_xrdOssDF->Read(offset, blen);
}

ssize_t XrdCephOssBufferedFile::Read(void *buff, off_t offset, size_t blen) {
  size_t thread_id = std::hash<std::thread::id>{}(std::this_thread::get_id());

  IXrdCephBufferAlg * buffer{nullptr};
  // check for, and create if needed, a buffer
  {
    // lock in case need to create a new algorithm instance
    const std::lock_guard<std::mutex> lock(m_buf_mutex);
    auto buffer_itr = m_bufferReadAlgs.find(thread_id);
    if (buffer_itr == m_bufferReadAlgs.end()) {
      // only create a buffer, if we haven't hit the max buffers yet
      auto buffer_ptr = std::move(createBuffer());
      if (buffer_ptr) {
        buffer = buffer_ptr.get();
        m_bufferReadAlgs[thread_id] = std::move(buffer_ptr);
      } else {
        // if we can't create a buffer, we just have to pass through the read ... 
        ssize_t rc = m_xrdOssDF->Read(buff, offset, blen);
        if (rc >= 0) {
          LOGCEPH( "XrdCephOssBufferedFile::Read buffers and read failed with rc: " << rc );
        }
        return rc;
      }
    } else {
      buffer = buffer_itr->second.get();
    }
  } // scope of lock
  
  int retry_counter{m_maxBufferRetries};
  ssize_t rc {0};
  while (retry_counter > 0) {
    rc = buffer->read(buff, offset, blen);
    if (rc != -EBUSY) break; // either worked, or is a real non busy error
    LOGCEPH( "XrdCephOssBufferedFile::Read Recieved EBUSY for fd: " << m_fd << " on try: " << (m_maxBufferRetries-retry_counter) << ". Sleeping .. "
              << " rc:" << rc  << " off:" << offset << " len:" << blen);
    std::this_thread::sleep_for(m_maxBufferRetrySleepTime_ms * 1ms);
    --retry_counter;
  }
  if (retry_counter == 0) {
    // reach maximum attempts for ebusy retry; fail the job
    LOGCEPH( "XrdCephOssBufferedFile::Read Max attempts for fd: " << m_fd << " on try: " << (m_maxBufferRetries-retry_counter) << ". Terminating with -EIO: "
              << " rc:" << rc  << " off:" << offset << " len:" << blen );
    // set a permanent error code:
    rc = -EIO;    
  }
  if (rc >=0) {
    m_bytesRead.fetch_add(rc);
  } else {
    LOGCEPH( "XrdCephOssBufferedFile::Read: Read error  fd: " << m_fd << " rc:" << rc  << " off:" << offset << " len:" << blen);
  }
  // LOGCEPH( "XrdCephOssBufferedFile::Read: Read good  fd: " << m_fd << " rc:" << rc  << " off:" << offset << " len:" << blen);
  return rc;
}

int XrdCephOssBufferedFile::Read(XrdSfsAio *aiop) {
  size_t thread_id = std::hash<std::thread::id>{}(std::this_thread::get_id());
  IXrdCephBufferAlg * buffer{nullptr};
  // check for, and create if needed, a buffer
  {
    // lock in case need to create a new algorithm instance
    const std::lock_guard<std::mutex> lock(m_buf_mutex);
    auto buffer_itr = m_bufferReadAlgs.find(thread_id);
    if (buffer_itr == m_bufferReadAlgs.end()) {
      m_bufferReadAlgs[thread_id] = createBuffer();
      buffer = m_bufferReadAlgs.find(thread_id)->second.get();
    } else {
      buffer = buffer_itr->second.get();
    }
  }

  // LOGCEPH("XrdCephOssBufferedFile::AIOREAD: fd: " << m_xrdOssDF->getFileDescriptor() << "  "  << time(nullptr) << " : " 
  //         << aiop->sfsAio.aio_offset << " " 
  //         << aiop->sfsAio.aio_nbytes << " " << aiop->sfsAio.aio_reqprio << " "
  //         << aiop->sfsAio.aio_fildes );
  ssize_t rc = buffer->read_aio(aiop);
  if (rc > 0) {
    m_bytesReadAIO.fetch_add(rc);
  }  else {
    LOGCEPH( "XrdCephOssBufferedFile::Read: ReadAIO error  fd: " << m_fd << " rc:" << rc  
            << " off:" << aiop->sfsAio.aio_offset << " len:" << aiop->sfsAio.aio_nbytes );
  }
  return rc;
}

ssize_t XrdCephOssBufferedFile::ReadRaw(void *buff, off_t offset, size_t blen) {
  // #TODO; ReadRaw should bypass the buffer ?
  return m_xrdOssDF->ReadRaw(buff, offset, blen);
}

int XrdCephOssBufferedFile::Fstat(struct stat *buff) {
  return m_xrdOssDF->Fstat(buff);
}

ssize_t XrdCephOssBufferedFile::Write(const void *buff, off_t offset, size_t blen) {

  if (!m_bufferAlg) {
    m_bufferAlg = createBuffer();
    if (!m_bufferAlg) {
      LOGCEPH( "XrdCephOssBufferedFile: Error in creating buffered object");
      return -EINVAL;
    }
  }


  int retry_counter{m_maxBufferRetries};
  ssize_t rc {0};
  while (retry_counter > 0) {
    rc = m_bufferAlg->write(buff, offset, blen);
    if (rc != -EBUSY) break; // either worked, or is a real non busy error
    LOGCEPH( "XrdCephOssBufferedFile::Write Recieved EBUSY for fd: " << m_fd << " on try: " << (m_maxBufferRetries-retry_counter) << ". Sleeping .. "
              << " rc:" << rc  << " off:" << offset << " len:" << blen);
    std::this_thread::sleep_for(m_maxBufferRetrySleepTime_ms * 1ms);
    --retry_counter;
  }
  if (retry_counter == 0) {
    // reach maximum attempts for ebusy retry; fail the job
    LOGCEPH( "XrdCephOssBufferedFile::Write Max attempts for fd: " << m_fd << " on try: " << (m_maxBufferRetries-retry_counter) << ". Terminating with -EIO: "
              << " rc:" << rc  << " off:" << offset << " len:" << blen );
    // set a permanent error code:
    rc = -EIO;    
  }
  if (rc >=0) {
    m_bytesWrite.fetch_add(rc);
  }  else {
    LOGCEPH( "XrdCephOssBufferedFile::Write: Write error  fd: " << m_fd << " rc:" << rc  << " off:" << offset << " len:" << blen);
  }
  return rc;
}

int XrdCephOssBufferedFile::Write(XrdSfsAio *aiop) {
  if (!m_bufferAlg) {
    m_bufferAlg = createBuffer();
    if (!m_bufferAlg) {
      LOGCEPH( "XrdCephOssBufferedFile: Error in creating buffered object");
      return -EINVAL;
    }
  }

  // LOGCEPH("XrdCephOssBufferedFile::AIOWRITE: fd: " << m_xrdOssDF->getFileDescriptor() << "  "   << time(nullptr) << " : " 
  //         << aiop->sfsAio.aio_offset << " " 
  //         << aiop->sfsAio.aio_nbytes << " " << aiop->sfsAio.aio_reqprio << " "
  //         << aiop->sfsAio.aio_fildes << " " );
  ssize_t rc = m_bufferAlg->write_aio(aiop);
  if (rc > 0) {
     m_bytesWriteAIO.fetch_add(rc);
  }   else {
    LOGCEPH( "XrdCephOssBufferedFile::Write: WriteAIO error  fd: " << m_fd << " rc:" << rc  
            << " off:" << aiop->sfsAio.aio_offset << " len:" << aiop->sfsAio.aio_nbytes );
  }
  return rc;

}

int XrdCephOssBufferedFile::Fsync() {
  return m_xrdOssDF->Fsync();
}

int XrdCephOssBufferedFile::Ftruncate(unsigned long long len) {
  return m_xrdOssDF->Ftruncate(len);
}


std::unique_ptr<XrdCephBuffer::IXrdCephBufferAlg> XrdCephOssBufferedFile::createBuffer() {
    std::unique_ptr<IXrdCephBufferAlg> bufferAlg;

    size_t bufferSize {m_bufsize};  // create buffer of default size
    if (m_bufferReadAlgs.size() >= m_maxCountReadBuffers) {
      BUFLOG("XrdCephOssBufferedFile: buffer reached max number of simul-buffers for this file: creating only 1MiB buffer" );
      bufferSize = 1048576;
    } else {
      BUFLOG("XrdCephOssBufferedFile: buffer: got " << m_bufferReadAlgs.size() << " buffers already");
    }

    try {
      std::unique_ptr<IXrdCephBufferData> cephbuffer = std::unique_ptr<IXrdCephBufferData>(new XrdCephBufferDataSimple(bufferSize));
      std::unique_ptr<ICephIOAdapter>     cephio;
      if (m_bufferIOmode == "aio") {
          cephio = std::unique_ptr<ICephIOAdapter>(new CephIOAdapterAIORaw(cephbuffer.get(),m_fd));
      } else if (m_bufferIOmode == "io") {
          cephio = std::unique_ptr<ICephIOAdapter>(new CephIOAdapterRaw(cephbuffer.get(),m_fd,
                                                  !m_cephoss->m_useDefaultPreadAlg));
      } else {
            BUFLOG("XrdCephOssBufferedFile: buffer mode needs to be one of aio|io " );
            m_xrdOssDF->Close();
            return bufferAlg; // invalid instance;
      }

      LOGCEPH( "XrdCephOssBufferedFile::Open: fd: " << m_fd <<  " Buffer created: " << cephbuffer->capacity() );
      bufferAlg = std::unique_ptr<IXrdCephBufferAlg>(new XrdCephBufferAlgSimple(std::move(cephbuffer),std::move(cephio),m_fd) );
    } catch (const std::bad_alloc &e) {
      BUFLOG("XrdCephOssBufferedFile: Bad memory allocation in buffer: " << e.what() );
    }

    return bufferAlg;
  }
