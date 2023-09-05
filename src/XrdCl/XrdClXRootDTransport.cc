//------------------------------------------------------------------------------
// Copyright (c) 2011-2014 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
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

#include "XrdCl/XrdClXRootDTransport.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClSocket.hh"
#include "XrdCl/XrdClMessage.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClSIDManager.hh"
#include "XrdCl/XrdClUtils.hh"
#include "XrdCl/XrdClTransportManager.hh"
#include "XrdCl/XrdClTls.hh"
#include "XrdNet/XrdNetAddr.hh"
#include "XrdNet/XrdNetUtils.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdOuc/XrdOucErrInfo.hh"
#include "XrdOuc/XrdOucUtils.hh"
#include "XrdOuc/XrdOucCRC.hh"
#include "XrdOuc/XrdOucTokenizer.hh"
#include "XrdSys/XrdSysTimer.hh"
#include "XrdSys/XrdSysAtomics.hh"
#include "XrdSys/XrdSysPlugin.hh"
#include "XrdSec/XrdSecLoadSecurity.hh"
#include "XrdSec/XrdSecProtect.hh"
#include "XrdSys/XrdSysE2T.hh"
#include "XrdCl/XrdClTls.hh"
#include "XrdCl/XrdClSocket.hh"
#include "XProtocol/XProtocol.hh"
#include "XrdVersion.hh"

#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sstream>
#include <iomanip>
#include <set>
#include <limits>

#include <atomic>

XrdVERSIONINFOREF( XrdCl );

namespace XrdCl
{
  struct PluginUnloadHandler
  {
      PluginUnloadHandler() : unloaded( false ) { }

      static void UnloadHandler()
      {
        UnloadHandler( "root" );
        UnloadHandler( "xroot" );
      }

      static void UnloadHandler( const std::string &trProt )
      {
        TransportManager *trManager = DefaultEnv::GetTransportManager();
        TransportHandler *trHandler = trManager->GetHandler( trProt );
        trHandler->WaitBeforeExit();
      }

      void Register( const std::string &protocol )
      {
        XrdSysRWLockHelper scope( lock, false ); // obtain write lock
        std::pair< std::set<std::string>::iterator, bool > ret = protocols.insert( protocol );
        // if that's the first time we are using the protocol, the sec lib
        // was just loaded so now's the time to register the atexit handler
        if( ret.second )
        {
          atexit( UnloadHandler );
        }
      }

      XrdSysRWLock          lock;
      bool                  unloaded;
      std::set<std::string> protocols;
  };

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
  //! Selects less loaded stream for read operation over multiple streams
  //----------------------------------------------------------------------------
  struct StreamSelector
  {
      StreamSelector( uint16_t size )
      {
        //----------------------------------------------------------------------
        // Subtract one because we shouldn't take into account the control
        // stream.
        //----------------------------------------------------------------------
        strmqueues.resize( size - 1, 0 );
      }

      //------------------------------------------------------------------------
      // @param size : number of streams
      //------------------------------------------------------------------------
      void AdjustQueues( uint16_t size )
      {
         strmqueues.resize( size - 1, 0);
      }

      //------------------------------------------------------------------------
      // @param connected : bitarray stating if given sub-stream is connected
      //
      // @return          : substream number
      //------------------------------------------------------------------------
      uint16_t Select( const std::vector<bool> &connected )
      {
        uint16_t ret    = 0;
        size_t   minval = std::numeric_limits<size_t>::max();

        for( uint16_t i = 0; i < connected.size() && i < strmqueues.size(); ++i )
        {
          if( !connected[i] ) continue;

          if( strmqueues[i] < minval )
          {
            ret = i;
            minval = strmqueues[i];
          }
        }

        ++strmqueues[ret];
        return ret + 1;
      }

      //--------------------------------------------------------------------------
      // Update queue for given substream
      //--------------------------------------------------------------------------
      void MsgReceived( uint16_t substrm )
      {
        if( substrm > 0 )
        --strmqueues[substrm - 1];
      }

    private:

      std::vector<size_t> strmqueues;
  };

  struct BindPrefSelector
  {
    BindPrefSelector( std::vector<std::string> && bindprefs ) :
      bindprefs( std::move( bindprefs ) ), next( 0 )
    {
    }

    inline const std::string& Get()
    {
      std::string &ret = bindprefs[next];
      ++next;
      if( next >= bindprefs.size() )
        next = 0;
      return ret;
    }

    private:
      std::vector<std::string> bindprefs;
      size_t                   next;
  };

  //----------------------------------------------------------------------------
  //! Information holder for xrootd channels
  //----------------------------------------------------------------------------
  struct XRootDChannelInfo
  {
    //--------------------------------------------------------------------------
    // Constructor
    //--------------------------------------------------------------------------
    XRootDChannelInfo( const URL &url ):
      serverFlags(0),
      protocolVersion(0),
      firstLogIn(true),
      authBuffer(0),
      authProtocol(0),
      authParams(0),
      authEnv(0),
      finstcnt(0),
      openFiles(0),
      waitBarrier(0),
      protection(0),
      protRespBody(0),
      protRespSize(0),
      encrypted(false),
      istpc(false)
    {
      sidManager = SIDMgrPool::Instance().GetSIDMgr( url.GetChannelId() );
      memset( sessionId, 0, 16 );
      memset( oldSessionId, 0, 16 );
    }

    //--------------------------------------------------------------------------
    // Destructor
    //--------------------------------------------------------------------------
    ~XRootDChannelInfo()
    {
      delete [] authBuffer;
    }

    typedef std::vector<XRootDStreamInfo> StreamInfoVector;

    //--------------------------------------------------------------------------
    // Data
    //--------------------------------------------------------------------------
    uint32_t                           serverFlags;
    uint32_t                           protocolVersion;
    uint8_t                            sessionId[16];
    uint8_t                            oldSessionId[16];
    bool                               firstLogIn;
    std::shared_ptr<SIDManager>        sidManager;
    char                              *authBuffer;
    XrdSecProtocol                    *authProtocol;
    XrdSecParameters                  *authParams;
    XrdOucEnv                         *authEnv;
    StreamInfoVector                   stream;
    std::string                        streamName;
    std::string                        authProtocolName;
    std::set<uint16_t>                 sentOpens;
    std::set<uint16_t>                 sentCloses;
    std::atomic<uint32_t>              finstcnt; // file instance count
    uint32_t                           openFiles;
    time_t                             waitBarrier;
    XrdSecProtect                     *protection;
    ServerResponseBody_Protocol       *protRespBody;
    unsigned int                       protRespSize;
    std::unique_ptr<StreamSelector>    strmSelector;
    bool                               encrypted;
    bool                               istpc;
    std::unique_ptr<BindPrefSelector>  bindSelector;
    std::string                        logintoken;
    XrdSysMutex                        mutex;
  };

  //----------------------------------------------------------------------------
  // Constructor
  //----------------------------------------------------------------------------
  XRootDTransport::XRootDTransport():
    pSecUnloadHandler( new PluginUnloadHandler() )
  {
  }

  //----------------------------------------------------------------------------
  // Destructor
  //----------------------------------------------------------------------------
  XRootDTransport::~XRootDTransport()
  {
    delete pSecUnloadHandler; pSecUnloadHandler = 0;
  }

  //----------------------------------------------------------------------------
  // Read message header from socket
  //----------------------------------------------------------------------------
  XRootDStatus XRootDTransport::GetHeader( Message &message, Socket *socket )
  {
    //--------------------------------------------------------------------------
    // A new message - allocate the space needed for the header
    //--------------------------------------------------------------------------
    if( message.GetCursor() == 0 && message.GetSize() < 8 )
      message.Allocate( 8 );

    //--------------------------------------------------------------------------
    // Read the message header
    //--------------------------------------------------------------------------
    if( message.GetCursor() < 8 )
    {
      size_t leftToBeRead = 8 - message.GetCursor();
      while( leftToBeRead )
      {
        int bytesRead = 0;
        XRootDStatus status = socket->Read( message.GetBufferAtCursor(),
                                            leftToBeRead, bytesRead );
        if( !status.IsOK() || status.code == suRetry )
          return status;

        leftToBeRead -= bytesRead;
        message.AdvanceCursor( bytesRead );
      }
      UnMarshallHeader( message );

      uint32_t bodySize = *(uint32_t*)(message.GetBuffer(4));
      Log *log = DefaultEnv::GetLog();
      log->Dump( XRootDTransportMsg, "[msg: 0x%x] Expecting %d bytes of message "
                 "body", &message, bodySize );

      return XRootDStatus( stOK, suDone );
    }
    return XRootDStatus( stError, errInternal );
  }

  //----------------------------------------------------------------------------
  // Read message body from socket
  //----------------------------------------------------------------------------
  XRootDStatus XRootDTransport::GetBody( Message &message, Socket *socket )
  {
    //--------------------------------------------------------------------------
    // Retrieve the body
    //--------------------------------------------------------------------------
    size_t   leftToBeRead = 0;
    uint32_t bodySize = 0;
    ServerResponseHeader* rsphdr = (ServerResponseHeader*)message.GetBuffer();
    bodySize = rsphdr->dlen;

    if( message.GetSize() < bodySize + 8 )
      message.ReAllocate( bodySize + 8 );

    leftToBeRead = bodySize-(message.GetCursor()-8);
    while( leftToBeRead )
    {
      int bytesRead = 0;
      XRootDStatus status = socket->Read( message.GetBufferAtCursor(), leftToBeRead, bytesRead );

      if( !status.IsOK() || status.code == suRetry )
        return status;

      leftToBeRead -= bytesRead;
      message.AdvanceCursor( bytesRead );
    }

    return XRootDStatus( stOK, suDone );
  }

  //----------------------------------------------------------------------------
  // Read more of the message body from socket
  //----------------------------------------------------------------------------
  XRootDStatus XRootDTransport::GetMore( Message &message, Socket *socket )
  {
    ServerResponseHeader* rsphdr = (ServerResponseHeader*)message.GetBuffer();
    if( rsphdr->status != kXR_status )
      return XRootDStatus( stError, errInvalidOp );

    //--------------------------------------------------------------------------
    // In case of non kXR_status responses we read all the response, including
    // data. For kXR_status responses we first read only the remainder of the
    // header. The header must then be unmarshalled, and then a second call to
    // GetMore (repeated for suRetry as needed) will read the data.
    //--------------------------------------------------------------------------

    uint32_t bodySize = rsphdr->dlen;
    if( bodySize+8 < sizeof( ServerResponseStatus ) )
      return XRootDStatus( stError, errInvalidMessage, 0,
                          "kXR_status: invalid message size." );

    ServerResponseStatus *rspst = (ServerResponseStatus*)message.GetBuffer();
    bodySize += rspst->bdy.dlen;

    if( message.GetSize() < bodySize + 8 )
      message.ReAllocate( bodySize + 8 );

    size_t leftToBeRead = bodySize-(message.GetCursor()-8);
    while( leftToBeRead )
    {
      int bytesRead = 0;
      XRootDStatus status = socket->Read( message.GetBufferAtCursor(), leftToBeRead, bytesRead );

      if( !status.IsOK() || status.code == suRetry )
        return status;

      leftToBeRead -= bytesRead;
      message.AdvanceCursor( bytesRead );
    }

    // Unmarchal to message body
    Log *log = DefaultEnv::GetLog();
    XRootDStatus st = XRootDTransport::UnMarchalStatusMore( message );
    if( !st.IsOK() && st.code == errDataError )
    {
      log->Error( XRootDTransportMsg, "[msg: 0x%x] %s", &message,
                  st.GetErrorMessage().c_str() );
      return st;
    }

    if( !st.IsOK() )
    {
      log->Error( XRootDTransportMsg, "[msg: 0x%x] Failed to unmarshall status body.",
                  &message );
      return st;
    }

    return XRootDStatus( stOK, suDone );
  }

  //----------------------------------------------------------------------------
  // Initialize channel
  //----------------------------------------------------------------------------
  void XRootDTransport::InitializeChannel( const URL  &url,
                                           AnyObject  &channelData )
  {
    XRootDChannelInfo *info = new XRootDChannelInfo( url );
    XrdSysMutexHelper scopedLock( info->mutex );
    channelData.Set( info );

    Env *env = DefaultEnv::GetEnv();
    int streams = DefaultSubStreamsPerChannel;
    env->GetInt( "SubStreamsPerChannel", streams );
    if( streams < 1 ) streams = 1;
    info->stream.resize( streams );
    info->strmSelector.reset( new StreamSelector( streams ) );
    info->encrypted    = url.IsSecure();
    info->istpc        = url.IsTPC();
    info->logintoken   = url.GetLoginToken();
  }

  //----------------------------------------------------------------------------
  // Finalize channel
  //----------------------------------------------------------------------------
  void XRootDTransport::FinalizeChannel( AnyObject & )
  {
  }

  //----------------------------------------------------------------------------
  // HandShake
  //----------------------------------------------------------------------------
  XRootDStatus XRootDTransport::HandShake( HandShakeData *handShakeData,
                                           AnyObject     &channelData )
  {
    XRootDChannelInfo *info = 0;
    channelData.Get( info );
    XrdSysMutexHelper scopedLock( info->mutex );

    if( info->stream.size() <= handShakeData->subStreamId )
    {
      Log *log = DefaultEnv::GetLog();
      log->Error( XRootDTransportMsg,
                  "[%s] Internal error: not enough substreams",
                  handShakeData->streamName.c_str() );
      return XRootDStatus( stFatal, errInternal );
    }

    if( handShakeData->subStreamId == 0 )
    {
      info->streamName = handShakeData->streamName;
      return HandShakeMain( handShakeData, channelData );
    }
    return HandShakeParallel( handShakeData, channelData );
  }

