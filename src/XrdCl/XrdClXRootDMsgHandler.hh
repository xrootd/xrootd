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

#ifndef __XRD_CL_XROOTD_MSG_HANDLER_HH__
#define __XRD_CL_XROOTD_MSG_HANDLER_HH__

#include "XrdCl/XrdClPostMasterInterfaces.hh"
#include "XrdCl/XrdClXRootDResponses.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClMessage.hh"
#include "XProtocol/XProtocol.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClAsyncPageReader.hh"

#include "XrdSys/XrdSysPthread.hh"
#include "XrdSys/XrdSysPageSize.hh"
#include "XrdSys/XrdSysKernelBuffer.hh"
#include "XrdSys/XrdSysPlatform.hh"

#include "XrdOuc/XrdOucPgrwUtils.hh"

#include <sys/uio.h>
#include <arpa/inet.h> // for network unmarshaling stuff

#include <array>
#include <list>
#include <memory>
#include <atomic>

namespace XrdCl
{
  class PostMaster;
  class SIDManager;
  class URL;
  class LocalFileHandler;
  class Socket;

  //----------------------------------------------------------------------------
  // Single entry in the redirect-trace-back
  //----------------------------------------------------------------------------
  struct RedirectEntry
  {
      enum Type
      {
        EntryRedirect,
        EntryRedirectOnWait,
        EntryRetry,
        EntryWait
      };

      RedirectEntry( const URL &from, const URL &to, Type type ) :
        from( from ), to( to ), type( type )
      {

      }

      URL          from;
      URL          to;
      Type         type;
      XRootDStatus status;

      std::string ToString( bool prevok = true )
      {
        const std::string tostr   = to.GetLocation();
        const std::string fromstr = from.GetLocation();

        if( prevok )
        {
          switch( type )
          {
            case EntryRedirect: return "Redirected from: " + fromstr + " to: "
                + tostr;

            case EntryRedirectOnWait: return "Server responded with wait. "
                "Falling back to virtual redirector: " + tostr;

            case EntryRetry: return "Retrying: " + tostr;

            case EntryWait: return "Waited at server request. Resending: "
                + tostr;
          }
        }
        return "Failed at: " + fromstr + ", retrying at: " + tostr;
      }
  };

