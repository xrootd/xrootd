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

namespace XrdCl
{
  class PostMaster;
  class SIDManager;
  class URL;

  //----------------------------------------------------------------------------
  //! Handle/Process/Forward XRootD messages
  //----------------------------------------------------------------------------
  class XRootDMsgHandler: public IncomingMsgHandler,
                          public OutgoingMsgHandler
  {
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
      XRootDMsgHandler( Message         *msg,
                        ResponseHandler *respHandler,
                        const URL       *url,
                        SIDManager      *sidMgr ):
        pRequest( msg ),
        pResponse( 0 ),
        pResponseHandler( respHandler ),
        pUrl( *url ),
        pSidMgr( sidMgr ),
        pExpiration( 0 ),
        pRedirectAsAnswer( false ),
        pHosts( 0 ),
        pHasLoadBalancer( false ),
        pHasSessionId( false ),
        pChunkList( 0 ),
        pRedirectCounter( 0 ),

        pAsyncOffset( 0 ),
        pAsyncReadSize( 0 ),
        pAsyncReadBuffer( 0 ),
        pAsyncMsgSize( 0 ),

        pReadRawStarted( false ),
        pReadRawCurrentOffset( 0 ),

        pReadVRawMsgOffset( 0 ),
        pReadVRawChunkHeaderDone( false ),
        pReadVRawChunkHeaderStarted( false ),
        pReadVRawSizeError( false ),
        pReadVRawChunkIndex( 0 ),
        pReadVRawMsgDiscard( false ),

        pOtherRawStarted( false )
      {
        pPostMaster = DefaultEnv::GetPostMaster();
        if( msg->GetSessionId() )
          pHasSessionId = true;
        memset( &pReadVRawChunkHeader, 0, sizeof( readahead_list ) );
      }

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      ~XRootDMsgHandler()
      {
        if( !pHasSessionId )
          delete pRequest;
        delete pResponse;
        std::vector<Message *>::iterator it;
        for( it = pPartialResps.begin(); it != pPartialResps.end(); ++it )
          delete *it;
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
      virtual Status ReadMessageBody( Message  *msg,
                                      int       socket,
                                      uint32_t &bytesRead );

      //------------------------------------------------------------------------
      //! Handle an event other that a message arrival
      //!
      //! @param event     type of the event
      //! @param streamNum stream concerned
      //! @param status    status info
      //------------------------------------------------------------------------
      virtual uint8_t OnStreamEvent( StreamEvent event,
                                     uint16_t    streamNum,
                                     Status      status );

      //------------------------------------------------------------------------
      //! The requested action has been performed and the status is available
      //------------------------------------------------------------------------
      virtual void OnStatusReady( const Message *message,
                                  Status         status );

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
      virtual Status WriteMessageBody( int       socket,
                                       uint32_t &bytesRead );

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

      //------------------------------------------------------------------------
      //! Set the redirect counter
      //------------------------------------------------------------------------
      void SetRedirectCounter( uint16_t redirectCounter )
      {
        pRedirectCounter = redirectCounter;
      }

    private:
      //------------------------------------------------------------------------
      //! Handle a kXR_read in raw mode
      //------------------------------------------------------------------------
      Status ReadRawRead( Message  *msg,
                          int       socket,
                          uint32_t &bytesRead );

      //------------------------------------------------------------------------
      //! Handle a kXR_readv in raw mode
      //------------------------------------------------------------------------
      Status ReadRawReadV( Message  *msg,
                           int       socket,
                           uint32_t &bytesRead );

      //------------------------------------------------------------------------
      //! Handle anything other than kXR_read and kXR_readv in raw mode
      //------------------------------------------------------------------------
      Status ReadRawOther( Message  *msg,
                           int       socket,
                           uint32_t &bytesRead );

      //------------------------------------------------------------------------
      //! Read a buffer asynchronously - depends on pAsyncBuffer, pAsyncSize
      //! and pAsyncOffset
      //------------------------------------------------------------------------
      Status ReadAsync( int socket, uint32_t &btesRead );

      //------------------------------------------------------------------------
      //! Recover error
      //------------------------------------------------------------------------
      void HandleError( Status status, Message *msg = 0 );

      //------------------------------------------------------------------------
      //! Retry the request at another server
      //------------------------------------------------------------------------
      Status RetryAtServer( const URL &url );

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
      //! Perform the changes to the original request needed by the redirect
      //! procedure - allocate new streamid, append redirection data and such
      //------------------------------------------------------------------------
      Status RewriteRequestRedirect( const URL::ParamsMap &newCgi,
                                     const std::string    &newPath );

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
      void UpdateTriedCGI();

      //------------------------------------------------------------------------
      //! Switch on the refresh flag for some requests
      //------------------------------------------------------------------------
      void SwitchOnRefreshFlag();

      //------------------------------------------------------------------------
      // Helper struct for async reading of chunks
      //------------------------------------------------------------------------
      struct ChunkStatus
      {
        ChunkStatus(): sizeError( false ), done( false ) {}
        bool sizeError;
        bool done;
      };

      Message                   *pRequest;
      Message                   *pResponse;
      std::vector<Message *>     pPartialResps;
      ResponseHandler           *pResponseHandler;
      URL                        pUrl;
      PostMaster                *pPostMaster;
      SIDManager                *pSidMgr;
      Status                     pStatus;
      time_t                     pExpiration;
      bool                       pRedirectAsAnswer;
      HostList                  *pHosts;
      bool                       pHasLoadBalancer;
      HostInfo                   pLoadBalancer;
      bool                       pHasSessionId;
      std::string                pRedirectUrl;
      ChunkList                 *pChunkList;
      std::vector<ChunkStatus>   pChunkStatus;
      uint16_t                   pRedirectCounter;

      uint32_t                   pAsyncOffset;
      uint32_t                   pAsyncReadSize;
      char*                      pAsyncReadBuffer;
      uint32_t                   pAsyncMsgSize;

      bool                       pReadRawStarted;
      uint32_t                   pReadRawCurrentOffset;

      uint32_t                   pReadVRawMsgOffset;
      bool                       pReadVRawChunkHeaderDone;
      bool                       pReadVRawChunkHeaderStarted;
      bool                       pReadVRawSizeError;
      int32_t                    pReadVRawChunkIndex;
      readahead_list             pReadVRawChunkHeader;
      bool                       pReadVRawMsgDiscard;

      bool                       pOtherRawStarted;
  };
}

#endif // __XRD_CL_XROOTD_MSG_HANDLER_HH__