  //----------------------------------------------------------------------------
  // Hand shake the main stream
  //----------------------------------------------------------------------------
  XRootDStatus XRootDTransport::HandShakeMain( HandShakeData *handShakeData,
                                         AnyObject     &channelData )
  {
    XRootDChannelInfo *info = 0;
    channelData.Get( info );
    XRootDStreamInfo &sInfo = info->stream[handShakeData->subStreamId];

    //--------------------------------------------------------------------------
    // First step - we need to create and initial handshake and send it out
    //--------------------------------------------------------------------------
    if( sInfo.status == XRootDStreamInfo::Disconnected ||
        sInfo.status == XRootDStreamInfo::Broken )
    {
      handShakeData->out = GenerateInitialHSProtocol( handShakeData, info,
                                           ClientProtocolRequest::kXR_ExpLogin );
      sInfo.status = XRootDStreamInfo::HandShakeSent;
      return XRootDStatus( stOK, suContinue );
    }

    //--------------------------------------------------------------------------
    // Second step - we got the reply message to the initial handshake
    //--------------------------------------------------------------------------
    if( sInfo.status == XRootDStreamInfo::HandShakeSent )
    {
      XRootDStatus st = ProcessServerHS( handShakeData, info );
      if( st.IsOK() )
        sInfo.status = XRootDStreamInfo::HandShakeReceived;
      else
        sInfo.status = XRootDStreamInfo::Broken;
      return st;
    }

    //--------------------------------------------------------------------------
    // Third step - we got the response to the protocol request, we need
    // to process it and send out a login request
    //--------------------------------------------------------------------------
    if( sInfo.status == XRootDStreamInfo::HandShakeReceived )
    {
      XRootDStatus st = ProcessProtocolResp( handShakeData, info );

      if( !st.IsOK() )
      {
        sInfo.status = XRootDStreamInfo::Broken;
        return st;
      }

      if( st.code == suRetry )
      {
        handShakeData->out = GenerateProtocol( handShakeData, info,
                                          ClientProtocolRequest::kXR_ExpLogin );
        sInfo.status = XRootDStreamInfo::HandShakeReceived;
        return XRootDStatus( stOK, suRetry );
      }

      handShakeData->out = GenerateLogIn( handShakeData, info );
      sInfo.status = XRootDStreamInfo::LoginSent;
      return XRootDStatus( stOK, suContinue );
    }

    //--------------------------------------------------------------------------
    // Fourth step - handle the log in response and proceed with the
    // authentication if required by the server
    //--------------------------------------------------------------------------
    if( sInfo.status == XRootDStreamInfo::LoginSent )
    {
      XRootDStatus st = ProcessLogInResp( handShakeData, info );

      if( !st.IsOK() )
      {
        sInfo.status = XRootDStreamInfo::Broken;
        return st;
      }

      if( st.IsOK() && st.code == suDone )
      {
        //----------------------------------------------------------------------
        // If it's not our first log in we need to end the previous session
        // to make sure that the server noticed our disconnection and closed
        // all the writable handles that we owned
        //----------------------------------------------------------------------
        if( !info->firstLogIn )
        {
          handShakeData->out = GenerateEndSession( handShakeData, info );
          sInfo.status = XRootDStreamInfo::EndSessionSent;
          return XRootDStatus( stOK, suContinue );
        }

        sInfo.status = XRootDStreamInfo::Connected;
        info->firstLogIn = false;
        return st;
      }

      st = DoAuthentication( handShakeData, info );
      if( !st.IsOK() )
        sInfo.status = XRootDStreamInfo::Broken;
      else
        sInfo.status = XRootDStreamInfo::AuthSent;
      return st;
    }

    //--------------------------------------------------------------------------
    // Fifth step and later - proceed with the authentication
    //--------------------------------------------------------------------------
    if( sInfo.status == XRootDStreamInfo::AuthSent )
    {
      XRootDStatus st = DoAuthentication( handShakeData, info );

      if( !st.IsOK() )
      {
        sInfo.status = XRootDStreamInfo::Broken;
        return st;
      }

      if( st.IsOK() && st.code == suDone )
      {
        //----------------------------------------------------------------------
        // If it's not our first log in we need to end the previous session
        //----------------------------------------------------------------------
        if( !info->firstLogIn )
        {
          handShakeData->out = GenerateEndSession( handShakeData, info );
          sInfo.status = XRootDStreamInfo::EndSessionSent;
          return XRootDStatus( stOK, suContinue );
        }

        sInfo.status = XRootDStreamInfo::Connected;
        info->firstLogIn = false;
        return st;
      }

      return st;
    }

    //--------------------------------------------------------------------------
    // The last step - kXR_endsess returned
    //--------------------------------------------------------------------------
    if( sInfo.status == XRootDStreamInfo::EndSessionSent )
    {
      XRootDStatus st = ProcessEndSessionResp( handShakeData, info );

      if( st.IsOK() && st.code == suDone )
      {
        sInfo.status = XRootDStreamInfo::Connected;
      }
      else if( !st.IsOK() )
      {
        sInfo.status = XRootDStreamInfo::Broken;
      }

      return st;
    }

    return XRootDStatus( stOK, suDone );
  }

  //----------------------------------------------------------------------------
  // Hand shake parallel stream
  //----------------------------------------------------------------------------
  XRootDStatus XRootDTransport::HandShakeParallel( HandShakeData *handShakeData,
                                                   AnyObject     &channelData )
  {
    XRootDChannelInfo *info = 0;
    channelData.Get( info );

    XRootDStreamInfo &sInfo = info->stream[handShakeData->subStreamId];

    //--------------------------------------------------------------------------
    // First step - we need to create and initial handshake and send it out
    //--------------------------------------------------------------------------
    if( sInfo.status == XRootDStreamInfo::Disconnected ||
        sInfo.status == XRootDStreamInfo::Broken )
    {
      handShakeData->out = GenerateInitialHSProtocol( handShakeData, info,
                                            ClientProtocolRequest::kXR_ExpBind );
      sInfo.status = XRootDStreamInfo::HandShakeSent;
      return XRootDStatus( stOK, suContinue );
    }

    //--------------------------------------------------------------------------
    // Second step - we got the reply message to the initial handshake,
    // if successful we need to send bind
    //--------------------------------------------------------------------------
    if( sInfo.status == XRootDStreamInfo::HandShakeSent )
    {
      XRootDStatus st = ProcessServerHS( handShakeData, info );
      if( st.IsOK() )
        sInfo.status = XRootDStreamInfo::HandShakeReceived;
      else
        sInfo.status = XRootDStreamInfo::Broken;
      return st;
    }

    //--------------------------------------------------------------------------
    // Second step bis - we got the response to the protocol request, we need
    // to process it and send out a bind request
    //--------------------------------------------------------------------------
    if( sInfo.status == XRootDStreamInfo::HandShakeReceived )
    {
      XRootDStatus st = ProcessProtocolResp( handShakeData, info );

      if( !st.IsOK() )
      {
        sInfo.status = XRootDStreamInfo::Broken;
        return st;
      }

      handShakeData->out = GenerateBind( handShakeData, info );
      sInfo.status = XRootDStreamInfo::BindSent;
      return XRootDStatus( stOK, suContinue );
    }

    //--------------------------------------------------------------------------
    // Third step - we got the response to the kXR_bind
    //--------------------------------------------------------------------------
    if( sInfo.status == XRootDStreamInfo::BindSent )
    {
      XRootDStatus st = ProcessBindResp( handShakeData, info );

      if( !st.IsOK() )
      {
        sInfo.status = XRootDStreamInfo::Broken;
        return st;
      }
      sInfo.status = XRootDStreamInfo::Connected;
      return XRootDStatus();
    }
    return XRootDStatus();
  }

  //------------------------------------------------------------------------
  // @return true if handshake has been done and stream is connected,
  //         false otherwise
  //------------------------------------------------------------------------
  bool XRootDTransport::HandShakeDone( HandShakeData *handShakeData,
                                       AnyObject     &channelData )
  {
    XRootDChannelInfo *info = 0;
    channelData.Get( info );
    XRootDStreamInfo &sInfo = info->stream[handShakeData->subStreamId];
    return ( sInfo.status == XRootDStreamInfo::Connected );
  }

  //----------------------------------------------------------------------------
  // Check if the stream should be disconnected
  //----------------------------------------------------------------------------
  bool XRootDTransport::IsStreamTTLElapsed( time_t     inactiveTime,
                                            AnyObject &channelData )
  {
    XRootDChannelInfo *info = 0;
    channelData.Get( info );
    Env *env = DefaultEnv::GetEnv();
    Log *log = DefaultEnv::GetLog();

    //--------------------------------------------------------------------------
    // Check the TTL settings for the current server
    //--------------------------------------------------------------------------
    int ttl;
    if( info->serverFlags & kXR_isServer )
    {
      ttl = DefaultDataServerTTL;
      env->GetInt( "DataServerTTL", ttl );
    }
    else
    {
      ttl = DefaultLoadBalancerTTL;
      env->GetInt( "LoadBalancerTTL", ttl );
    }

    //--------------------------------------------------------------------------
    // See whether we can give a go-ahead for the disconnection
    //--------------------------------------------------------------------------
    XrdSysMutexHelper scopedLock( info->mutex );
    uint16_t allocatedSIDs = info->sidManager->GetNumberOfAllocatedSIDs();
    log->Dump( XRootDTransportMsg, "[%s] Stream inactive since %d seconds, "
               "TTL: %d, allocated SIDs: %d, open files: %d, bound file objects: %d",
               info->streamName.c_str(), inactiveTime, ttl, allocatedSIDs,
               info->openFiles, info->finstcnt.load( std::memory_order_relaxed ) );

    if( info->openFiles != 0 && info->finstcnt.load( std::memory_order_relaxed ) != 0 )
      return false;

    if( !allocatedSIDs && inactiveTime > ttl )
      return true;

    return false;
  }

  //----------------------------------------------------------------------------
  // Check the stream is broken - ie. TCP connection got broken and
  // went undetected by the TCP stack
  //----------------------------------------------------------------------------
  Status XRootDTransport::IsStreamBroken( time_t     inactiveTime,
                                          AnyObject &channelData )
  {
    XRootDChannelInfo *info = 0;
    channelData.Get( info );
    Env *env = DefaultEnv::GetEnv();
    Log *log = DefaultEnv::GetLog();

    int streamTimeout = DefaultStreamTimeout;
    env->GetInt( "StreamTimeout", streamTimeout );

    XrdSysMutexHelper scopedLock( info->mutex );

    const time_t now  = time(0);
    const bool anySID =
      info->sidManager->IsAnySIDOldAs( now - streamTimeout );

    log->Dump( XRootDTransportMsg, "[%s] Stream inactive since %d seconds, "
               "stream timeout: %d, any SID: %d, wait barrier: %s",
               info->streamName.c_str(), inactiveTime, streamTimeout,
               anySID, Utils::TimeToString(info->waitBarrier).c_str() );

    if( inactiveTime < streamTimeout )
      return Status();

    if( now < info->waitBarrier )
      return Status();

    if( !anySID )
      return Status();

    return Status( stError, errSocketTimeout );
  }

  //----------------------------------------------------------------------------
  // Multiplex
  //----------------------------------------------------------------------------
  PathID XRootDTransport::Multiplex( Message *, AnyObject &, PathID * )
  {
    return PathID( 0, 0 );
  }

  //----------------------------------------------------------------------------
  // Multiplex
  //----------------------------------------------------------------------------
  PathID XRootDTransport::MultiplexSubStream( Message   *msg,
                                              AnyObject &channelData,
                                              PathID    *hint )
  {
    XRootDChannelInfo *info = 0;
    channelData.Get( info );
    XrdSysMutexHelper scopedLock( info->mutex );

    //--------------------------------------------------------------------------
    // If we're not connected to a data server or we don't know that yet
    // we stream through 0
    //--------------------------------------------------------------------------
    if( !(info->serverFlags & kXR_isServer) || info->stream.size() == 0 )
      return PathID( 0, 0 );

    //--------------------------------------------------------------------------
    // Select the streams
    //--------------------------------------------------------------------------
    Log *log = DefaultEnv::GetLog();
    uint16_t upStream   = 0;
    uint16_t downStream = 0;

    if( hint )
    {
      upStream   = hint->up;
      downStream = hint->down;
    }
    else
    {
      upStream = 0;
      std::vector<bool> connected;
      connected.reserve( info->stream.size() - 1 );
      size_t nbConnected = 0;
      for( size_t i = 1; i < info->stream.size(); ++i )
        if( info->stream[i].status == XRootDStreamInfo::Connected )
        {
          connected.push_back( true );
          ++nbConnected;
        }
        else
          connected.push_back( false );

      if( nbConnected == 0 )
        downStream = 0;
      else
        downStream = info->strmSelector->Select( connected );
    }

    if( upStream >= info->stream.size() )
    {
      log->Debug( XRootDTransportMsg,
                  "[%s] Up link stream %d does not exist, using 0",
                  info->streamName.c_str(), upStream );
      upStream = 0;
    }

    if( downStream >= info->stream.size() )
    {
      log->Debug( XRootDTransportMsg,
                  "[%s] Down link stream %d does not exist, using 0",
                  info->streamName.c_str(), downStream );
      downStream = 0;
    }

    //--------------------------------------------------------------------------
    // Modify the message
    //--------------------------------------------------------------------------
    UnMarshallRequest( msg );
    ClientRequestHdr *hdr = (ClientRequestHdr*)msg->GetBuffer();
    switch( hdr->requestid )
    {
      //------------------------------------------------------------------------
      // Read - we update the path id to tell the server where we want to
      // get the response, but we still send the request through stream 0
      // We need to allocate space for read_args if we don't have it
      // included yet
      //------------------------------------------------------------------------
      case kXR_read:
      {
        if( msg->GetSize() < sizeof(ClientReadRequest) + 8 )
        {
          msg->ReAllocate( sizeof(ClientReadRequest) + 8 );
          void *newBuf = msg->GetBuffer(sizeof(ClientReadRequest));
          memset( newBuf, 0, 8 );
          ClientReadRequest *req = (ClientReadRequest*)msg->GetBuffer();
          req->dlen += 8;
        }
        read_args *args = (read_args*)msg->GetBuffer(sizeof(ClientReadRequest));
        args->pathid = info->stream[downStream].pathId;
        break;
      }


      //------------------------------------------------------------------------
      // PgRead - we update the path id to tell the server where we want to
      // get the response, but we still send the request through stream 0
      // We need to allocate space for ClientPgReadReqArgs if we don't have it
      // included yet
      //------------------------------------------------------------------------
      case kXR_pgread:
      {
        if( msg->GetSize() < sizeof( ClientPgReadRequest ) + sizeof( ClientPgReadReqArgs ) )
        {
          msg->ReAllocate( sizeof( ClientPgReadRequest ) + sizeof( ClientPgReadReqArgs ) );
          void *newBuf = msg->GetBuffer( sizeof( ClientPgReadRequest ) );
          memset( newBuf, 0, sizeof( ClientPgReadReqArgs ) );
          ClientPgReadRequest *req = (ClientPgReadRequest*)msg->GetBuffer();
          req->dlen += sizeof( ClientPgReadReqArgs );
        }
        ClientPgReadReqArgs *args = reinterpret_cast<ClientPgReadReqArgs*>(
            msg->GetBuffer( sizeof( ClientPgReadRequest ) ) );
        args->pathid   = info->stream[downStream].pathId;
        break;
      }

      //------------------------------------------------------------------------
      // ReadV - the situation is identical to read but we don't need any
      // additional structures to specify the return path
      //------------------------------------------------------------------------
      case kXR_readv:
      {
        ClientReadVRequest *req = (ClientReadVRequest*)msg->GetBuffer();
        req->pathid = info->stream[downStream].pathId;
        break;
      }

      //------------------------------------------------------------------------
      // Write - multiplexing writes doesn't work properly in the server
      //------------------------------------------------------------------------
      case kXR_write:
      {
//        ClientWriteRequest *req = (ClientWriteRequest*)msg->GetBuffer();
//        req->pathid = info->stream[downStream].pathId;
        break;
      }

      //------------------------------------------------------------------------
      // WriteV - multiplexing writes doesn't work properly in the server
      //------------------------------------------------------------------------
      case kXR_writev:
      {
//        ClientWriteVRequest *req = (ClientWriteVRequest*)msg->GetBuffer();
//        req->pathid = info->stream[downStream].pathId;
        break;
      }

      //------------------------------------------------------------------------
      // PgWrite - multiplexing writes doesn't work properly in the server
      //------------------------------------------------------------------------
      case kXR_pgwrite:
      {
//        ClientWriteVRequest *req = (ClientWriteVRequest*)msg->GetBuffer();
//        req->pathid = info->stream[downStream].pathId;
        break;
      }
    };
    MarshallRequest( msg );
    return PathID( upStream, downStream );
  }

