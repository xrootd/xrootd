/*-----------------------------------------------------------------------------
Copyright(c) 2010 - 2018 ViSUS L.L.C.,
Scientific Computing and Imaging Institute of the University of Utah

ViSUS L.L.C., 50 W.Broadway, Ste. 300, 84101 - 2044 Salt Lake City, UT
University of Utah, 72 S Central Campus Dr, Room 3750, 84112 Salt Lake City, UT

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met :

* Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

* Neither the name of the copyright holder nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOrunning_requests FTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

For additional information about this project contact : pascucci@acm.org
For support : support@visus.net
-----------------------------------------------------------------------------*/

#ifndef VISUS_NETWORK_SERVICE_H
#define VISUS_NETWORK_SERVICE_H

#include <Visus/Kernel.h>
#include <Visus/NetMessage.h>
#include <Visus/Async.h>
#include <Visus/NetSocket.h>

#include <atomic>

namespace Visus {

//////////////////////////////////////////////////////////////////////
class VISUS_KERNEL_API NetGlobalStats
{
public:

  VISUS_NON_COPYABLE_CLASS(NetGlobalStats)

#if !SWIG
  std::atomic<Int64> tot_requests;
  std::atomic<Int64> rbytes;
  std::atomic<Int64> wbytes;
  std::atomic<Int64> running_requests;
#endif

  //constructor
  NetGlobalStats() : tot_requests(0), running_requests(0), rbytes(0),wbytes(0) {
  }

  //resetStats
  void resetStats() {
    tot_requests = rbytes = wbytes = 0;
    //running_requests is a real number
  }

  //getNumRequests
  Int64 getNumRequests() const {
    return tot_requests;
  }

  //getReadBytes
  Int64 getReadBytes() const {
    return rbytes;
  }

  //getWriteBytes
  Int64 getWriteBytes() const {
    return wbytes;
  }

};

///////////////////////////////////////////////////////////////////////
class VISUS_KERNEL_API NetService 
{
public:

  VISUS_PIMPL_CLASS(NetService)

  //global_stats
  static NetGlobalStats* global_stats() {
    static NetGlobalStats ret;
    return &ret;
  }

  class VISUS_KERNEL_API Defaults
  {
  public:
    static String proxy;
    static int    proxy_port;
  };

  //constructor
  NetService(int nconnections,bool bVerbose=1);

  //destructor
  virtual ~NetService();

  //attach
  static void attach();

  //detach
  static void detach();

  //setSocketType
  void setVerbose(int value) {
    VisusAssert(!pimpl);
    this->verbose=value;
  }

  //getConnectTimeout
  int getConnectTimeout() const {
    return connect_timeout;
  }

  //setConnectTimeout
  void setConnectTimeout(int value) {
    VisusAssert(!pimpl);
    this->connect_timeout=value;
  }

  //push
  static Future<NetResponse> push(SharedPtr<NetService> service, NetRequest request);

  //getNetResponse
  static NetResponse getNetResponse(NetRequest request);

  //testSpeed
  static void testSpeed(int nconnections, int nrequests, std::vector<String> urls);

private:

  typedef std::list< std::pair< SharedPtr<NetRequest> , Promise<NetResponse> > > Waiting;

  int                          nconnections = 8;
  int                          min_wait_time = 10;
  int                          max_connections_per_sec = 0;
  int                          connect_timeout = 10; //in seconds (explanation in CONNECTTIMEOUT)
  int                          verbose = 1;

  CriticalSection              waiting_lock;
  Waiting                      waiting;

  Semaphore                    got_request;

  //entryProc
  void entryProc();

  //printStatistics
  void printStatistics(int connection_id,const NetRequest& request,const NetResponse& response);

  //handleAsync
  Future<NetResponse> handleAsync(SharedPtr<NetRequest> request);

};


} //namespace Visus


#endif //VISUS_NETWORK_SERVICE_H




