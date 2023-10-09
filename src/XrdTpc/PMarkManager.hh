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
#ifndef XROOTD_PMARKMANAGER_HH
#define XROOTD_PMARKMANAGER_HH

#include "XrdNet/XrdNetPMark.hh"
#include "XrdSec/XrdSecEntity.hh"
#include "XrdNet/XrdNetAddrInfo.hh"
#include "XrdNet/XrdNetAddr.hh"
#include "XrdHttp/XrdHttpExtHandler.hh"

#include <map>
#include <memory>
#include <queue>

/**
 * This class will manage packet marking handles for TPC transfers
 *
 * Each time a socket will be opened by curl (via the opensocket_callback), the manager
 * will register the related information to the socket.
 *
 * Once the transfer will start we will start the packet marking by creating XrdNetPMark::Handle
 * objects from the socket information previously registered.
 *
 * In the case of multi-stream HTTP TPC transfers, a packet marking handle will be created for each stream.
 * The first one will be created as a basic one. The other will be created using the first packet marking handle as a basis.
 */
class PMarkManager {
public:

  /**
   * This class allows to create and keep a XrdSecEntity object
   * from the socket file descriptor and address
   * Everything is done on the constructor
   *
   * These infos will be used later on when we create new PMark handles
   */
  class SocketInfo {
  public:
    SocketInfo(int fd, const struct sockaddr * sockP);
    XrdNetAddr netAddr;
    XrdSecEntity client;
  };

  PMarkManager(XrdNetPMark * pmark);
  /**
   * Add the connected socket information that will be used for packet marking to this manager class
   * Note: these info will only be added if startTransfer(...) has been called. It allows
   * to ensure that the connection will be related to the data transfers and not for anything else. We only want
   * to mark the traffic of the transfers.
   * @param fd the socket file descriptor
   * @param sockP the structure describing the address of the socket
   */
  void addFd(int fd, const struct sockaddr * sockP);

  /**
   * Calling this function will indicate that the connections that will happen will be related to the
   * data transfer. The addFd(...) function will then register any socket that is created after this function
   * will be called.
   * @param req the request object that will be used later on to get some information about the transfer
   */
  void startTransfer(XrdHttpExtReq * req);
  /**
   * Creates the different packet marking handles allowing to mark the transfer packets
   *
   * Call this after the curl_multi_perform() has been called.
   */
  void beginPMarks();

  /**
   * This function deletes the PMark handle associated to the fd passed in parameter
   * Use this before closing the associated socket! Otherwise the information contained in the firefly
   * (e.g sent bytes or received bytes) will have values equal to 0.
   * @param fd the fd of the socket to be closed
   */
  void endPmark(int fd);

  virtual ~PMarkManager() = default;
private:
  // The queue of socket information from which we will create the packet marking handles
  std::queue<SocketInfo> mSocketInfos;
  // The map of socket FD and packet marking handles
  std::map<int,std::unique_ptr<XrdNetPMark::Handle>> mPmarkHandles;
  // The instance of the packet marking functionality
  XrdNetPMark * mPmark;
  // Is true when startTransfer(...) has been called
  bool mTransferWillStart;
  // The XrdHttpTPC request information
  XrdHttpExtReq * mReq;
  // The file descriptor used to create the first packet marking handle
  int mInitialFD = -1;
};


#endif //XROOTD_PMARKMANAGER_HH