  //----------------------------------------------------------------------------
  // Return a number of substreams per stream that should be created
  // This depends on the environment and whether we are connected to
  // a data server or not
  //----------------------------------------------------------------------------
  uint16_t XRootDTransport::SubStreamNumber( AnyObject &channelData )
  {
    XRootDChannelInfo *info = 0;
    channelData.Get( info );
    XrdSysMutexHelper scopedLock( info->mutex );

    //--------------------------------------------------------------------------
    // If the connection has been opened in order to orchestrate a TPC or
    // the remote server is a Manager or Metamanager we will need only one
    // (control) stream.
    //--------------------------------------------------------------------------
    if( info->istpc || !(info->serverFlags & kXR_isServer ) ) return 1;

    //--------------------------------------------------------------------------
    // Number of streams requested by user
    //--------------------------------------------------------------------------
    uint16_t ret = info->stream.size();

    XrdCl::Env *env = XrdCl::DefaultEnv::GetEnv();
    int nodata = DefaultTlsNoData;
    env->GetInt( "TlsNoData", nodata );

    // Does the server require the stream 0 to be encrypted?
    bool srvTlsStrm0 = ( info->serverFlags & kXR_gotoTLS )  ||
                       ( info->serverFlags & kXR_tlsLogin ) ||
                       ( info->serverFlags & kXR_tlsSess );
    // Does the server NOT require the data streams to be encrypted?
    bool srvNoTlsData = !( info->serverFlags & kXR_tlsData );
    // Does the user require the stream 0 to be encrypted?
    bool usrTlsStrm0 = info->encrypted;
    // Does the user NOT require the data streams to be encrypted?
    bool usrNoTlsData = !info->encrypted || ( info->encrypted && nodata );

    if( ( usrTlsStrm0 && usrNoTlsData && srvNoTlsData ) ||
        ( srvTlsStrm0 && srvNoTlsData && usrNoTlsData ) )
    {
      //------------------------------------------------------------------------
      // The server or user asked us to encrypt stream 0, but to send the data
      // (read/write) using a plain TCP connection
      //------------------------------------------------------------------------
      if( ret == 1 ) ++ret;
    }

    if( ret > info->stream.size() )
    {
      info->stream.resize( ret );
      info->strmSelector->AdjustQueues( ret );
    }

    return ret;
  }

