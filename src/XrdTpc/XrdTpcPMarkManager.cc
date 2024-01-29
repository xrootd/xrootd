//------------------------------------------------------------------------------
// This file is part of XrdTpcTPC
//
// Copyright (c) 2023 by European Organization for Nuclear Research (CERN)
// Author: Cedric Caffy <ccaffy@cern.ch>
// File Date: Oct 2023
//------------------------------------------------------------------------------
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
//------------------------------------------------------------------------------


#include <sstream>
#include "XrdTpcPMarkManager.hh"

namespace XrdTpc
{
PMarkManager::SocketInfo::SocketInfo(int fd, const struct sockaddr * sockP) {
  netAddr.Set(sockP,fd);
  client.addrInfo = static_cast<XrdNetAddrInfo*>(&netAddr);
}

PMarkManager::PMarkManager(XrdNetPMark *pmark) : mPmark(pmark), mTransferWillStart(false) {}

void PMarkManager::addFd(int fd, const struct sockaddr * sockP) {
  if(mPmark && mTransferWillStart && mReq->mSciTag >= 0) {
    // The transfer will start and the packet marking has been configured, this socket must be registered for future packet marking
    mSocketInfos.emplace(fd, sockP);
  }
}

void PMarkManager::startTransfer(XrdHttpExtReq * req) {
  mReq = req;
  mTransferWillStart = true;
}

void PMarkManager::beginPMarks() {
  if(!mSocketInfos.empty() && mPmarkHandles.empty()) {
    // Create the first pmark handle that will be used as a basis for the other handles
    // if that handle cannot be created (mPmark->Begin() would return nullptr), then the packet marking will not work
    // This base pmark handle will be placed at the beginning of the vector of pmark handles
    std::stringstream ss;
    ss << "scitag.flow=" << mReq->mSciTag;
    SocketInfo & sockInfo = mSocketInfos.front();
    mInitialFD = sockInfo.client.addrInfo->SockFD();
    std::unique_ptr<XrdNetPMark::Handle> initialPmark(mPmark->Begin(sockInfo.client, mReq->resource.c_str(), ss.str().c_str(), "http-tpc"));
    if(initialPmark) {
      // It may happen that the socket attached to the file descriptor is not connected yet. If this is the case the initial
      // Pmark will be nullptr...
      mPmarkHandles.emplace(mInitialFD,std::move(initialPmark));
      mSocketInfos.pop();
    }
  } else {
    // The first pmark handle was created, or not. Create the other pmark handles from the other connected sockets
    while(!mSocketInfos.empty()) {
      SocketInfo & sockInfo = mSocketInfos.front();
      if(mPmarkHandles[mInitialFD]){
        std::unique_ptr<XrdNetPMark::Handle> pmark(mPmark->Begin(*sockInfo.client.addrInfo, *mPmarkHandles[mInitialFD], nullptr));
        if(pmark) {
          mPmarkHandles.emplace(sockInfo.client.addrInfo->SockFD(),std::move(pmark));
          mSocketInfos.pop();
        } else {
          // We could not create the pmark handle from the socket, we break the loop, we will retry later on when
          // this function will be called again.
          break;
        }
      }
    }
  }
}

void PMarkManager::endPmark(int fd) {
  // We need to delete the PMark handle associated to the fd passed in parameter
  // we just look for it and reset the unique_ptr to nullptr to trigger the PMark handle deletion
  mPmarkHandles.erase(fd);
}
} // namespace XrdTpc