  //----------------------------------------------------------------------------
  //! Handle/Process/Forward XRootD messages
  //----------------------------------------------------------------------------
  class XRootDMsgHandler: public IncomingMsgHandler,
                          public OutgoingMsgHandler
  {
      friend class HandleRspJob;

    public:
      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @param msg         message that has been sent out
      //! @param respHandler response handler to be called then the final
      //!                    final response arrives
      //! @param url         the url the message has been sent to
      //! @param sidMgr      the sid manager used to allocate SID for the initial
      //!                    message
      //------------------------------------------------------------------------
      XRootDMsgHandler( Message                     *msg,
                        ResponseHandler             *respHandler,
                        const URL                   *url,
                        std::shared_ptr<SIDManager>  sidMgr,
                        LocalFileHandler            *lFileHandler):
        pRequest( msg ),
        pResponse( 0 ),
        pResponseHandler( respHandler ),
        pUrl( *url ),
        pEffectiveDataServerUrl( 0 ),
        pSidMgr( sidMgr ),
        pLFileHandler( lFileHandler ),
        pExpiration( 0 ),
        pRedirectAsAnswer( false ),
        pOksofarAsAnswer( false ),
        pHosts( 0 ),
        pHasLoadBalancer( false ),
        pHasSessionId( false ),
        pChunkList( 0 ),
        pKBuff( 0 ),
        pRedirectCounter( 0 ),
        pNotAuthorizedCounter( 0 ),

        pAsyncOffset( 0 ),
        pAsyncChunkOffset( 0 ),
        pAsyncChunkIndex( 0 ),
        pAsyncReadSize( 0 ),
        pAsyncReadBuffer( 0 ),
        pAsyncMsgSize( 0 ),

        pReadRawStarted( false ),
        pReadRawCurrentOffset( 0 ),

//        pPgReadCksumBuff( 4 ),
//        pPgReadOffset( 0 ),
//        pPgReadLength( 0 ),
//        pPgReadCurrentPageSize( 0 ),

        pPgWrtCksumBuff( 4 ),
        pPgWrtCurrentPageOffset( 0 ),
        pPgWrtCurrentPageNb( 0 ),

        pReadVRawMsgOffset( 0 ),
        pReadVRawChunkHeaderDone( false ),
        pReadVRawChunkHeaderStarted( false ),
        pReadVRawSizeError( false ),
        pReadVRawChunkIndex( 0 ),
        pReadVRawMsgDiscard( false ),

        pOtherRawStarted( false ),

        pFollowMetalink( false ),

        pStateful( false ),

        pAggregatedWaitTime( 0 ),

        pMsgInFly( false ),

        pTimeoutFence( false ),

        pDirListStarted( false ),
        pDirListWithStat( false ),

        pCV( 0 ),

        pSslErrCnt( 0 ),

        pRspStatusBodyUnMarshaled( false ),
        pRspPgWrtRetrnsmReqUnMarshalled( false )
      {
        pPostMaster = DefaultEnv::GetPostMaster();
        if( msg->GetSessionId() )
          pHasSessionId = true;
        memset( &pReadVRawChunkHeader, 0, sizeof( readahead_list ) );

        Log *log = DefaultEnv::GetLog();
        log->Debug( ExDbgMsg, "[%s] MsgHandler created: 0x%x (message: %s ).",
                    pUrl.GetHostId().c_str(), this,
                    pRequest->GetDescription().c_str() );

        ClientRequestHdr *hdr = (ClientRequestHdr*)pRequest->GetBuffer();
        if( ntohs( hdr->requestid ) == kXR_pgread )
        {
          ClientPgReadRequest *pgrdreq = (ClientPgReadRequest*)pRequest->GetBuffer();
          pCrc32cDigests.reserve( XrdOucPgrwUtils::csNum( ntohll( pgrdreq->offset ),
                                                         ntohl( pgrdreq->rlen ) ) );
        }
      }

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      ~XRootDMsgHandler()
      {
        DumpRedirectTraceBack();

        if( !pHasSessionId )
          delete pRequest;
        delete pResponse;
        std::vector<Message *>::iterator it;
        for( it = pPartialResps.begin(); it != pPartialResps.end(); ++it )
          delete *it;

        delete pEffectiveDataServerUrl;

        pRequest                = reinterpret_cast<Message*>( 0xDEADBEEF );
        pResponse               = reinterpret_cast<Message*>( 0xDEADBEEF );
        pResponseHandler        = reinterpret_cast<ResponseHandler*>( 0xDEADBEEF );
        pPostMaster             = reinterpret_cast<PostMaster*>( 0xDEADBEEF );
        pLFileHandler           = reinterpret_cast<LocalFileHandler*>( 0xDEADBEEF );
        pHosts                  = reinterpret_cast<HostList*>( 0xDEADBEEF );
        pChunkList              = reinterpret_cast<ChunkList*>( 0xDEADBEEF );
        pEffectiveDataServerUrl = reinterpret_cast<URL*>( 0xDEADBEEF );

        Log *log = DefaultEnv::GetLog();
        log->Debug( ExDbgMsg, "[%s] Destroying MsgHandler: 0x%x.",
                    pUrl.GetHostId().c_str(), this );
      }

      //------------------------------------------------------------------------
      //! Examine an incoming message, and decide on the action to be taken
      //!
      //! @param msg    the message, may be zero if receive failed
      //! @return       action type that needs to be take wrt the message and
      //!               the handler
      //------------------------------------------------------------------------
      virtual uint16_t Examine( Message *msg  );

      //------------------------------------------------------------------------
      //! Reexamine the incoming message, and decide on the action to be taken
      //!
      //! In case of kXR_status the message can be only fully examined after
      //! reading the whole body (without raw data).
      //!
      //! @param msg    the message, may be zero if receive failed
      //! @return       action type that needs to be take wrt the message and
      //!               the handler
      //------------------------------------------------------------------------
      virtual uint16_t InspectStatusRsp( Message *msg );

      //------------------------------------------------------------------------
      //! Get handler sid
      //!
      //! return sid of the corresponding request, otherwise 0
      //------------------------------------------------------------------------
      virtual uint16_t GetSid() const;

      //------------------------------------------------------------------------
      //! Process the message if it was "taken" by the examine action
      //!
      //! @param msg the message to be processed
      //------------------------------------------------------------------------
      virtual void Process( Message *msg );

      //------------------------------------------------------------------------
      //! Read message body directly from a socket - called if Examine returns
      //! Raw flag - only socket related errors may be returned here
      //!
      //! @param msg       the corresponding message header
      //! @param socket    the socket to read from
      //! @param bytesRead number of bytes read by the method
      //! @return          stOK & suDone if the whole body has been processed
      //!                  stOK & suRetry if more data is needed
      //!                  stError on failure
      //------------------------------------------------------------------------
      virtual XRootDStatus ReadMessageBody( Message  *msg,
                                            Socket   *socket,
                                            uint32_t &bytesRead );

      //------------------------------------------------------------------------
      //! Handle an event other that a message arrival
      //!
      //! @param event     type of the event
      //! @param streamNum stream concerned
      //! @param status    status info
      //------------------------------------------------------------------------
      virtual uint8_t OnStreamEvent( StreamEvent  event,
                                     XRootDStatus status );

      //------------------------------------------------------------------------
      //! The requested action has been performed and the status is available
      //------------------------------------------------------------------------
      virtual void OnStatusReady( const Message *message,
                                  XRootDStatus   status );

      //------------------------------------------------------------------------
      //! Are we a raw writer or not?
      //------------------------------------------------------------------------
      virtual bool IsRaw() const;

      //------------------------------------------------------------------------
      //! Write message body directly to a socket - called if IsRaw returns
      //! true - only socket related errors may be returned here
      //!
      //! @param socket    the socket to read from
      //! @param bytesRead number of bytes read by the method
      //! @return          stOK & suDone if the whole body has been processed
      //!                  stOK & suRetry if more data needs to be written
      //!                  stError on failure
      //------------------------------------------------------------------------
      XRootDStatus WriteMessageBody( Socket   *socket,
                                     uint32_t &bytesWritten );

      //------------------------------------------------------------------------
      //! Called after the wait time for kXR_wait has elapsed
      //!
      //! @param  now current timestamp
      //------------------------------------------------------------------------
      void WaitDone( time_t now );

      //------------------------------------------------------------------------
      //! Set a timestamp after which we give up
      //------------------------------------------------------------------------
      void SetExpiration( time_t expiration )
      {
        pExpiration = expiration;
      }

      //------------------------------------------------------------------------
      //! Treat the kXR_redirect response as a valid answer to the message
      //! and notify the handler with the URL as a response
      //------------------------------------------------------------------------
      void SetRedirectAsAnswer( bool redirectAsAnswer )
      {
        pRedirectAsAnswer = redirectAsAnswer;
      }

      //------------------------------------------------------------------------
      //! Treat the kXR_oksofar response as a valid answer to the message
      //! and notify the handler with the URL as a response
      //------------------------------------------------------------------------
      void SetOksofarAsAnswer( bool oksofarAsAnswer )
      {
        pOksofarAsAnswer = oksofarAsAnswer;
      }

      //------------------------------------------------------------------------
      //! Get the request pointer
      //------------------------------------------------------------------------
      const Message *GetRequest() const
      {
        return pRequest;
      }

      //------------------------------------------------------------------------
      //! Set the load balancer
      //------------------------------------------------------------------------
      void SetLoadBalancer( const HostInfo &loadBalancer )
      {
        if( !loadBalancer.url.IsValid() )
          return;
        pLoadBalancer    = loadBalancer;
        pHasLoadBalancer = true;
      }

      //------------------------------------------------------------------------
      //! Set host list
      //------------------------------------------------------------------------
      void SetHostList( HostList *hostList )
      {
        delete pHosts;
        pHosts = hostList;
      }

      //------------------------------------------------------------------------
      //! Set the chunk list
      //------------------------------------------------------------------------
      void SetChunkList( ChunkList *chunkList )
      {
        pChunkList = chunkList;
        if( chunkList )
          pChunkStatus.resize( chunkList->size() );
        else
          pChunkStatus.clear();
      }

      void SetCrc32cDigests( std::vector<uint32_t> && crc32cDigests )
      {
        pCrc32cDigests = std::move( crc32cDigests );
      }

      //------------------------------------------------------------------------
      //! Set the kernel buffer
      //------------------------------------------------------------------------
      void SetKernelBuffer( XrdSys::KernelBuffer *kbuff )
      {
        pKBuff = kbuff;
      }

      //------------------------------------------------------------------------
      //! Set the redirect counter
      //------------------------------------------------------------------------
      void SetRedirectCounter( uint16_t redirectCounter )
      {
        pRedirectCounter = redirectCounter;
      }

      void SetFollowMetalink( bool followMetalink )
      {
        pFollowMetalink = followMetalink;
      }

      void SetStateful( bool stateful )
      {
        pStateful = stateful;
      }

      //------------------------------------------------------------------------
      //! Bookkeeping after partial response has been received:
      //! - take down the timeout fence after oksofar response has been handled
      //! - reset status-response-body marshaled flag
      //------------------------------------------------------------------------
      void PartialReceived();

    private:

      //------------------------------------------------------------------------
      //! Handle a kXR_read in raw mode
      //------------------------------------------------------------------------
      Status ReadRawRead( Message  *msg,
                          Socket   *socket,
                          uint32_t &bytesRead );

//      //------------------------------------------------------------------------
//      //! Handle a kXR_pgread in raw mode
//      //------------------------------------------------------------------------
//      Status ReadRawPgRead( Message  *msg,
//                            Socket   *socket,
//                            uint32_t &bytesRead );

      //------------------------------------------------------------------------
      //! Handle a kXR_readv in raw mode
      //------------------------------------------------------------------------
      Status ReadRawReadV( Message  *msg,
                           Socket   *socket,
                           uint32_t &bytesRead );

      //------------------------------------------------------------------------
      //! Handle anything other than kXR_read and kXR_readv in raw mode
      //------------------------------------------------------------------------
      Status ReadRawOther( Message  *msg,
                           Socket   *socket,
                           uint32_t &bytesRead );

      //------------------------------------------------------------------------
      //! Read a buffer asynchronously - depends on pAsyncBuffer, pAsyncSize
      //! and pAsyncOffset
      //------------------------------------------------------------------------
      inline Status ReadAsync( Socket *socket, uint32_t &bytesRead )
      {
        uint32_t toBeRead = pAsyncReadSize - pAsyncOffset;
        uint32_t btsRead  = 0;
        Status st = ReadBytesAsync( socket, pAsyncReadBuffer, toBeRead, btsRead );
        pAsyncOffset += btsRead;
        bytesRead    += btsRead;
        return st;
      }

//      //------------------------------------------------------------------------
//      //! Read all page asynchronously - depends on pAsyncBuffer,
//      //! pAsyncSize and pAsyncOffset
//      //------------------------------------------------------------------------
//      inline Status ReadPagesAsync( Socket *socket, uint32_t &bytesRead )
//      {
//        uint32_t toBeRead = pAsyncReadSize - pAsyncOffset;
//        while( toBeRead > 0 )
//        {
//          uint32_t btsRead = 0;
//          Status st = ReadPageAsync( socket, btsRead );
//          if( !st.IsOK() ) return st;
//          bytesRead += btsRead;
//          toBeRead  -= btsRead;
//          if( st.code == suRetry ) return st;
//        }
//
//        return Status();
//      }

      //------------------------------------------------------------------------
      //! Read a single page asynchronously - depends on pAsyncBuffer,
      //! pAsyncSize and pAsyncOffset
      //------------------------------------------------------------------------
//      Status ReadPageAsync( Socket *socket, uint32_t &bytesRead );

      //------------------------------------------------------------------------
      //! Read a buffer asynchronously
      //------------------------------------------------------------------------
      static Status ReadBytesAsync( Socket   *socket,   char     *&buffer,
                                    uint32_t  toBeRead, uint32_t  &bytesRead );

      //------------------------------------------------------------------------
      //! Recover error
      //------------------------------------------------------------------------
      void HandleError( XRootDStatus status, Message *msg = 0 );

      //------------------------------------------------------------------------
      //! Retry the request at another server
      //------------------------------------------------------------------------
      Status RetryAtServer( const URL &url, RedirectEntry::Type entryType );

      //------------------------------------------------------------------------
      //! Unpack the message and call the response handler
      //------------------------------------------------------------------------
      void HandleResponse();

      //------------------------------------------------------------------------
      //! Extract the status information from the stuff that we got
      //------------------------------------------------------------------------
      XRootDStatus *ProcessStatus();

      //------------------------------------------------------------------------
      //! Parse the response and put it in an object that could be passed to
      //! the user
      //------------------------------------------------------------------------
      Status ParseResponse( AnyObject *&response );

      //------------------------------------------------------------------------
      //! Parse the response to kXR_fattr request and put it in an object that
      //! could be passed to the user
      //------------------------------------------------------------------------
      Status ParseXAttrResponse( char *data, size_t len, AnyObject *&response );

      //------------------------------------------------------------------------
      //! Perform the changes to the original request needed by the redirect
      //! procedure - allocate new streamid, append redirection data and such
      //------------------------------------------------------------------------
      Status RewriteRequestRedirect( const URL &newUrl );

      //------------------------------------------------------------------------
      //! Some requests need to be rewritten also after getting kXR_wait - sigh
      //------------------------------------------------------------------------
      Status RewriteRequestWait();

      //------------------------------------------------------------------------
      //! Post process vector read
      //------------------------------------------------------------------------
      Status PostProcessReadV( VectorReadInfo *vReadInfo );

      //------------------------------------------------------------------------
      //! Unpack a single readv response
      //------------------------------------------------------------------------
      Status UnPackReadVResponse( Message *msg );

      //------------------------------------------------------------------------
      //! Update the "tried=" part of the CGI of the current message
      //------------------------------------------------------------------------
      void UpdateTriedCGI(uint32_t errNo=0);

      //------------------------------------------------------------------------
      //! Switch on the refresh flag for some requests
      //------------------------------------------------------------------------
      void SwitchOnRefreshFlag();

      //------------------------------------------------------------------------
      //! If the current thread is a worker thread from our thread-pool
      //! handle the response, otherwise submit a new task to the thread-pool
      //------------------------------------------------------------------------
      void HandleRspOrQueue();

      //------------------------------------------------------------------------ 
      //! Handle a redirect to a local file
      //------------------------------------------------------------------------
      void HandleLocalRedirect( URL *url );

      //------------------------------------------------------------------------
      //! Check if it is OK to retry this request
      //!
      //! @param   reuqest : the request in question
      //! @return          : true if yes, false if no
      //------------------------------------------------------------------------
      bool IsRetriable( Message *request );

      //------------------------------------------------------------------------
      //! Check if for given request and Metalink redirector  it is OK to omit
      //! the kXR_wait and proceed stright to the next entry in the Metalink file
      //!
      //! @param   reuqest : the request in question
      //! @param   url     : metalink URL
      //! @return          : true if yes, false if no
      //------------------------------------------------------------------------
      bool OmitWait( Message *request, const URL &url );

      //------------------------------------------------------------------------
      //! Checks if the given error returned by server is retriable.
      //!
      //! @param   status  : the status returned by the server
      //! @return          : true if the load-balancer is a MetaManager and the
      //!                    error is retriable for MetaManagers
      //------------------------------------------------------------------------
      bool RetriableErrorResponse( const Status &status );

      //------------------------------------------------------------------------
      //! Dump the redirect-trace-back into the log file
      //------------------------------------------------------------------------
      void DumpRedirectTraceBack();
      
      //! Read data from buffer
      //!
      //! @param buffer : the buffer with data
      //! @param size   : the size of the buffer
      //! @param result : output parameter (data read)
      //! @return       : status of the operation
      //------------------------------------------------------------------------
      template<typename T>
      Status ReadFromBuffer( char *&buffer, size_t &buflen, T& result );

      //------------------------------------------------------------------------
      //! Read a string from buffer
      //!
      //! @param buffer : the buffer with data
      //! @param size   : the size of the buffer
      //! @param result : output parameter (data read)
      //! @return       : status of the operation
      //------------------------------------------------------------------------
      Status ReadFromBuffer( char *&buffer, size_t &buflen, std::string &result );

      //------------------------------------------------------------------------
      //! Read a string from buffer
      //!
      //! @param buffer : the buffer with data
      //! @param buflen : size of the buffer
      //! @param size   : size of the data to read
      //! @param result : output parameter (data read)
      //! @return       : status of the operation
      //------------------------------------------------------------------------
      Status ReadFromBuffer( char *&buffer, size_t &buflen, size_t size,
                             std::string &result );

      //------------------------------------------------------------------------
      // Helper struct for async reading of chunks
      //------------------------------------------------------------------------
      struct ChunkStatus
      {
        ChunkStatus(): sizeError( false ), done( false ) {}
        bool sizeError;
        bool done;
      };

      typedef std::list<std::unique_ptr<RedirectEntry>> RedirectTraceBack;

      static const size_t             CksumSize      = sizeof( uint32_t );
      static const size_t             PageWithCksum  = XrdSys::PageSize + CksumSize;
      static const size_t             MaxSslErrRetry = 3;

      inline static size_t NbPgPerRsp( uint64_t offset, uint32_t dlen )
      {
        uint32_t pgcnt = 0;
        uint32_t remainder = offset % XrdSys::PageSize;
        if( remainder > 0 )
        {
          // account for the first unaligned page
          ++pgcnt;
          // the size of the 1st unaligned page
          uint32_t _1stpg = XrdSys::PageSize - remainder;
          offset += _1stpg;
          dlen   -= _1stpg + CksumSize;
        }
        pgcnt += dlen / PageWithCksum;
        if( dlen % PageWithCksum ) ++ pgcnt;
        return pgcnt;
      }

      inline void Copy( uint32_t offchlst, char *buffer, size_t length )
      {
        auto itr = pChunkList->begin();
        while( length > 0 )
        {
          // first find the right buffer
          char   *dstbuf = nullptr;
          size_t  cplen = 0;
          for( ; itr != pChunkList->end() ; ++itr )
          {
            if( offchlst < itr->offset ||
                offchlst >= itr->offset + itr->length )
              continue;
            size_t dstoff = offchlst - itr->offset;
            dstbuf = reinterpret_cast<char*>( itr->buffer ) + dstoff;
            cplen = itr->length - cplen;
            break;
          }
          // now do the copy
          if( cplen > length ) cplen = length;
          memcpy( dstbuf, buffer, cplen );
          buffer += cplen;
          length -= cplen;
        }
      }

      Message                        *pRequest;
      Message                        *pResponse;
      std::vector<Message *>          pPartialResps;
      ResponseHandler                *pResponseHandler;
      URL                             pUrl;
      URL                            *pEffectiveDataServerUrl;
      PostMaster                     *pPostMaster;
      std::shared_ptr<SIDManager>     pSidMgr;
      LocalFileHandler               *pLFileHandler;
      XRootDStatus                    pStatus;
      Status                          pLastError;
      time_t                          pExpiration;
      bool                            pRedirectAsAnswer;
      bool                            pOksofarAsAnswer;
      HostList                       *pHosts;
      bool                            pHasLoadBalancer;
      HostInfo                        pLoadBalancer;
      bool                            pHasSessionId;
      std::string                     pRedirectUrl;
      ChunkList                      *pChunkList;
      std::vector<uint32_t>           pCrc32cDigests;
      XrdSys::KernelBuffer           *pKBuff;
      std::vector<ChunkStatus>        pChunkStatus;
      uint16_t                        pRedirectCounter;
      uint16_t                        pNotAuthorizedCounter;

      uint32_t                        pAsyncOffset;
      uint32_t                        pAsyncChunkOffset;
      uint32_t                        pAsyncChunkIndex;
      uint32_t                        pAsyncReadSize;
      char*                           pAsyncReadBuffer;
      uint32_t                        pAsyncMsgSize;

      bool                            pReadRawStarted;
      uint32_t                        pReadRawCurrentOffset;

//      Buffer                          pPgReadCksumBuff;
//      uint64_t                        pPgReadOffset;
//      uint32_t                        pPgReadLength;
//      uint32_t                        pPgReadCurrentPageSize;
      std::unique_ptr<AsyncPageReader> pPageReader;

      Buffer                          pPgWrtCksumBuff;
      uint32_t                        pPgWrtCurrentPageOffset;
      uint32_t                        pPgWrtCurrentPageNb;

      uint32_t                        pReadVRawMsgOffset;
      bool                            pReadVRawChunkHeaderDone;
      bool                            pReadVRawChunkHeaderStarted;
      bool                            pReadVRawSizeError;
      int32_t                         pReadVRawChunkIndex;
      readahead_list                  pReadVRawChunkHeader;
      bool                            pReadVRawMsgDiscard;

      bool                            pOtherRawStarted;

      bool                            pFollowMetalink;

      bool                            pStateful;
      int                             pAggregatedWaitTime;

      std::unique_ptr<RedirectEntry>  pRdirEntry;
      RedirectTraceBack               pRedirectTraceBack;

      bool                            pMsgInFly;

      //------------------------------------------------------------------------
      // true if MsgHandler is both in inQueue and installed in respective
      // Stream (this could happen if server gave oksofar response), otherwise
      // false
      //------------------------------------------------------------------------
      std::atomic<bool>               pTimeoutFence;

      //------------------------------------------------------------------------
      // if we are serving chunked data to the user's handler in case of
      // kXR_dirlist we need to memorize if the response contains stat info or
      // not (the information is only encoded in the first chunk)
      //------------------------------------------------------------------------
      bool                            pDirListStarted;
      bool                            pDirListWithStat;

      //------------------------------------------------------------------------
      // synchronization is needed in case the MsgHandler has been configured
      // to serve kXR_oksofar as a response to the user's handler
      //------------------------------------------------------------------------
      XrdSysCondVar                   pCV;

      //------------------------------------------------------------------------
      // Count of consecutive `errTlsSslError` errors
      //------------------------------------------------------------------------
      size_t                          pSslErrCnt;

      //------------------------------------------------------------------------
      // Keep track if respective parts of kXR_status response have been
      // unmarshaled.
      //------------------------------------------------------------------------
      bool                            pRspStatusBodyUnMarshaled;
      bool                            pRspPgWrtRetrnsmReqUnMarshalled;
  };
}

#endif // __XRD_CL_XROOTD_MSG_HANDLER_HH__
