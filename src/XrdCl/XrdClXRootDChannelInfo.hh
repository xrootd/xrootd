/*
 * XrdClXRootDChannelInfo.hh
 *
 *  Created on: Oct 5, 2016
 *      Author: simonm
 */

#ifndef SRC_XRDCL_XRDCLXROOTDCHANNELINFO_HH_
#define SRC_XRDCL_XRDCLXROOTDCHANNELINFO_HH_

#include "XrdCl/XrdClSIDManager.hh"
#include "XrdSec/XrdSecInterface.hh"
#include "XrdSec/XrdSecProtect.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdSys/XrdSysPthread.hh"

#include <vector>
#include <string>
#include <set>
#include <stdint.h>

namespace XrdCl
{
  //----------------------------------------------------------------------------
  //! Information holder for XRootDStreams
  //----------------------------------------------------------------------------
  struct XRootDStreamInfo
  {
    //--------------------------------------------------------------------------
    // Define the stream status for the link negotiation purposes
    //--------------------------------------------------------------------------
    enum StreamStatus
    {
      Disconnected,
      Broken,
      HandShakeSent,
      HandShakeReceived,
      LoginSent,
      AuthSent,
      BindSent,
      EndSessionSent,
      Connected
    };

    //--------------------------------------------------------------------------
    // Constructor
    //--------------------------------------------------------------------------
    XRootDStreamInfo(): status( Disconnected ), pathId( 0 )
    {
    }

    StreamStatus status;
    uint8_t      pathId;
  };

  //----------------------------------------------------------------------------
  //! Information holder for xrootd channels
  //----------------------------------------------------------------------------
  struct XRootDChannelInfo
  {
    //--------------------------------------------------------------------------
    // Constructor
    //--------------------------------------------------------------------------
    XRootDChannelInfo():
      serverFlags(0),
      protocolVersion(0),
      firstLogIn(true),
      sidManager(0),
      authBuffer(0),
      authProtocol(0),
      authParams(0),
      authEnv(0),
      openFiles(0),
      waitBarrier(0),
      protection(0),
      signprot(0),
      protRespBody(0),
      protRespSize(0)
    {
      sidManager = new SIDManager();
      memset( sessionId, 0, 16 );
      memset( oldSessionId, 0, 16 );
    }

    //--------------------------------------------------------------------------
    // Destructor
    //--------------------------------------------------------------------------
    ~XRootDChannelInfo()
    {
      delete    sidManager;
      delete [] authBuffer;
      delete    protRespBody;

      if( protection )
        protection->Delete();

      if( signprot )
        signprot->Delete();
    }

    typedef std::vector<XRootDStreamInfo> StreamInfoVector;

    //--------------------------------------------------------------------------
    // Data
    //--------------------------------------------------------------------------
    uint32_t                     serverFlags;
    uint32_t                     protocolVersion;
    uint8_t                      sessionId[16];
    uint8_t                      oldSessionId[16];
    bool                         firstLogIn;
    SIDManager                  *sidManager;
    char                        *authBuffer;
    XrdSecProtocol              *authProtocol;
    XrdSecParameters            *authParams;
    XrdOucEnv                   *authEnv;
    StreamInfoVector             stream;
    std::string                  streamName;
    std::string                  authProtocolName;
    std::set<uint16_t>           sentOpens;
    std::set<uint16_t>           sentCloses;
    uint32_t                     openFiles;
    time_t                       waitBarrier;
    XrdSecProtect               *protection;
    XrdSecProtocol              *signprot;
    ServerResponseBody_Protocol *protRespBody;
    unsigned int                 protRespSize;
    XrdSysMutex                  mutex;
  };

};

#endif /* SRC_XRDCL_XRDCLXROOTDCHANNELINFO_HH_ */