  //----------------------------------------------------------------------------
  // Marshall
  //----------------------------------------------------------------------------
  XRootDStatus XRootDTransport::MarshallRequest( char *msg )
  {
    ClientRequest *req = (ClientRequest*)msg;
    switch( req->header.requestid )
    {
      //------------------------------------------------------------------------
      // kXR_protocol
      //------------------------------------------------------------------------
      case kXR_protocol:
        req->protocol.clientpv = htonl( req->protocol.clientpv );
        break;

      //------------------------------------------------------------------------
      // kXR_login
      //------------------------------------------------------------------------
      case kXR_login:
        req->login.pid = htonl( req->login.pid );
        break;

      //------------------------------------------------------------------------
      // kXR_locate
      //------------------------------------------------------------------------
      case kXR_locate:
        req->locate.options = htons( req->locate.options );
        break;

      //------------------------------------------------------------------------
      // kXR_query
      //------------------------------------------------------------------------
      case kXR_query:
        req->query.infotype = htons( req->query.infotype );
        break;

      //------------------------------------------------------------------------
      // kXR_truncate
      //------------------------------------------------------------------------
      case kXR_truncate:
        req->truncate.offset = htonll( req->truncate.offset );
        break;

      //------------------------------------------------------------------------
      // kXR_mkdir
      //------------------------------------------------------------------------
      case kXR_mkdir:
        req->mkdir.mode = htons( req->mkdir.mode );
        break;

      //------------------------------------------------------------------------
      // kXR_chmod
      //------------------------------------------------------------------------
      case kXR_chmod:
        req->chmod.mode = htons( req->chmod.mode );
        break;

      //------------------------------------------------------------------------
      // kXR_open
      //------------------------------------------------------------------------
      case kXR_open:
        req->open.mode    = htons( req->open.mode );
        req->open.options = htons( req->open.options );
        break;

      //------------------------------------------------------------------------
      // kXR_read
      //------------------------------------------------------------------------
      case kXR_read:
        req->read.offset = htonll( req->read.offset );
        req->read.rlen   = htonl( req->read.rlen );
        break;

      //------------------------------------------------------------------------
      // kXR_write
      //------------------------------------------------------------------------
      case kXR_write:
        req->write.offset = htonll( req->write.offset );
        break;

      //------------------------------------------------------------------------
      // kXR_mv
      //------------------------------------------------------------------------
      case kXR_mv:
        req->mv.arg1len = htons( req->mv.arg1len );
        break;

      //------------------------------------------------------------------------
      // kXR_readv
      //------------------------------------------------------------------------
      case kXR_readv:
      {
        uint16_t numChunks  = (req->readv.dlen)/16;
        readahead_list *dataChunk = (readahead_list*)( msg + 24 );
        for( size_t i = 0; i < numChunks; ++i )
        {
          dataChunk[i].rlen   = htonl( dataChunk[i].rlen );
          dataChunk[i].offset = htonll( dataChunk[i].offset );
        }
        break;
      }

      //------------------------------------------------------------------------
      // kXR_writev
      //------------------------------------------------------------------------
      case kXR_writev:
      {
        uint16_t numChunks  = (req->writev.dlen)/16;
        XrdProto::write_list *wrtList =
            reinterpret_cast<XrdProto::write_list*>( msg + 24 );
        for( size_t i = 0; i < numChunks; ++i )
        {
          wrtList[i].wlen   = htonl( wrtList[i].wlen );
          wrtList[i].offset = htonll( wrtList[i].offset );
        }

        break;
      }

      case kXR_pgread:
      {
        req->pgread.offset = htonll( req->pgread.offset );
        req->pgread.rlen   = htonl( req->pgread.rlen );
        break;
      }

      case kXR_pgwrite:
      {
        req->pgwrite.offset = htonll( req->pgwrite.offset );
        break;
      }

      //------------------------------------------------------------------------
      // kXR_prepare
      //------------------------------------------------------------------------
      case kXR_prepare:
      {
        req->prepare.optionX = htons( req->prepare.optionX );
        req->prepare.port    = htons( req->prepare.port );
        break;
      }

      case kXR_chkpoint:
      {
        if( req->chkpoint.opcode == kXR_ckpXeq )
          MarshallRequest( msg + 24 );
        break;
      }
    };

    req->header.requestid = htons( req->header.requestid );
    req->header.dlen      = htonl( req->header.dlen );
    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Unmarshall the request - sometimes the requests need to be rewritten,
  // so we need to unmarshall them
  //----------------------------------------------------------------------------
  XRootDStatus XRootDTransport::UnMarshallRequest( Message *msg )
  {
    if( !msg->IsMarshalled() ) return XRootDStatus( stOK, suAlreadyDone );
    // We rely on the marshaling process to be symmetric!
    // First we unmarshall the request ID and the length because
    // MarshallRequest() relies on these, and then we need to unmarshall these
    // two again, because they get marshalled in MarshallRequest().
    // All this is pretty damn ugly and should be rewritten.
    ClientRequest *req = (ClientRequest*)msg->GetBuffer();
    req->header.requestid = htons( req->header.requestid );
    req->header.dlen      = htonl( req->header.dlen );
    XRootDStatus st = MarshallRequest( msg );
    req->header.requestid = htons( req->header.requestid );
    req->header.dlen      = htonl( req->header.dlen );
    msg->SetIsMarshalled( false );
    return st;
  }

  //----------------------------------------------------------------------------
  // Unmarshall the body of the incoming message
  //----------------------------------------------------------------------------
  XRootDStatus XRootDTransport::UnMarshallBody( Message *msg, uint16_t reqType )
  {
    ServerResponse *m = (ServerResponse *)msg->GetBuffer();

    //--------------------------------------------------------------------------
    // kXR_ok
    //--------------------------------------------------------------------------
    if( m->hdr.status == kXR_ok )
    {
      switch( reqType )
      {
        //----------------------------------------------------------------------
        // kXR_protocol
        //----------------------------------------------------------------------
        case kXR_protocol:
          if( m->hdr.dlen < 8 )
            return XRootDStatus( stError, errInvalidMessage, 0, "kXR_protocol: body too short." );
          m->body.protocol.pval  = ntohl( m->body.protocol.pval );
          m->body.protocol.flags = ntohl( m->body.protocol.flags );
          break;
      }
    }
    //--------------------------------------------------------------------------
    // kXR_error
    //--------------------------------------------------------------------------
    else if( m->hdr.status == kXR_error )
    {
      if( m->hdr.dlen < 4 )
        return XRootDStatus( stError, errInvalidMessage, 0, "kXR_error: body too short." );
      m->body.error.errnum = ntohl( m->body.error.errnum );
    }

    //--------------------------------------------------------------------------
    // kXR_wait
    //--------------------------------------------------------------------------
    else if( m->hdr.status == kXR_wait )
    {
      if( m->hdr.dlen < 4 )
        return XRootDStatus( stError, errInvalidMessage, 0, "kXR_wait: body too short." );
      m->body.wait.seconds = htonl( m->body.wait.seconds );
    }

    //--------------------------------------------------------------------------
    // kXR_redirect
    //--------------------------------------------------------------------------
    else if( m->hdr.status == kXR_redirect )
    {
      if( m->hdr.dlen < 4 )
        return XRootDStatus( stError, errInvalidMessage, 0, "kXR_redirect: body too short." );
      m->body.redirect.port = htonl( m->body.redirect.port );
    }

    //--------------------------------------------------------------------------
    // kXR_waitresp
    //--------------------------------------------------------------------------
    else if( m->hdr.status == kXR_waitresp )
    {
      if( m->hdr.dlen < 4 )
        return XRootDStatus( stError, errInvalidMessage, 0, "kXR_waitresp: body too short." );
      m->body.waitresp.seconds = htonl( m->body.waitresp.seconds );
    }

    //--------------------------------------------------------------------------
    // kXR_attn
    //--------------------------------------------------------------------------
    else if( m->hdr.status == kXR_attn )
    {
      if( m->hdr.dlen < 4 )
        return XRootDStatus( stError, errInvalidMessage, 0, "kXR_attn: body too short." );
      m->body.attn.actnum = htonl( m->body.attn.actnum );
    }

    return XRootDStatus();
  }

  //------------------------------------------------------------------------
  //! Unmarshall the body of the status response
  //------------------------------------------------------------------------
  XRootDStatus XRootDTransport::UnMarshalStatusBody( Message &msg, uint16_t reqType )
  {
    //--------------------------------------------------------------------------
    // Calculate the crc32c before the unmarshaling the body!
    //--------------------------------------------------------------------------
    ServerResponseStatus *rspst   = (ServerResponseStatus*)msg.GetBuffer();
    char   *buffer = msg.GetBuffer( 8 + sizeof( rspst->bdy.crc32c ) );
    size_t  length = rspst->hdr.dlen - sizeof( rspst->bdy.crc32c );
    uint32_t crcval = XrdOucCRC::Calc32C( buffer, length );

    size_t stlen = sizeof( ServerResponseStatus );
    switch( reqType )
    {
      case kXR_pgread:
      {
        stlen += sizeof( ServerResponseBody_pgRead );
        break;
      }

      case kXR_pgwrite:
      {
        stlen += sizeof( ServerResponseBody_pgWrite );
        break;
      }
    }

    if( msg.GetSize() < stlen ) return XRootDStatus( stError, errInvalidMessage, 0,
                                                      "kXR_status: invalid message size." );

    rspst->bdy.crc32c = ntohl( rspst->bdy.crc32c );
    rspst->bdy.dlen   = ntohl( rspst->bdy.dlen );

    switch( reqType )
    {
      case kXR_pgread:
      {
        ServerResponseBody_pgRead *pgrdbdy = (ServerResponseBody_pgRead*)msg.GetBuffer( sizeof( ServerResponseStatus ) );
        pgrdbdy->offset = ntohll( pgrdbdy->offset );
        break;
      }

      case kXR_pgwrite:
      {
        ServerResponseBody_pgWrite *pgwrtbdy = (ServerResponseBody_pgWrite*)msg.GetBuffer( sizeof( ServerResponseStatus ) );
        pgwrtbdy->offset = ntohll( pgwrtbdy->offset );
        break;
      }
    }

    //--------------------------------------------------------------------------
    // Do the integrity checks
    //--------------------------------------------------------------------------
    if( crcval != rspst->bdy.crc32c )
    {
      return XRootDStatus( stError, errDataError, 0, "kXR_status response header "
                           "corrupted (crc32c integrity check failed)." );
    }

    if( rspst->hdr.streamid[0] != rspst->bdy.streamID[0] ||
        rspst->hdr.streamid[1] != rspst->bdy.streamID[1] )
    {
      return XRootDStatus( stError, errDataError, 0, "response header corrupted "
                  "(stream ID mismatch)." );
    }



    if( rspst->bdy.requestid + kXR_1stRequest != reqType )
    {
      return XRootDStatus( stError, errDataError, 0, "kXR_status response header corrupted "
                  "(request ID mismatch)." );
    }

    return XRootDStatus();
  }

  XRootDStatus XRootDTransport::UnMarchalStatusMore( Message &msg )
  {
    ServerResponseV2 *rsp = (ServerResponseV2*)msg.GetBuffer();
    uint16_t reqType = rsp->status.bdy.requestid + kXR_1stRequest;

    switch( reqType )
    {
      case kXR_pgwrite:
      {
        //--------------------------------------------------------------------------
        // If there's no additional data there's nothing to unmarshal
        //--------------------------------------------------------------------------
        if( rsp->status.bdy.dlen == 0 ) return XRootDStatus();
        //--------------------------------------------------------------------------
        // If there's not enough data to form correction-segment report an error
        //--------------------------------------------------------------------------
        if( size_t( rsp->status.bdy.dlen ) < sizeof( ServerResponseBody_pgWrCSE ) )
          return XRootDStatus( stError, errInvalidMessage, 0,
                               "kXR_status: invalid message size." );

        //--------------------------------------------------------------------------
        // Calculate the crc32c for the additional data
        //--------------------------------------------------------------------------
        ServerResponseBody_pgWrCSE *cse = (ServerResponseBody_pgWrCSE*)msg.GetBuffer( sizeof( ServerResponseV2 ) );
        cse->cseCRC = ntohl( cse->cseCRC );
        size_t length = rsp->status.bdy.dlen - sizeof( uint32_t );
        void*  buffer = msg.GetBuffer( sizeof( ServerResponseV2 ) + sizeof( uint32_t ) );
        uint32_t crcval = XrdOucCRC::Calc32C( buffer, length );

        //--------------------------------------------------------------------------
        // Do the integrity checks
        //--------------------------------------------------------------------------
        if( crcval != cse->cseCRC )
        {
          return XRootDStatus( stError, errDataError, 0, "kXR_status response header "
                               "corrupted (crc32c integrity check failed)." );
        }

        cse->dlFirst = ntohs( cse->dlFirst );
        cse->dlLast  = ntohs( cse->dlLast );

        size_t pgcnt = ( rsp->status.bdy.dlen  - sizeof( ServerResponseBody_pgWrCSE ) ) /
                       sizeof( kXR_int64 );
        kXR_int64 *pgoffs = (kXR_int64*)msg.GetBuffer( sizeof( ServerResponseV2 ) +
                                                        sizeof( ServerResponseBody_pgWrCSE ) );

        for( size_t i = 0; i < pgcnt; ++i )
          pgoffs[i] = ntohll( pgoffs[i] );

        return XRootDStatus();
        break;
      }

      default:
        break;
    }

    return XRootDStatus( stError, errNotSupported );
  }

  //----------------------------------------------------------------------------
  // Unmarshall the header of the incoming message
  //----------------------------------------------------------------------------
  void XRootDTransport::UnMarshallHeader( Message &msg )
  {
    ServerResponseHeader *header = (ServerResponseHeader *)msg.GetBuffer();
    header->status = ntohs( header->status );
    header->dlen   = ntohl( header->dlen );
  }

  //----------------------------------------------------------------------------
  // Log server error response
  //----------------------------------------------------------------------------
  void XRootDTransport::LogErrorResponse( const Message &msg )
  {
    Log *log = DefaultEnv::GetLog();
    ServerResponse *rsp = (ServerResponse *)msg.GetBuffer();
    char *errmsg = new char[rsp->hdr.dlen-3]; errmsg[rsp->hdr.dlen-4] = 0;
    memcpy( errmsg, rsp->body.error.errmsg, rsp->hdr.dlen-4 );
    log->Error( XRootDTransportMsg, "Server responded with an error [%d]: %s",
                                    rsp->body.error.errnum, errmsg );
   delete [] errmsg;
  }

  //------------------------------------------------------------------------
  // Number of currently connected data streams
  //------------------------------------------------------------------------
  uint16_t XRootDTransport::NbConnectedStrm( AnyObject &channelData )
  {
    XRootDChannelInfo *info = 0;
    channelData.Get( info );
    XrdSysMutexHelper scopedLock( info->mutex );

    uint16_t nbConnected = 0;
    for( size_t i = 1; i < info->stream.size(); ++i )
      if( info->stream[i].status == XRootDStreamInfo::Connected )
        ++nbConnected;

    return nbConnected;
  }

  //----------------------------------------------------------------------------
  // The stream has been disconnected, do the cleanups
  //----------------------------------------------------------------------------
  void XRootDTransport::Disconnect( AnyObject &channelData,
                                    uint16_t   subStreamId )
  {
    XRootDChannelInfo *info = 0;
    channelData.Get( info );
    XrdSysMutexHelper scopedLock( info->mutex );

    CleanUpProtection( info );

    if( !info->stream.empty() )
    {
      XRootDStreamInfo &sInfo = info->stream[subStreamId];
      sInfo.status = XRootDStreamInfo::Disconnected;
    }

    if( subStreamId == 0 )
    {
      info->sidManager->ReleaseAllTimedOut();
      info->sentOpens.clear();
      info->sentCloses.clear();
      info->openFiles   = 0;
      info->waitBarrier = 0;
    }
  }

  //------------------------------------------------------------------------
  // Query the channel
  //------------------------------------------------------------------------
  Status XRootDTransport::Query( uint16_t   query,
                                 AnyObject &result,
                                 AnyObject &channelData )
  {
    XRootDChannelInfo *info = 0;
    channelData.Get( info );
    XrdSysMutexHelper scopedLock( info->mutex );

    switch( query )
    {
      //------------------------------------------------------------------------
      // Protocol name
      //------------------------------------------------------------------------
      case TransportQuery::Name:
        result.Set( (const char*)"XRootD", false );
        return Status();

      //------------------------------------------------------------------------
      // Authentication
      //------------------------------------------------------------------------
      case TransportQuery::Auth:
        result.Set( new std::string( info->authProtocolName ), false );
        return Status();

      //------------------------------------------------------------------------
      // Server flags
      //------------------------------------------------------------------------
      case XRootDQuery::ServerFlags:
        result.Set( new int( info->serverFlags ), false );
        return Status();

      //------------------------------------------------------------------------
      // Protocol version
      //------------------------------------------------------------------------
      case XRootDQuery::ProtocolVersion:
        result.Set( new int( info->protocolVersion ), false );
        return Status();

      case XRootDQuery::IsEncrypted:
        result.Set( new bool( info->encrypted ), false );
        return Status();
    };
    return Status( stError, errQueryNotSupported );
  }

  //----------------------------------------------------------------------------
  // Check whether the transport can hijack the message
  //----------------------------------------------------------------------------
  uint32_t XRootDTransport::MessageReceived( Message   &msg,
                                             uint16_t   subStream,
                                             AnyObject &channelData )
  {
    XRootDChannelInfo *info = 0;
    channelData.Get( info );
    XrdSysMutexHelper scopedLock( info->mutex );
    Log *log = DefaultEnv::GetLog();

    //--------------------------------------------------------------------------
    // Update the substream queues
    //--------------------------------------------------------------------------
    info->strmSelector->MsgReceived( subStream );

    //--------------------------------------------------------------------------
    // Check whether this message is a response to a request that has
    // timed out, and if so, drop it
    //--------------------------------------------------------------------------
    ServerResponse *rsp = (ServerResponse*)msg.GetBuffer();
    if( rsp->hdr.status == kXR_attn )
    {
      return NoAction;
    }

    if( info->sidManager->IsTimedOut( rsp->hdr.streamid ) )
    {
      log->Error( XRootDTransportMsg, "Message 0x%x, stream [%d, %d] is a "
                  "response that we're no longer interested in (timed out)",
                  &msg, rsp->hdr.streamid[0], rsp->hdr.streamid[1] );
      //------------------------------------------------------------------------
      // If it is kXR_waitresp there will be another one,
      // so we don't release the sid yet
      //------------------------------------------------------------------------
      if( rsp->hdr.status != kXR_waitresp )
        info->sidManager->ReleaseTimedOut( rsp->hdr.streamid );
      //------------------------------------------------------------------------
      // If it is a successful response to an open request
      // that timed out, we need to send a close
      //------------------------------------------------------------------------
      uint16_t sid; memcpy( &sid, rsp->hdr.streamid, 2 );
      std::set<uint16_t>::iterator sidIt = info->sentOpens.find( sid );
      if( sidIt != info->sentOpens.end() )
      {
        info->sentOpens.erase( sidIt );
        if( rsp->hdr.status == kXR_ok ) return RequestClose;
      }
      return DigestMsg;
    }

    //--------------------------------------------------------------------------
    // If we have a wait or waitresp
    //--------------------------------------------------------------------------
    uint32_t seconds = 0;
    if( rsp->hdr.status == kXR_wait )
      seconds = ntohl( rsp->body.wait.seconds ) + 5; // we need extra time
                                                     // to re-send the request
    else if( rsp->hdr.status == kXR_waitresp )
    {
      seconds = ntohl( rsp->body.waitresp.seconds );

      log->Dump( XRootDMsg, "[%s] Got kXR_waitresp response of %d seconds, "
                 "setting up wait barrier.",
                 info->streamName.c_str(),
                 seconds );
    }

    time_t barrier = time(0) + seconds;
    if( info->waitBarrier < barrier )
      info->waitBarrier = barrier;

    //--------------------------------------------------------------------------
    // If we got a response to an open request, we may need to bump the counter
    // of open files
    //--------------------------------------------------------------------------
    uint16_t sid; memcpy( &sid, rsp->hdr.streamid, 2 );
    std::set<uint16_t>::iterator sidIt = info->sentOpens.find( sid );
    if( sidIt != info->sentOpens.end() )
    {
      if( rsp->hdr.status == kXR_waitresp )
        return NoAction;
      info->sentOpens.erase( sidIt );
      if( rsp->hdr.status == kXR_ok )
      {
        ++info->openFiles;
        info->finstcnt.fetch_add( 1, std::memory_order_relaxed ); // another file File object instance has been bound with this connection
      }
      return NoAction;
    }

    //--------------------------------------------------------------------------
    // If we got a response to a close, we may need to decrement the counter of
    // open files
    //--------------------------------------------------------------------------
    sidIt = info->sentCloses.find( sid );
    if( sidIt != info->sentCloses.end() )
    {
      if( rsp->hdr.status == kXR_waitresp )
        return NoAction;
      info->sentCloses.erase( sidIt );
      --info->openFiles;
      return NoAction;
    }
    return NoAction;
  }

  //----------------------------------------------------------------------------
  // Notify the transport about a message having been sent
  //----------------------------------------------------------------------------
  void XRootDTransport::MessageSent( Message   *msg,
                                     uint16_t   subStream,
                                     uint32_t   bytesSent,
                                     AnyObject &channelData )
  {
    XRootDChannelInfo *info = 0;
    channelData.Get( info );
    XrdSysMutexHelper scopedLock( info->mutex );
    ClientRequest *req = (ClientRequest*)msg->GetBuffer();
    uint16_t reqid = ntohs( req->header.requestid );


    //--------------------------------------------------------------------------
    // We need to track opens to know if we can close streams due to idleness
    //--------------------------------------------------------------------------
    uint16_t sid;
    memcpy( &sid, req->header.streamid, 2 );

    if( reqid == kXR_open )
      info->sentOpens.insert( sid );
    else if( reqid == kXR_close )
      info->sentCloses.insert( sid );
  }


  //----------------------------------------------------------------------------
  // Get signature for given message
  //----------------------------------------------------------------------------
  Status XRootDTransport::GetSignature( Message *toSign, Message *&sign, AnyObject &channelData )
  {
    XRootDChannelInfo *info = 0;
    channelData.Get( info );
    return GetSignature( toSign, sign, info );
  }

  //------------------------------------------------------------------------
  //! Get signature for given message
  //------------------------------------------------------------------------
  Status XRootDTransport::GetSignature( Message           *toSign,
                                        Message           *&sign,
                                        XRootDChannelInfo *info )
  {
    XrdSysRWLockHelper scope( pSecUnloadHandler->lock );
    if( pSecUnloadHandler->unloaded ) return Status( stError, errInvalidOp );

    ClientRequest *thereq  = reinterpret_cast<ClientRequest*>( toSign->GetBuffer() );
    if( !info ) return Status( stError, errInternal );
    if( info->protection )
    {
      SecurityRequest *newreq  = 0;
      // check if we have to secure the request in the first place
      if( !( NEED2SECURE ( info->protection )( *thereq ) ) ) return Status();
      // secure (sign/encrypt) the request
      int rc = info->protection->Secure( newreq, *thereq, 0 );
      // there was an error
      if( rc < 0 )
        return Status( stError, errInternal, -rc );

      sign = new Message();
      sign->Grab( reinterpret_cast<char*>( newreq ), rc );
    }

    return Status();
  }

  //------------------------------------------------------------------------
  //! Decrement file object instance count bound to this channel
  //------------------------------------------------------------------------
  void XRootDTransport::DecFileInstCnt( AnyObject &channelData )
  {
    XRootDChannelInfo *info = 0;
    channelData.Get( info );
    if( info->finstcnt.load( std::memory_order_relaxed ) > 0 )
      info->finstcnt.fetch_sub( 1, std::memory_order_relaxed );
  }

  //----------------------------------------------------------------------------
  // Wait before exit
  //----------------------------------------------------------------------------
  void XRootDTransport::WaitBeforeExit()
  {
    XrdSysRWLockHelper scope( pSecUnloadHandler->lock, false ); // obtain write lock
    pSecUnloadHandler->unloaded = true;
  }

  //----------------------------------------------------------------------------
  // @return : true if encryption should be turned on, false otherwise
  //----------------------------------------------------------------------------
  bool XRootDTransport::NeedEncryption( HandShakeData  *handShakeData,
                                        AnyObject      &channelData )
  {
    XRootDChannelInfo *info = 0;
    channelData.Get( info );

    XrdCl::Env *env = XrdCl::DefaultEnv::GetEnv();
    int notlsok = DefaultNoTlsOK;
    env->GetInt( "NoTlsOK", notlsok );

    if( notlsok )
      return info->encrypted;

    // Did the server instructed us to switch to TLS right away?
    if( info->serverFlags & kXR_gotoTLS )
    {
      info->encrypted = true;
      return true ;
    }

    XRootDStreamInfo &sInfo = info->stream[handShakeData->subStreamId];

    //--------------------------------------------------------------------------
    // The control stream (sub-stream 0)  might need to switch to TLS before
    // login or after login
    //--------------------------------------------------------------------------
    if( handShakeData->subStreamId == 0 )
    {
      //------------------------------------------------------------------------
      // We are about to login and the server asked to start encrypting
      // before login
      //------------------------------------------------------------------------
      if( ( sInfo.status == XRootDStreamInfo::LoginSent ) &&
          ( info->serverFlags & kXR_tlsLogin ) )
      {
        info->encrypted = true;
        return true;
      }

      //--------------------------------------------------------------------
      // The hand-shake is done and the server requested to encrypt the session
      //--------------------------------------------------------------------
      if( (sInfo.status == XRootDStreamInfo::Connected ||
          //--------------------------------------------------------------------
          // we really need to turn on TLS before we sent kXR_endsess and we
          // are about to do so (1st enable encryption, then send kXR_endsess)
          //--------------------------------------------------------------------
           sInfo.status == XRootDStreamInfo::EndSessionSent ) &&
          ( info->serverFlags & kXR_tlsSess ) )
      {
        info->encrypted = true;
        return true;
      }
    }
    //--------------------------------------------------------------------------
    // A data stream (sub-stream > 0) if need be will be switched to TLS before
    // bind.
    //--------------------------------------------------------------------------
    else
    {
      //------------------------------------------------------------------------
      // We are about to bind a data stream and the server asked to start
      // encrypting before bind
      //------------------------------------------------------------------------
      if( ( sInfo.status == XRootDStreamInfo::BindSent ) &&
          ( info->serverFlags & kXR_tlsData ) )
      {
        info->encrypted = true;
        return true;
      }
    }

    return false;
  }

  //------------------------------------------------------------------------
  // Get bind preference for the next data stream
  //------------------------------------------------------------------------
  URL XRootDTransport::GetBindPreference( const URL  &url,
                                          AnyObject  &channelData )
  {
    XRootDChannelInfo *info = 0;
    channelData.Get( info );
    if( !bool( info->bindSelector ) )
      return url;

    return URL( info->bindSelector->Get() );
  }

  //----------------------------------------------------------------------------
  // Generate the message to be sent as an initial handshake
  // (handshake+kXR_protocol)
  //----------------------------------------------------------------------------
  Message *XRootDTransport::GenerateInitialHSProtocol( HandShakeData     *hsData,
                                                       XRootDChannelInfo *info,
                                                       kXR_char           expect )
  {
    Log *log = DefaultEnv::GetLog();
    log->Debug( XRootDTransportMsg,
                "[%s] Sending out the initial hand shake + kXR_protocol",
                hsData->streamName.c_str() );

    Message *msg = new Message();

    msg->Allocate( 20+sizeof(ClientProtocolRequest) );
    msg->Zero();

    ClientInitHandShake   *init  = (ClientInitHandShake *)msg->GetBuffer();
    init->fourth = htonl(4);
    init->fifth  = htonl(2012);

    ClientProtocolRequest *proto = (ClientProtocolRequest *)msg->GetBuffer(20);
    InitProtocolReq( proto, info, expect );

    return msg;
  }

  //------------------------------------------------------------------------
  // Generate the protocol message
  //------------------------------------------------------------------------
  Message *XRootDTransport::GenerateProtocol( HandShakeData     *hsData,
                                              XRootDChannelInfo *info,
                                              kXR_char           expect )
  {
    Log *log = DefaultEnv::GetLog();
    log->Debug( XRootDTransportMsg,
                "[%s] Sending out the kXR_protocol",
                hsData->streamName.c_str() );

    Message *msg = new Message();
    msg->Allocate( sizeof(ClientProtocolRequest) );
    msg->Zero();

    ClientProtocolRequest *proto = (ClientProtocolRequest *)msg->GetBuffer();
    InitProtocolReq( proto, info, expect );

    return msg;
  }

  //------------------------------------------------------------------------
  // Initialize protocol request
  //------------------------------------------------------------------------
  void XRootDTransport::InitProtocolReq( ClientProtocolRequest *request,
                                         XRootDChannelInfo     *info,
                                         kXR_char               expect )
  {
    request->requestid = htons(kXR_protocol);
    request->clientpv  = htonl(kXR_PROTOCOLVERSION);
    request->flags     = ClientProtocolRequest::kXR_secreqs |
                         ClientProtocolRequest::kXR_bifreqs;

    XrdCl::Env *env = XrdCl::DefaultEnv::GetEnv();

    int notlsok = DefaultNoTlsOK;
    env->GetInt( "NoTlsOK", notlsok );

    if (info->encrypted || !notlsok)
      request->flags |= ClientProtocolRequest::kXR_ableTLS;

    bool nodata = false;
    if( expect & ClientProtocolRequest::kXR_ExpBind )
    {
      int value = DefaultTlsNoData;
      env->GetInt( "TlsNoData", value );
      nodata = bool( value );
    }

    if( info->encrypted && !nodata )
      request->flags |= ClientProtocolRequest::kXR_wantTLS;
    request->expect = expect;
    //--------------------------------------------------------------------------
    // If we are in the curse of establishing a connection in the context of
    // TPC update the expect! (this will be never followed be a bind)
    //--------------------------------------------------------------------------
    if( info->istpc )
      request->expect = ClientProtocolRequest::kXR_ExpTPC;
  }

  //----------------------------------------------------------------------------
  // Process the server initial handshake response
  //----------------------------------------------------------------------------
  XRootDStatus XRootDTransport::ProcessServerHS( HandShakeData     *hsData,
                                                 XRootDChannelInfo *info )
  {
    Log *log = DefaultEnv::GetLog();

    Message *msg = hsData->in;
    ServerResponseHeader *respHdr = (ServerResponseHeader *)msg->GetBuffer();
    ServerInitHandShake  *hs      = (ServerInitHandShake *)msg->GetBuffer(4);

    if( respHdr->status != kXR_ok )
    {
      log->Error( XRootDTransportMsg, "[%s] Invalid hand shake response",
                  hsData->streamName.c_str() );

      return XRootDStatus( stFatal, errHandShakeFailed, 0, "Invalid hand shake response." );
    }

    info->protocolVersion = ntohl(hs->protover);
    info->serverFlags     = ntohl(hs->msgval) == kXR_DataServer ?
                            kXR_isServer:
                            kXR_isManager;

    log->Debug( XRootDTransportMsg,
                "[%s] Got the server hand shake response (%s, protocol "
                "version %x)",
                hsData->streamName.c_str(),
                ServerFlagsToStr( info->serverFlags ).c_str(),
                info->protocolVersion );

    return XRootDStatus( stOK, suContinue );
  }

  //----------------------------------------------------------------------------
  // Process the protocol response
  //----------------------------------------------------------------------------
  XRootDStatus XRootDTransport::ProcessProtocolResp( HandShakeData     *hsData,
                                                     XRootDChannelInfo *info )
  {
    Log *log = DefaultEnv::GetLog();

    XRootDStatus st = UnMarshallBody( hsData->in, kXR_protocol );
    if( !st.IsOK() )
      return st;

    ServerResponse *rsp = (ServerResponse*)hsData->in->GetBuffer();


    if( rsp->hdr.status != kXR_ok )
    {
      log->Error( XRootDTransportMsg, "[%s] kXR_protocol request failed",
                                      hsData->streamName.c_str() );

      return XRootDStatus( stFatal, errHandShakeFailed, 0, "kXR_protocol request failed" );
    }

    XrdCl::Env *env = XrdCl::DefaultEnv::GetEnv();
    int notlsok = DefaultNoTlsOK;
    env->GetInt( "NoTlsOK", notlsok );

    if( rsp->body.protocol.pval < kXR_PROTTLSVERSION && info->encrypted )
    {
      //------------------------------------------------------------------------
      // User requested an encrypted connection but the server is to old to
      // support it!
      //------------------------------------------------------------------------
      if( !notlsok ) return XRootDStatus( stFatal, errTlsError, ENOTSUP, "TLS not supported" );

      //------------------------------------------------------------------------
      // We are falling back to unencrypted data transmission, as configured
      // in XRD_NOTLSOK environment variable
      //------------------------------------------------------------------------
      log->Info( XRootDTransportMsg,
                  "[%s] Falling back to unencrypted transmission, server does "
                  "not support TLS encryption.",
                  hsData->streamName.c_str() );
      info->encrypted = false;
    }

    if( rsp->body.protocol.pval >= 0x297 )
      info->serverFlags = rsp->body.protocol.flags;

    if( rsp->hdr.dlen > 8 )
    {
      info->protRespBody = new ServerResponseBody_Protocol();
      info->protRespBody->flags = rsp->body.protocol.flags;
      info->protRespBody->pval  = rsp->body.protocol.pval;

      char*  bodybuff = reinterpret_cast<char*>( &rsp->body.protocol.secreq );
      size_t bodysize = rsp->hdr.dlen - 8;
      XRootDStatus st = ProcessProtocolBody( bodybuff, bodysize, info );
      if( !st.IsOK() )
        return st;
    }

    log->Debug( XRootDTransportMsg,
                "[%s] kXR_protocol successful (%s, protocol version %x)",
                hsData->streamName.c_str(),
                ServerFlagsToStr( info->serverFlags ).c_str(),
                info->protocolVersion );

    if( !( info->serverFlags & kXR_haveTLS ) && info->encrypted )
    {
      //------------------------------------------------------------------------
      // User requested an encrypted connection but the server was not configured
      // to support encryption!
      //------------------------------------------------------------------------
      return XRootDStatus( stFatal, errTlsError, ECONNREFUSED,
                           "Server was not configured to support encryption." );
    }

    //--------------------------------------------------------------------------
    // Now see if we have to enforce encryption in case the server does not
    // support PgRead/PgWrite
    //--------------------------------------------------------------------------
    int tlsOnNoPgrw = DefaultWantTlsOnNoPgrw;
    env->GetInt( "WantTlsOnNoPgrw", tlsOnNoPgrw );
    if( !( info->serverFlags & kXR_suppgrw ) && tlsOnNoPgrw )
    {
      //------------------------------------------------------------------------
      // If user requested encryption just make sure it is not switched off for
      // data
      //------------------------------------------------------------------------
      if( info->encrypted )
      {
        log->Debug( XRootDTransportMsg,
                    "[%s] Server does not support PgRead/PgWrite and"
                    " WantTlsOnNoPgrw is on; enforcing encryption for data.",
                    hsData->streamName.c_str() );
        env->PutInt( "TlsNoData", DefaultTlsNoData );
      }
      //------------------------------------------------------------------------
      // Otherwise, if server is not enforcing data encryption, we will need to
      // redo the protocol request with kXR_wantTLS set.
      //------------------------------------------------------------------------
      else if( !( info->serverFlags & kXR_tlsData ) &&
                ( info->serverFlags & kXR_haveTLS ) )
      {
        info->encrypted = true;
        return XRootDStatus( stOK, suRetry );
      }
    }

    return XRootDStatus( stOK, suContinue );
  }

  XRootDStatus XRootDTransport::ProcessProtocolBody( char              *bodybuff,
                                                     size_t             bodysize,
                                                     XRootDChannelInfo *info  )
  {
    //--------------------------------------------------------------------------
    // Parse bind preferences
    //--------------------------------------------------------------------------
    XrdProto::bifReqs *bifreq = reinterpret_cast<XrdProto::bifReqs*>( bodybuff );
    if( bodysize >= sizeof( XrdProto::bifReqs ) && bifreq->theTag == 'B' )
    {
      bodybuff += sizeof( XrdProto::bifReqs );
      bodysize -= sizeof( XrdProto::bifReqs );

      if( bodysize < bifreq->bifILen )
        return XRootDStatus( stError, errDataError, 0, "Received incomplete "
                             "protocol response." );
      std::string bindprefs_str( bodybuff, bifreq->bifILen );
      std::vector<std::string> bindprefs;
      Utils::splitString( bindprefs, bindprefs_str, "," );
      info->bindSelector.reset( new BindPrefSelector( std::move( bindprefs ) ) );
      bodybuff += bifreq->bifILen;
      bodysize -= bifreq->bifILen;
    }
    //--------------------------------------------------------------------------
    // Parse security requirements
    //--------------------------------------------------------------------------
    XrdProto::secReqs *secreq = reinterpret_cast<XrdProto::secReqs*>( bodybuff );
    if( bodysize >= 6 /*XrdProto::secReqs*/ && secreq->theTag == 'S' )
    {
      memcpy( &info->protRespBody->secreq, secreq, bodysize );
      info->protRespSize = bodysize + 8 /*pval & flags*/;
    }

    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Generate the bind message
  //----------------------------------------------------------------------------
  Message *XRootDTransport::GenerateBind( HandShakeData     *hsData,
                                          XRootDChannelInfo *info )
  {
    Log *log = DefaultEnv::GetLog();

    log->Debug( XRootDTransportMsg,
                "[%s] Sending out the bind request",
                hsData->streamName.c_str() );


    Message *msg = new Message( sizeof( ClientBindRequest ) );
    ClientBindRequest *bindReq = (ClientBindRequest *)msg->GetBuffer();

    bindReq->requestid = kXR_bind;
    memcpy( bindReq->sessid, info->sessionId, 16 );
    bindReq->dlen = 0;
    MarshallRequest( msg );
    return msg;
  }

  //----------------------------------------------------------------------------
  // Generate the bind message
  //----------------------------------------------------------------------------
  XRootDStatus XRootDTransport::ProcessBindResp( HandShakeData     *hsData,
                                                 XRootDChannelInfo *info )
  {
    Log *log = DefaultEnv::GetLog();

    XRootDStatus st = UnMarshallBody( hsData->in, kXR_bind );
    if( !st.IsOK() )
      return st;

    ServerResponse *rsp = (ServerResponse*)hsData->in->GetBuffer();

    if( rsp->hdr.status != kXR_ok )
    {
      log->Error( XRootDTransportMsg, "[%s] kXR_bind request failed",
                  hsData->streamName.c_str() );
      return XRootDStatus( stFatal, errHandShakeFailed, 0, "kXR_bind request failed" );
    }

    info->stream[hsData->subStreamId].pathId = rsp->body.bind.substreamid;
    log->Debug( XRootDTransportMsg, "[%s] kXR_bind successful",
                hsData->streamName.c_str() );

    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Generate the login message
  //----------------------------------------------------------------------------
  Message *XRootDTransport::GenerateLogIn( HandShakeData *hsData,
                                           XRootDChannelInfo *info )
  {
    Log *log = DefaultEnv::GetLog();
    Env *env = DefaultEnv::GetEnv();

    //--------------------------------------------------------------------------
    // Compute the login cgi
    //--------------------------------------------------------------------------
    int timeZone = XrdSysTimer::TimeZone();
    char *hostName = XrdNetUtils::MyHostName();
    std::string countryCode = Utils::FQDNToCC( hostName );
    char *cgiBuffer = new char[1024 + info->logintoken.size()];
    std::string appName;
    std::string monInfo;
    env->GetString( "AppName", appName );
    env->GetString( "MonInfo", monInfo );
    if( info->logintoken.empty() )
    {
      snprintf( cgiBuffer, 1024,
                "xrd.cc=%s&xrd.tz=%d&xrd.appname=%s&xrd.info=%s&"
                "xrd.hostname=%s&xrd.rn=%s", countryCode.c_str(), timeZone,
                appName.c_str(), monInfo.c_str(), hostName, XrdVERSION );
    }
    else
    {
      snprintf( cgiBuffer, 1024,
                "xrd.cc=%s&xrd.tz=%d&xrd.appname=%s&xrd.info=%s&"
                "xrd.hostname=%s&xrd.rn=%s&%s", countryCode.c_str(), timeZone,
                appName.c_str(), monInfo.c_str(), hostName, XrdVERSION, info->logintoken.c_str() );
    }
    uint16_t cgiLen = strlen( cgiBuffer );
    free( hostName );

    //--------------------------------------------------------------------------
    // Generate the message
    //--------------------------------------------------------------------------
    Message *msg = new Message( sizeof(ClientLoginRequest) + cgiLen );
    ClientLoginRequest *loginReq = (ClientLoginRequest *)msg->GetBuffer();

    loginReq->requestid = kXR_login;
    loginReq->pid       = ::getpid();
    loginReq->capver[0] = kXR_asyncap | kXR_ver005;
    loginReq->dlen      = cgiLen;
    loginReq->ability   = kXR_fullurl | kXR_readrdok | kXR_lclfile | kXR_redirflags;
#ifdef WITH_XRDEC
    loginReq->ability2  = kXR_ecredir;
#endif

    int multiProtocol = 0;
    env->GetInt( "MultiProtocol", multiProtocol );
    if(multiProtocol)
      loginReq->ability |= kXR_multipr;

    //--------------------------------------------------------------------------
    // Check the IP stacks
    //--------------------------------------------------------------------------
    XrdNetUtils::NetProt stacks      = XrdNetUtils::NetConfig();
    bool                 dualStack   = false;
    bool                 privateIPv6 = false;
    bool                 privateIPv4 = false;

    if( (stacks & XrdNetUtils::hasIP64) == XrdNetUtils::hasIP64 )
    {
      dualStack = true;
      loginReq->ability  |= kXR_hasipv64;
    }

    if( (stacks & XrdNetUtils::hasIPv6) && !(stacks & XrdNetUtils::hasPub6) )
    {
      privateIPv6 = true;
      loginReq->ability |= kXR_onlyprv6;
    }

    if( (stacks & XrdNetUtils::hasIPv4) && !(stacks & XrdNetUtils::hasPub4) )
    {
      privateIPv4 = true;
      loginReq->ability |= kXR_onlyprv4;
    }

    // The following code snippet tries to overcome the problem that this host
    // may still be dual-stacked but we don't know it because one of the
    // interfaces was not registered in DNS.
    //
    if( !dualStack && hsData->serverAddr )
      {if ( ( ( stacks & XrdNetUtils::hasIPv4 )
       &&    hsData->serverAddr->isIPType(XrdNetAddrInfo::IPv6))
       ||   ( ( stacks & XrdNetUtils::hasIPv6 )
       &&    hsData->serverAddr->isIPType(XrdNetAddrInfo::IPv4)))
          {dualStack = true;
           loginReq->ability  |= kXR_hasipv64;
          }
      }

    //--------------------------------------------------------------------------
    // Check the username
    //--------------------------------------------------------------------------
    std::string buffer( 8, 0 );
    if( hsData->url->GetUserName().length() )
      buffer = hsData->url->GetUserName();
    else
    {
      char *name = new char[1024];
      if( !XrdOucUtils::UserName( geteuid(), name, 1024 ) )
	buffer = name;
      else
	buffer = "_anon_";
      delete [] name;
    }
    buffer.resize( 8, 0 );
    std::copy( buffer.begin(), buffer.end(), (char*)loginReq->username );

    msg->Append( cgiBuffer, cgiLen, 24 );

    log->Debug( XRootDTransportMsg, "[%s] Sending out kXR_login request, "
                "username: %s, cgi: %s, dual-stack: %s, private IPv4: %s, "
                "private IPv6: %s", hsData->streamName.c_str(),
                loginReq->username, cgiBuffer, dualStack ? "true" : "false",
                privateIPv4 ? "true" : "false",
                privateIPv6 ? "true" : "false" );

    delete [] cgiBuffer;
    MarshallRequest( msg );
    return msg;
  }

  //----------------------------------------------------------------------------
  // Process the protocol response
  //----------------------------------------------------------------------------
  XRootDStatus XRootDTransport::ProcessLogInResp( HandShakeData     *hsData,
                                                  XRootDChannelInfo *info )
  {
    Log *log = DefaultEnv::GetLog();

    XRootDStatus st = UnMarshallBody( hsData->in, kXR_login );
    if( !st.IsOK() )
      return st;

    ServerResponse *rsp = (ServerResponse*)hsData->in->GetBuffer();

    if( rsp->hdr.status != kXR_ok )
    {
      log->Error( XRootDTransportMsg, "[%s] Got invalid login response",
                  hsData->streamName.c_str() );
      return XRootDStatus( stFatal, errLoginFailed, 0, "Got invalid login response." );
    }

    if( !info->firstLogIn )
      memcpy( info->oldSessionId, info->sessionId, 16 );

    if( rsp->hdr.dlen == 0 && info->protocolVersion <= 0x289 )
    {
      //--------------------------------------------------------------------------
      // This if statement is there only to support dCache inaccurate
      // implementation of XRoot protocol, that in some cases returns
      // an empty login response for protocol version <= 2.8.9.
      //--------------------------------------------------------------------------
      memset( info->sessionId, 0, 16 );
      log->Warning( XRootDTransportMsg,
                    "[%s] Logged in, accepting empty login response.",
                    hsData->streamName.c_str() );
      return XRootDStatus();
    }

    if( rsp->hdr.dlen < 16 )
      return XRootDStatus( stError, errDataError, 0, "Login response too short." );

    memcpy( info->sessionId, rsp->body.login.sessid, 16 );

    std::string sessId = Utils::Char2Hex( rsp->body.login.sessid, 16 );

    log->Debug( XRootDTransportMsg, "[%s] Logged in, session: %s",
                hsData->streamName.c_str(), sessId.c_str() );

    //--------------------------------------------------------------------------
    // We have an authentication info to process
    //--------------------------------------------------------------------------
    if( rsp->hdr.dlen > 16 )
    {
      size_t len = rsp->hdr.dlen-16;
      info->authBuffer = new char[len+1];
      info->authBuffer[len] = 0;
      memcpy( info->authBuffer, rsp->body.login.sec, len );
      log->Debug( XRootDTransportMsg, "[%s] Authentication is required: %s",
                  hsData->streamName.c_str(), info->authBuffer );

      return XRootDStatus( stOK, suContinue );
    }

    return XRootDStatus();
  }

  //----------------------------------------------------------------------------
  // Do the authentication
  //----------------------------------------------------------------------------
  XRootDStatus XRootDTransport::DoAuthentication( HandShakeData     *hsData,
                                                  XRootDChannelInfo *info )
  {
    //--------------------------------------------------------------------------
    // Prepare
    //--------------------------------------------------------------------------
    Log               *log   = DefaultEnv::GetLog();
    XRootDStreamInfo  &sInfo = info->stream[hsData->subStreamId];
    XrdSecCredentials *credentials = 0;
    std::string        protocolName;

    //--------------------------------------------------------------------------
    // We're doing this for the first time
    //--------------------------------------------------------------------------
    if( sInfo.status == XRootDStreamInfo::LoginSent )
    {
      log->Debug( XRootDTransportMsg, "[%s] Sending authentication data",
                                      hsData->streamName.c_str() );

      //------------------------------------------------------------------------
      // Set up the authentication environment
      //------------------------------------------------------------------------
      info->authEnv = new XrdOucEnv();
      info->authEnv->Put( "sockname", hsData->clientName.c_str() );
      info->authEnv->Put( "username", hsData->url->GetUserName().c_str() );
      info->authEnv->Put( "password", hsData->url->GetPassword().c_str() );

      const URL::ParamsMap &urlParams = hsData->url->GetParams();
      URL::ParamsMap::const_iterator it;
      for( it = urlParams.begin(); it != urlParams.end(); ++it )
      {
        if( it->first.compare( 0, 4, "xrd." ) == 0 ||
            it->first.compare( 0, 6, "xrdcl." ) == 0 )
          info->authEnv->Put( it->first.c_str(), it->second.c_str() );
      }

      //------------------------------------------------------------------------
      // Initialize some other structs
      //------------------------------------------------------------------------
      size_t authBuffLen = strlen( info->authBuffer );
      char *pars = (char *)malloc( authBuffLen + 1 );
      memcpy( pars, info->authBuffer, authBuffLen );
      info->authParams = new XrdSecParameters( pars, authBuffLen );
      sInfo.status = XRootDStreamInfo::AuthSent;
      delete [] info->authBuffer;
      info->authBuffer = 0;

      //------------------------------------------------------------------------
      // Find a protocol that gives us valid credentials
      //------------------------------------------------------------------------
      XRootDStatus st = GetCredentials( credentials, hsData, info );
      if( !st.IsOK() )
      {
        CleanUpAuthentication( info );
        return st;
      }
      protocolName = info->authProtocol->Entity.prot;
    }

    //--------------------------------------------------------------------------
    // We've been here already
    //--------------------------------------------------------------------------
    else
    {
      ServerResponse *rsp = (ServerResponse*)hsData->in->GetBuffer();
      protocolName = info->authProtocol->Entity.prot;

      //------------------------------------------------------------------------
      // We're required to send out more authentication data
      //------------------------------------------------------------------------
      if( rsp->hdr.status == kXR_authmore )
      {
        log->Debug( XRootDTransportMsg,
                    "[%s] Sending more authentication data for %s",
                    hsData->streamName.c_str(), protocolName.c_str() );

        uint32_t          len      = rsp->hdr.dlen;
        char             *secTokenData = (char*)malloc( len );
        memcpy( secTokenData, rsp->body.authmore.data, len );
        XrdSecParameters *secToken     = new XrdSecParameters( secTokenData, len );
        XrdOucErrInfo     ei( "", info->authEnv);
        credentials = info->authProtocol->getCredentials( secToken, &ei );
        delete secToken;

        //----------------------------------------------------------------------
        // The protocol handler refuses to give us the data
        //----------------------------------------------------------------------
        if( !credentials )
        {
          log->Error( XRootDTransportMsg,
                      "[%s] Auth protocol handler for %s refuses to give "
                      "us more credentials %s",
                      hsData->streamName.c_str(), protocolName.c_str(),
                      ei.getErrText() );
          CleanUpAuthentication( info );
          return XRootDStatus( stFatal, errAuthFailed, 0, ei.getErrText() );
        }
      }

      //------------------------------------------------------------------------
      // We have succeeded
      //------------------------------------------------------------------------
      else if( rsp->hdr.status == kXR_ok )
      {
        info->authProtocolName = info->authProtocol->Entity.prot;

        //----------------------------------------------------------------------
        // Do we need protection?
        //----------------------------------------------------------------------
        if( info->protRespBody )
        {
          int rc = XrdSecGetProtection( info->protection, *info->authProtocol, *info->protRespBody, info->protRespSize );
          if( rc > 0 )
          {
            log->Debug( XRootDTransportMsg,
                        "[%s] XrdSecProtect loaded.", hsData->streamName.c_str() );
          }
          else if( rc == 0 )
          {
            log->Debug( XRootDTransportMsg,
                        "[%s] XrdSecProtect: no protection needed.",
                        hsData->streamName.c_str() );
          }
          else
          {
            log->Debug( XRootDTransportMsg,
                        "[%s] Failed to load XrdSecProtect: %s",
                        hsData->streamName.c_str(), XrdSysE2T( -rc ) );
            CleanUpAuthentication( info );

            return XRootDStatus( stError, errAuthFailed, -rc, XrdSysE2T( -rc ) );
          }
        }

        if( !info->protection )
          CleanUpAuthentication( info );
        else
          pSecUnloadHandler->Register( info->authProtocolName );

        log->Debug( XRootDTransportMsg,
                    "[%s] Authenticated with %s.", hsData->streamName.c_str(),
                    protocolName.c_str() );

        //--------------------------------------------------------------------
        // Clear the SSL error queue of the calling thread, as there might be
        // some leftover from the authentication!
        //--------------------------------------------------------------------
        Tls::ClearErrorQueue();

        return XRootDStatus();
      } 
      //------------------------------------------------------------------------
      // Failure
      //------------------------------------------------------------------------
      else if( rsp->hdr.status == kXR_error )
      {
        char *errmsg = new char[rsp->hdr.dlen-3]; errmsg[rsp->hdr.dlen-4] = 0;
        memcpy( errmsg, rsp->body.error.errmsg, rsp->hdr.dlen-4 );
        log->Error( XRootDTransportMsg,
                    "[%s] Authentication with %s failed: %s",
                    hsData->streamName.c_str(), protocolName.c_str(),
                    errmsg );
        delete [] errmsg;

        info->authProtocol->Delete();
        info->authProtocol = 0;

        //----------------------------------------------------------------------
        // Find another protocol that gives us valid credentials
        //----------------------------------------------------------------------
        XRootDStatus st = GetCredentials( credentials, hsData, info );
        if( !st.IsOK() )
        {
          CleanUpAuthentication( info );
          return st;
        }
        protocolName = info->authProtocol->Entity.prot;
      }
      //------------------------------------------------------------------------
      // God knows what
      //------------------------------------------------------------------------
      else
      {
        info->authProtocolName = info->authProtocol->Entity.prot;
        CleanUpAuthentication( info );

        log->Error( XRootDTransportMsg,
                    "[%s] Authentication with %s failed: unexpected answer",
                    hsData->streamName.c_str(), protocolName.c_str() );
        return XRootDStatus( stFatal, errAuthFailed, 0, "Authentication failed: unexpected answer." );
      }
    }

    //--------------------------------------------------------------------------
    // Generate the client request
    //--------------------------------------------------------------------------
    Message *msg = new Message( sizeof(ClientAuthRequest)+credentials->size );
    msg->Zero();
    ClientRequest *req       = (ClientRequest*)msg->GetBuffer();
    char          *reqBuffer = msg->GetBuffer(sizeof(ClientAuthRequest));

    req->header.requestid = kXR_auth;
    req->auth.dlen        = credentials->size;
    memcpy( req->auth.credtype, protocolName.c_str(),
            protocolName.length() > 4 ? 4 : protocolName.length() );

    memcpy( reqBuffer, credentials->buffer, credentials->size );
    hsData->out = msg;
    MarshallRequest( msg );
    delete credentials;

    //------------------------------------------------------------------------
    // Clear the SSL error queue of the calling thread, as there might be
    // some leftover from the authentication!
    //------------------------------------------------------------------------
    Tls::ClearErrorQueue();

    return XRootDStatus( stOK, suContinue );
  }

  //------------------------------------------------------------------------
  // Get the initial credentials using one of the protocols
  //------------------------------------------------------------------------
  XRootDStatus XRootDTransport::GetCredentials( XrdSecCredentials *&credentials,
                                                HandShakeData      *hsData,
                                                XRootDChannelInfo  *info )
  {
    //--------------------------------------------------------------------------
    // Set up the auth handler
    //--------------------------------------------------------------------------
    Log             *log   = DefaultEnv::GetLog();
    XrdOucErrInfo    ei( "", info->authEnv);
    XrdSecGetProt_t  authHandler = GetAuthHandler();
    if( !authHandler )
      return XRootDStatus( stFatal, errAuthFailed, 0, "Could not load authentication handler." );

    //--------------------------------------------------------------------------
    // Retrieve secuid and secgid, if available. These will override the fsuid
    // and fsgid of the current thread reading the credentials to prevent
    // security holes in case this process is running with elevated permissions.
    //--------------------------------------------------------------------------
    char *secuidc = (ei.getEnv()) ? ei.getEnv()->Get("xrdcl.secuid") : 0;
    char *secgidc = (ei.getEnv()) ? ei.getEnv()->Get("xrdcl.secgid") : 0;

    int secuid = -1;
    int secgid = -1;

    if(secuidc) secuid = atoi(secuidc);
    if(secgidc) secgid = atoi(secgidc);

#ifdef __linux__
    ScopedFsUidSetter uidSetter(secuid, secgid, hsData->streamName);
    if(!uidSetter.IsOk()) {
      log->Error( XRootDTransportMsg, "[%s] Error while setting (fsuid, fsgid) to (%d, %d)",
                  hsData->streamName.c_str(), secuid, secgid );
      return XRootDStatus( stFatal, errAuthFailed, 0, "Error while setting (fsuid, fsgid)." );
    }
#else
    if(secuid >= 0 || secgid >= 0) {
      log->Error( XRootDTransportMsg, "[%s] xrdcl.secuid and xrdcl.secgid only supported on Linux.",
                  hsData->streamName.c_str() );
      return XRootDStatus( stFatal, errAuthFailed, 0, "xrdcl.secuid and xrdcl.secgid"
                                                      " only supported on Linux" );
    }
#endif

    //--------------------------------------------------------------------------
    // Loop over the possible protocols to find one that gives us valid
    // credentials
    //--------------------------------------------------------------------------
    XrdNetAddr &srvAddrInfo = *const_cast<XrdNetAddr *>(hsData->serverAddr);
    if( info->encrypted || ( info->serverFlags & kXR_gotoTLS ) ||
        ( info->serverFlags & kXR_tlsLogin ) )
      srvAddrInfo.SetTLS( true );
    while(1)
    {
      //------------------------------------------------------------------------
      // Get the protocol
      //------------------------------------------------------------------------
      info->authProtocol = (*authHandler)( hsData->url->GetHostName().c_str(),
                                           srvAddrInfo,
                                           *info->authParams,
                                           &ei );
      if( !info->authProtocol )
      {
        log->Error( XRootDTransportMsg, "[%s] No protocols left to try",
                    hsData->streamName.c_str() );
        return XRootDStatus( stFatal, errAuthFailed, 0, "No protocols left to try" );
      }

      std::string protocolName = info->authProtocol->Entity.prot;
      log->Debug( XRootDTransportMsg, "[%s] Trying to authenticate using %s",
                  hsData->streamName.c_str(), protocolName.c_str() );

      //------------------------------------------------------------------------
      // Get the credentials from the current protocol
      //------------------------------------------------------------------------
      credentials = info->authProtocol->getCredentials( 0, &ei );
      if( !credentials )
      {
        log->Debug( XRootDTransportMsg,
                    "[%s] Cannot get credentials for protocol %s: %s",
                    hsData->streamName.c_str(), protocolName.c_str(),
                    ei.getErrText() );
        info->authProtocol->Delete();
        continue;
      }
      return XRootDStatus( stOK, suContinue );
    }
  }

  //------------------------------------------------------------------------
  // Clean up the data structures created for the authentication process
  //------------------------------------------------------------------------
  Status XRootDTransport::CleanUpAuthentication( XRootDChannelInfo *info )
  {
    if( info->authProtocol )
      info->authProtocol->Delete();
    delete info->authParams;
    delete info->authEnv;
    info->authProtocol = 0;
    info->authParams   = 0;
    info->authEnv      = 0;
    Tls::ClearErrorQueue();
    return Status();
  }

  //------------------------------------------------------------------------
  // Clean up the data structures created for the protection purposes
  //------------------------------------------------------------------------
  Status XRootDTransport::CleanUpProtection( XRootDChannelInfo *info )
  {
    XrdSysRWLockHelper scope( pSecUnloadHandler->lock );
    if( pSecUnloadHandler->unloaded ) return Status( stError, errInvalidOp );

    if( info->protection )
    {
      info->protection->Delete();
      info->protection = 0;

      CleanUpAuthentication( info );
    }

    if( info->protRespBody )
    {
      delete info->protRespBody;
      info->protRespBody = 0;
      info->protRespSize = 0;
    }

    return Status();
  }

  //----------------------------------------------------------------------------
  // Get the authentication function handle
  //----------------------------------------------------------------------------
  XrdSecGetProt_t XRootDTransport::GetAuthHandler()
  {
    Log *log = DefaultEnv::GetLog();
    char errorBuff[1024];

    // the static constructor is invoked only once and it is guaranteed that this
    // is thread safe
    static std::atomic<XrdSecGetProt_t> authHandler( XrdSecLoadSecFactory( errorBuff, 1024 ) );
    auto ret = authHandler.load( std::memory_order_relaxed );
    if( ret ) return ret;

    // if we are here it means we failed to load the security library for the
    // first time and we hope the environment changed

    // obtain a lock
    static XrdSysMutex mtx;
    XrdSysMutexHelper lck( mtx );
    // check if in the meanwhile some else didn't load the library
    ret = authHandler.load( std::memory_order_relaxed );
    if( ret ) return ret;

    // load the library
    ret = XrdSecLoadSecFactory( errorBuff, 1024 );
    authHandler.store( ret, std::memory_order_relaxed );
    // if we failed report an error
    if( !ret )
    {
      log->Error( XRootDTransportMsg,
                  "Unable to get the security framework: %s", errorBuff );
      return 0;
    }
    return ret;
  }

  //----------------------------------------------------------------------------
  // Generate the end session message
  //----------------------------------------------------------------------------
  Message *XRootDTransport::GenerateEndSession( HandShakeData     *hsData,
                                                XRootDChannelInfo *info )
  {
    Log *log = DefaultEnv::GetLog();

    //--------------------------------------------------------------------------
    // Generate the message
    //--------------------------------------------------------------------------
    Message *msg = new Message( sizeof(ClientEndsessRequest) );
    ClientEndsessRequest *endsessReq = (ClientEndsessRequest *)msg->GetBuffer();

    endsessReq->requestid = kXR_endsess;
    memcpy( endsessReq->sessid, info->oldSessionId, 16 );
    std::string sessId = Utils::Char2Hex( endsessReq->sessid, 16 );

    log->Debug( XRootDTransportMsg, "[%s] Sending out kXR_endsess for session:"
                " %s", hsData->streamName.c_str(), sessId.c_str() );

    MarshallRequest( msg );

    Message *sign = 0;
    GetSignature( msg, sign, info );
    if( sign )
    {
      //------------------------------------------------------------------------
      // Now place both the signature and the request in a single buffer
      //------------------------------------------------------------------------
      uint32_t size = sign->GetSize();
      sign->ReAllocate( size + msg->GetSize() );
      char* buffer = sign->GetBuffer( size );
      memcpy( buffer, msg->GetBuffer(), msg->GetSize() );
      msg->Grab( sign->GetBuffer(), sign->GetSize() );
    }

    return msg;
  }

  //----------------------------------------------------------------------------
  // Process the protocol response
  //----------------------------------------------------------------------------
  Status XRootDTransport::ProcessEndSessionResp( HandShakeData     *hsData,
                                                 XRootDChannelInfo *info )
  {
    Log *log = DefaultEnv::GetLog();

    Status st = UnMarshallBody( hsData->in, kXR_endsess );
    if( !st.IsOK() )
      return st;

    ServerResponse *rsp = (ServerResponse*)hsData->in->GetBuffer();

    // If we're good, we're good!
    if( rsp->hdr.status == kXR_ok )
      return Status();

    // we ignore not found errors as such an error means the connection
    // has been already terminated
    if( rsp->hdr.status == kXR_error && rsp->body.error.errnum == kXR_NotFound )
      return Status();

    // other errors
    if( rsp->hdr.status == kXR_error )
    {
      std::string errorMsg( rsp->body.error.errmsg, rsp->hdr.dlen - 4 );
      log->Error( XRootDTransportMsg, "[%s] Got error response to "
                  "kXR_endsess: %s", hsData->streamName.c_str(),
                  errorMsg.c_str() );
      return Status( stFatal, errHandShakeFailed );
    }

    // Wait Response.
    if( rsp->hdr.status == kXR_wait )
    {
      std::string msg( rsp->body.wait.infomsg, rsp->hdr.dlen - 4 );
      log->Info( XRootDTransportMsg, "[%s] Got wait response to "
                  "kXR_endsess: %s", hsData->streamName.c_str(),
                  msg.c_str() );
      hsData->out = GenerateEndSession( hsData, info );
      return Status( stOK, suRetry );
    }

    // Any other response is protocol violation
    return Status( stError, errDataError );
  }

  //----------------------------------------------------------------------------
  // Get a string representation of the server flags
  //----------------------------------------------------------------------------
  std::string XRootDTransport::ServerFlagsToStr( uint32_t flags )
  {
    std::string repr = "type: ";
    if( flags & kXR_isManager )
      repr += "manager ";

    else if( flags & kXR_isServer )
      repr += "server ";

    repr += "[";

    if( flags & kXR_attrMeta )
      repr += "meta ";

    else if( flags & kXR_attrProxy )
      repr += "proxy ";

    else if( flags & kXR_attrSuper )
      repr += "super ";

    else
      repr += " ";

    repr.erase( repr.length()-1, 1 );

    repr += "]";
    return repr;
  }
}

namespace
{
  // Extract file name from a request
  //----------------------------------------------------------------------------
  char *GetDataAsString( char *msg )
  {
    ClientRequestHdr *req = (ClientRequestHdr*)msg;
    char *fn = new char[req->dlen+1];
    memcpy( fn, msg + 24, req->dlen );
    fn[req->dlen] = 0;
    return fn;
  }
}

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Get the description of a message
  //----------------------------------------------------------------------------
  void XRootDTransport::GenerateDescription( char *msg, std::ostringstream &o )
  {
    Log *log = DefaultEnv::GetLog();
    if( log->GetLevel() < Log::ErrorMsg )
      return;

    ClientRequestHdr *req = (ClientRequestHdr *)msg;
    switch( req->requestid )
    {
      //------------------------------------------------------------------------
      // kXR_open
      //------------------------------------------------------------------------
      case kXR_open:
      {
        ClientOpenRequest *sreq = (ClientOpenRequest *)msg;
        o << "kXR_open (";
        char *fn = GetDataAsString( msg );
        o << "file: " << fn << ", ";
        delete [] fn;
        o << "mode: 0" << std::setbase(8) << sreq->mode << ", ";
        o << std::setbase(10);
        o << "flags: ";
        if( sreq->options == 0 )
          o << "none";
        else
        {
          if( sreq->options & kXR_delete )
            o << "kXR_delete ";
          if( sreq->options & kXR_force )
            o << "kXR_force ";
          if( sreq->options & kXR_mkpath )
            o << "kXR_mkpath ";
          if( sreq->options & kXR_new )
            o << "kXR_new ";
          if( sreq->options & kXR_nowait )
            o << "kXR_delete ";
          if( sreq->options & kXR_open_apnd )
            o << "kXR_open_apnd ";
          if( sreq->options & kXR_open_read )
            o << "kXR_open_read ";
          if( sreq->options & kXR_open_updt )
            o << "kXR_open_updt ";
          if( sreq->options & kXR_posc )
            o << "kXR_posc ";
          if( sreq->options & kXR_refresh )
            o << "kXR_refresh ";
          if( sreq->options & kXR_replica )
            o << "kXR_replica ";
          if( sreq->options & kXR_seqio )
            o << "kXR_seqio ";
          if( sreq->options & kXR_async )
            o << "kXR_async ";
          if( sreq->options & kXR_retstat )
            o << "kXR_retstat ";
        }
        o << ")";
        break;
      }

      //------------------------------------------------------------------------
      // kXR_close
      //------------------------------------------------------------------------
      case kXR_close:
      {
        ClientCloseRequest *sreq = (ClientCloseRequest *)msg;
        o << "kXR_close (";
        o << "handle: " << FileHandleToStr( sreq->fhandle );
        o << ")";
        break;
      }

      //------------------------------------------------------------------------
      // kXR_stat
      //------------------------------------------------------------------------
      case kXR_stat:
      {
        ClientStatRequest *sreq = (ClientStatRequest *)msg;
        o << "kXR_stat (";
        if( sreq->dlen )
        {
          char *fn = GetDataAsString( msg );;
          o << "path: " << fn << ", ";
          delete [] fn;
        }
        else
        {
          o << "handle: " << FileHandleToStr( sreq->fhandle );
          o << ", ";
        }
        o << "flags: ";
        if( sreq->options == 0 )
          o << "none";
        else
        {
          if( sreq->options & kXR_vfs )
            o << "kXR_vfs";
        }
        o << ")";
        break;
      }

      //------------------------------------------------------------------------
      // kXR_read
      //------------------------------------------------------------------------
      case kXR_read:
      {
        ClientReadRequest *sreq = (ClientReadRequest *)msg;
        o << "kXR_read (";
        o << "handle: " << FileHandleToStr( sreq->fhandle );
        o << std::setbase(10);
        o << ", ";
        o << "offset: " << sreq->offset << ", ";
        o << "size: " << sreq->rlen << ")";
        break;
      }

      //------------------------------------------------------------------------
      // kXR_pgread
      //------------------------------------------------------------------------
      case kXR_pgread:
      {
        ClientPgReadRequest *sreq = (ClientPgReadRequest *)msg;
        o << "kXR_pgread (";
        o << "handle: " << FileHandleToStr( sreq->fhandle );
        o << std::setbase(10);
        o << ", ";
        o << "offset: " << sreq->offset << ", ";
        o << "size: " << sreq->rlen << ")";
        break;
      }

      //------------------------------------------------------------------------
      // kXR_write
      //------------------------------------------------------------------------
      case kXR_write:
      {
        ClientWriteRequest *sreq = (ClientWriteRequest *)msg;
        o << "kXR_write (";
        o << "handle: " << FileHandleToStr( sreq->fhandle );
        o << std::setbase(10);
        o << ", ";
        o << "offset: " << sreq->offset << ", ";
        o << "size: " << sreq->dlen << ")";
        break;
      }

      //------------------------------------------------------------------------
      // kXR_pgwrite
      //------------------------------------------------------------------------
      case kXR_pgwrite:
      {
        ClientPgWriteRequest *sreq = (ClientPgWriteRequest *)msg;
        o << "kXR_pgwrite (";
        o << "handle: " << FileHandleToStr( sreq->fhandle );
        o << std::setbase(10);
        o << ", ";
        o << "offset: " << sreq->offset << ", ";
        o << "size: " << sreq->dlen << ")";
        break;
      }

      //------------------------------------------------------------------------
      // kXR_sync
      //------------------------------------------------------------------------
      case kXR_sync:
      {
        ClientSyncRequest *sreq = (ClientSyncRequest *)msg;
        o << "kXR_sync (";
        o << "handle: " << FileHandleToStr( sreq->fhandle );
        o << ")";
        break;
      }

      //------------------------------------------------------------------------
      // kXR_truncate
      //------------------------------------------------------------------------
      case kXR_truncate:
      {
        ClientTruncateRequest *sreq = (ClientTruncateRequest *)msg;
        o << "kXR_truncate (";
        if( !sreq->dlen )
          o << "handle: " << FileHandleToStr( sreq->fhandle );
        else
        {
          char *fn = GetDataAsString( msg );
          o << "file: " << fn;
          delete [] fn;
        }
        o << std::setbase(10);
        o << ", ";
        o << "offset: " << sreq->offset;
        o << ")";
        break;
      }

      //------------------------------------------------------------------------
      // kXR_readv
      //------------------------------------------------------------------------
      case kXR_readv:
      {
        unsigned char *fhandle = 0;
        o << "kXR_readv (";

        o << "handle: ";
        readahead_list *dataChunk = (readahead_list*)(msg + 24 );
        fhandle = dataChunk[0].fhandle;
        if( fhandle )
          o << FileHandleToStr( fhandle );
        else
          o << "unknown";
        o << ", ";
        o << std::setbase(10);
        o << "chunks: [";
        uint64_t size      = 0;
        for( size_t i = 0; i < req->dlen/sizeof(readahead_list); ++i )
        {
          size += dataChunk[i].rlen;
          o << "(offset: " << dataChunk[i].offset;
          o << ", size: " << dataChunk[i].rlen << "); ";
        }
        o << "], ";
        o << "total size: " << size << ")";
        break;
      }

      //------------------------------------------------------------------------
      // kXR_writev
      //------------------------------------------------------------------------
      case kXR_writev:
      {
        unsigned char *fhandle = 0;
        o << "kXR_writev (";

        XrdProto::write_list *wrtList =
            reinterpret_cast<XrdProto::write_list*>( msg + 24 );
        uint64_t size      = 0;
        uint32_t numChunks = 0;
        for( size_t i = 0; i < req->dlen/sizeof(XrdProto::write_list); ++i )
        {
          fhandle = wrtList[i].fhandle;
          size   += wrtList[i].wlen;
          ++numChunks;
        }
        o << "handle: ";
        if( fhandle )
          o << FileHandleToStr( fhandle );
        else
          o << "unknown";
        o << ", ";
        o << std::setbase(10);
        o << "chunks: " << numChunks << ", ";
        o << "total size: " << size << ")";
        break;
      }

      //------------------------------------------------------------------------
      // kXR_locate
      //------------------------------------------------------------------------
      case kXR_locate:
      {
        ClientLocateRequest *sreq = (ClientLocateRequest *)msg;
        char *fn = GetDataAsString( msg );;
        o << "kXR_locate (";
        o << "path: " << fn << ", ";
        delete [] fn;
        o << "flags: ";
        if( sreq->options == 0 )
          o << "none";
        else
        {
          if( sreq->options & kXR_refresh )
            o << "kXR_refresh ";
          if( sreq->options & kXR_prefname )
            o << "kXR_prefname ";
          if( sreq->options & kXR_nowait )
            o << "kXR_nowait ";
          if( sreq->options & kXR_force )
            o << "kXR_force ";
          if( sreq->options & kXR_compress )
            o << "kXR_compress ";
        }
        o << ")";
        break;
      }

      //------------------------------------------------------------------------
      // kXR_mv
      //------------------------------------------------------------------------
      case kXR_mv:
      {
        ClientMvRequest *sreq = (ClientMvRequest *)msg;
        o << "kXR_mv (";
        o << "source: ";
        o.write( msg + sizeof( ClientMvRequest ), sreq->arg1len );
        o << ", ";
        o << "destination: ";
        o.write( msg + sizeof( ClientMvRequest ) + sreq->arg1len + 1, sreq->dlen - sreq->arg1len - 1 );
        o << ")";
        break;
      }

      //------------------------------------------------------------------------
      // kXR_query
      //------------------------------------------------------------------------
      case kXR_query:
      {
        ClientQueryRequest *sreq = (ClientQueryRequest *)msg;
        o << "kXR_query (";
        o << "code: ";
        switch( sreq->infotype )
        {
          case kXR_Qconfig: o << "kXR_Qconfig"; break;
          case kXR_Qckscan: o << "kXR_Qckscan"; break;
          case kXR_Qcksum:  o << "kXR_Qcksum"; break;
          case kXR_Qopaque: o << "kXR_Qopaque"; break;
          case kXR_Qopaquf: o << "kXR_Qopaquf"; break;
          case kXR_Qopaqug: o << "kXR_Qopaqug"; break;
          case kXR_QPrep:   o << "kXR_QPrep"; break;
          case kXR_Qspace:  o << "kXR_Qspace"; break;
          case kXR_QStats:  o << "kXR_QStats"; break;
          case kXR_Qvisa:   o << "kXR_Qvisa"; break;
          case kXR_Qxattr:  o << "kXR_Qxattr"; break;
          default: o << sreq->infotype; break;
        }
        o << ", ";          

        if( sreq->infotype == kXR_Qopaqug || sreq->infotype == kXR_Qvisa )
        {
          o << "handle: " << FileHandleToStr( sreq->fhandle );
          o << ", ";
        }

        o << "arg length: " << sreq->dlen << ")";
        break;
      }

      //------------------------------------------------------------------------
      // kXR_rm
      //------------------------------------------------------------------------
      case kXR_rm:
      {
        o << "kXR_rm (";
        char *fn = GetDataAsString( msg );;
        o << "path: " << fn << ")";
        delete [] fn;
        break;
      }

      //------------------------------------------------------------------------
      // kXR_mkdir
      //------------------------------------------------------------------------
      case kXR_mkdir:
      {
        ClientMkdirRequest *sreq = (ClientMkdirRequest *)msg;
        o << "kXR_mkdir (";
        char *fn = GetDataAsString( msg );
        o << "path: " << fn << ", ";
        delete [] fn;
        o << "mode: 0" << std::setbase(8) << sreq->mode << ", ";
        o << std::setbase(10);
        o << "flags: ";
        if( sreq->options[0] == 0 )
          o << "none";
        else
        {
          if( sreq->options[0] & kXR_mkdirpath )
            o << "kXR_mkdirpath";
        }
        o << ")";
        break;
      }

      //------------------------------------------------------------------------
      // kXR_rmdir
      //------------------------------------------------------------------------
      case kXR_rmdir:
      {
        o << "kXR_rmdir (";
        char *fn = GetDataAsString( msg );
        o << "path: " << fn << ")";
        delete [] fn;
        break;
      }

      //------------------------------------------------------------------------
      // kXR_chmod
      //------------------------------------------------------------------------
      case kXR_chmod:
      {
        ClientChmodRequest *sreq = (ClientChmodRequest *)msg;
        o << "kXR_chmod (";
        char *fn = GetDataAsString( msg );
        o << "path: " << fn << ", ";
        delete [] fn;
        o << "mode: 0" << std::setbase(8) << sreq->mode << ")";
        break;
      }

      //------------------------------------------------------------------------
      // kXR_ping
      //------------------------------------------------------------------------
      case kXR_ping:
      {
        o << "kXR_ping ()";
        break;
      }

      //------------------------------------------------------------------------
      // kXR_protocol
      //------------------------------------------------------------------------
      case kXR_protocol:
      {
        ClientProtocolRequest *sreq = (ClientProtocolRequest *)msg;
        o << "kXR_protocol (";
        o << "clientpv: 0x" << std::setbase(16) << sreq->clientpv << ")";
        break;
      }

      //------------------------------------------------------------------------
      // kXR_dirlist
      //------------------------------------------------------------------------
      case kXR_dirlist:
      {
        o << "kXR_dirlist (";
        char *fn = GetDataAsString( msg );;
        o << "path: " << fn << ")";
        delete [] fn;
        break;
      }

      //------------------------------------------------------------------------
      // kXR_set
      //------------------------------------------------------------------------
      case kXR_set:
      {
        o << "kXR_set (";
        char *fn = GetDataAsString( msg );;
        o << "data: " << fn << ")";
        delete [] fn;
        break;
      }

      //------------------------------------------------------------------------
      // kXR_prepare
      //------------------------------------------------------------------------
      case kXR_prepare:
      {
        ClientPrepareRequest *sreq = (ClientPrepareRequest *)msg;
        o << "kXR_prepare (";
        o << "flags: ";

        if( sreq->options == 0 )
          o << "none";
        else
        {
          if( sreq->options & kXR_stage )
            o << "kXR_stage ";
          if( sreq->options & kXR_wmode )
            o << "kXR_wmode ";
          if( sreq->options & kXR_coloc )
            o << "kXR_coloc ";
          if( sreq->options & kXR_fresh )
            o << "kXR_fresh ";
        }

        o << ", priority: " << (int) sreq->prty << ", ";

        char *fn = GetDataAsString( msg );
        char *cursor;
        for( cursor = fn; *cursor; ++cursor )
          if( *cursor == '\n' ) *cursor = ' ';

        o << "paths: " << fn << ")";
        delete [] fn;
        break;
      }

      case kXR_chkpoint:
      {
        ClientChkPointRequest *sreq = (ClientChkPointRequest*)msg;
        o << "kXR_chkpoint (";
        o << "opcode: ";
        if( sreq->opcode == kXR_ckpBegin )         o << "kXR_ckpBegin)";
        else if( sreq->opcode == kXR_ckpCommit )   o << "kXR_ckpCommit)";
        else if( sreq->opcode == kXR_ckpQuery )    o << "kXR_ckpQuery)";
        else if( sreq->opcode == kXR_ckpRollback ) o << "kXR_ckpRollback)";
        else if( sreq->opcode == kXR_ckpXeq )
        {
          o << "kXR_ckpXeq) ";
          // In this case our request body will be one of kXR_pgwrite,
          // kXR_truncate, kXR_write, or kXR_writev request.
          GenerateDescription( msg + sizeof( ClientChkPointRequest ), o );
        }

        break;
      }

      //------------------------------------------------------------------------
      // Default
      //------------------------------------------------------------------------
      default:
      {
        o << "kXR_unknown (length: " << req->dlen << ")";
        break;
      }
    };
  }

  //----------------------------------------------------------------------------
  // Get a string representation of file handle
  //----------------------------------------------------------------------------
  std::string XRootDTransport::FileHandleToStr( const unsigned char handle[4] )
  {
    std::ostringstream o;
    o << "0x";
    for( uint8_t i = 0; i < 4; ++i )
    {
      o << std::setbase(16) << std::setfill('0') << std::setw(2);
      o << (int)handle[i];
    }
    return o.str();
  }
}
