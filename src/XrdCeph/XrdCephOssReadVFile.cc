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
#include <memory>
#include <algorithm>
#include <fcntl.h>

#include "XrdCeph/XrdCephPosix.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdOuc/XrdOucTrace.hh"
#include "XrdSfs/XrdSfsAio.hh"
#include "XrdCeph/XrdCephOssFile.hh"

#include "XrdCeph/XrdCephOssReadVFile.hh"
#include "XrdCeph/XrdCephBuffers/XrdCephReadVBasic.hh"
#include "XrdCeph/XrdCephBuffers/XrdCephReadVNoOp.hh"

using namespace XrdCephBuffer;

extern XrdSysError XrdCephEroute;
extern XrdOucTrace XrdCephTrace;

XrdCephOssReadVFile::XrdCephOssReadVFile(XrdCephOss *cephoss,XrdCephOssFile *cephossDF,const std::string& algname):
XrdCephOssFile(cephoss), m_cephoss(cephoss), m_xrdOssDF(cephossDF),m_algname(algname)
{
  if (!m_xrdOssDF) XrdCephEroute.Say("XrdCephOssReadVFile::Null m_xrdOssDF");

  if (m_algname == "passthrough") { // #TODO consider to use a factory method. but this is simple enough for now
      m_readVAdapter =  std::unique_ptr<XrdCephBuffer::IXrdCephReadVAdapter>(new XrdCephBuffer::XrdCephReadVNoOp());
  } else if (m_algname == "basic") { 
      m_readVAdapter =  std::unique_ptr<XrdCephBuffer::IXrdCephReadVAdapter>(new XrdCephBuffer::XrdCephReadVBasic());
  } else {
    XrdCephEroute.Say("XrdCephOssReadVFile::ERROR Invalid ReadV algorthm passed; defaulting to passthrough");
    m_algname = "passthrough";
    m_readVAdapter =  std::unique_ptr<XrdCephBuffer::IXrdCephReadVAdapter>(new XrdCephBuffer::XrdCephReadVNoOp());
  }
  LOGCEPH("XrdCephOssReadVFile Algorithm type: " << m_algname);
}

XrdCephOssReadVFile::~XrdCephOssReadVFile() {
  if (m_xrdOssDF) {
    delete m_xrdOssDF;
    m_xrdOssDF = nullptr;
  }

}

int XrdCephOssReadVFile::Open(const char *path, int flags, mode_t mode, XrdOucEnv &env) {
  int rc = m_xrdOssDF->Open(path, flags, mode, env);
  if (rc < 0) {
    return rc;
  }
  m_fd = m_xrdOssDF->getFileDescriptor();
  LOGCEPH("XrdCephOssReadVFile::Open: fd: " << m_fd << " " << path );
  return rc;
}

int XrdCephOssReadVFile::Close(long long *retsz) {
  LOGCEPH("XrdCephOssReadVFile::Close: retsz: " << retsz << " Time_ceph_s: " << m_timer_read_ns.load()*1e-9 << " count: " 
      <<  m_timer_count.load()  << " size_B: " << m_timer_size.load()
      << " longest_s:" <<  m_timer_longest.load()*1e-9);

  return m_xrdOssDF->Close(retsz);
}


ssize_t XrdCephOssReadVFile::ReadV(XrdOucIOVec *readV, int rnum) {
  int fd = m_xrdOssDF->getFileDescriptor();
  LOGCEPH("XrdCephOssReadVFile::ReadV: fd: " << fd  << " " << rnum );

  std::stringstream msg_extents; 
  msg_extents << "XrdCephOssReadVFile::Extentslist={\"fd\": " << fd << ", \"EXTENTS\":[";

  ExtentHolder extents(rnum);
  for (int i = 0; i < rnum; i++) {
    extents.push_back(Extent(readV[i].offset, readV[i].size));
    msg_extents << "[" << readV[i].offset << "," << readV[i].size << "]," ;
  }
  msg_extents << "]}";
  //XrdCephEroute.Say(msg_extents.str().c_str()); msg_extents.clear();
  if (m_extraLogging) {
    // improve this so no wasted calls if logging is disabled
    LOGCEPH(msg_extents.str());
    msg_extents.clear();
  }

  LOGCEPH("XrdCephOssReadVFile::Extents: fd: "<< fd << " "  << extents.size() << " " << extents.len() << " " 
      << extents.begin() << " " << extents.end() << " " << extents.bytesContained() 
      << " " << extents.bytesMissing());

  // take the input set of extents and return a vector of merged extents (covering the range to read)
  std::vector<ExtentHolder> mappedExtents = m_readVAdapter->convert(extents);


  // counter is the iterator to the original readV elements, and is incremented for each chunk that's returned
  int nbytes = 0, curCount = 0, counter(0);
  size_t totalBytesRead(0), totalBytesUseful(0);

  // extract the largest range of the extents, and create a buffer.
  size_t buffersize{0};
  for (std::vector<ExtentHolder>::const_iterator ehit = mappedExtents.cbegin(); ehit!= mappedExtents.cend(); ++ehit ) {
    buffersize = std::max(buffersize, ehit->len());
  }
  std::vector<char> buffer;
  buffer.reserve(buffersize);


  //LOGCEPH("mappedExtents: len: " << mappedExtents.size() );
  for (std::vector<ExtentHolder>::const_iterator ehit = mappedExtents.cbegin(); ehit!= mappedExtents.cend(); ++ehit ) {
    off_t off = ehit->begin();
    size_t len = ehit->len();

    //LOGCEPH("outerloop: " << off << " " << len << " " << ehit->end() << " " << " " << ehit->size() );

    // read the full extent into the buffer
    long timed_read_ns{0};
    {Timer_ns ts(timed_read_ns); 
      curCount = m_xrdOssDF->Read(buffer.data(), off, len);
    } // timer scope 
    ++m_timer_count;
    auto l = m_timer_longest.load(); 
    m_timer_longest.store(max(l,timed_read_ns)); // doesn't quite prevent race conditions
    m_timer_read_ns.fetch_add(timed_read_ns);
    m_timer_size.fetch_add(curCount);

    // check that the correct amount of data was read. 
    // std:: clog << "buf Read " << curCount << std::endl;
    if (curCount != (ssize_t)len) {
      return (curCount < 0 ? curCount : -ESPIPE);
    }
    totalBytesRead += curCount;
    totalBytesUseful += ehit->bytesContained(); 


    // now read out into the original readV requests for each of the held inner extents
    const char* data = buffer.data();
    const ExtentContainer& innerExtents = ehit->extents();
    for (ExtentContainer::const_iterator it = innerExtents.cbegin(); it != innerExtents.cend(); ++it) {
      off_t innerBegin = it->begin() - off;
      off_t innerEnd   = it->end()   - off; 
      //LOGCEPH( "innerloop: " << innerBegin << " " << innerEnd << " " << off << " " 
      //          << it->begin() << " " << it-> end() << " " 
      //          << readV[counter].offset << " " << readV[counter].size);
      std::copy(data+innerBegin, data+innerEnd, readV[counter].data );
      nbytes += it->len();
      ++counter; // next element
    } // inner extents

  } // outer extents
  LOGCEPH( "readV returning " << nbytes << " bytes: " << "Read:  " <<totalBytesRead << " Useful: " << totalBytesUseful );
  return nbytes;

}

ssize_t XrdCephOssReadVFile::Read(off_t offset, size_t blen) {
  return m_xrdOssDF->Read(offset,blen);
}

ssize_t XrdCephOssReadVFile::Read(void *buff, off_t offset, size_t blen) {
  return m_xrdOssDF->Read(buff,offset,blen);
}

int XrdCephOssReadVFile::Read(XrdSfsAio *aiop) {
  return m_xrdOssDF->Read(aiop);
}

ssize_t XrdCephOssReadVFile::ReadRaw(void *buff, off_t offset, size_t blen) {
  return m_xrdOssDF->ReadRaw(buff, offset, blen);
}

int XrdCephOssReadVFile::Fstat(struct stat *buff) {
  return m_xrdOssDF->Fstat(buff);
}

ssize_t XrdCephOssReadVFile::Write(const void *buff, off_t offset, size_t blen) {
  return m_xrdOssDF->Write(buff,offset,blen);
}

int XrdCephOssReadVFile::Write(XrdSfsAio *aiop) {
  return m_xrdOssDF->Write(aiop);
}

int XrdCephOssReadVFile::Fsync() {
  return m_xrdOssDF->Fsync();
}

int XrdCephOssReadVFile::Ftruncate(unsigned long long len) {
  return m_xrdOssDF->Ftruncate(len);
}
