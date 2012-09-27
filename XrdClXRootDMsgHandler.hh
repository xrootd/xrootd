//------------------------------------------------------------------------------
// Copyright (c) 2011-2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
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

#ifndef __XRD_CL_XROOTD_MSG_HANDLER_HH__
#define __XRD_CL_XROOTD_MSG_HANDLER_HH__

#include "XrdCl/XrdClPostMasterInterfaces.hh"
#include "XrdCl/XrdClXRootDResponses.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClMessage.hh"

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
        pUserBuffer( 0 ),
        pUserBufferSize( 0 ),
        pHasLoadBalancer( false ),
        pStateful( false ),
        pCalledBack( false )
      {
        pPostMaster = DefaultEnv::GetPostMaster();
      }

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      ~XRootDMsgHandler()
      {
        if( !pStateful )
          delete pRequest;
        delete pResponse;
        std::vector<Message *>::iterator it;
        for( it = pPartialResps.begin(); it != pPartialResps.end(); ++it )
          delete *it;
      }

      //------------------------------------------------------------------------
      //! Examine an incomming message, and decide on the action to be taken
      //!
      //! @param msg    the message, may be zero if receive failed
      //! @return       action type that needs to be take wrt the message and
      //!               the handler
      //------------------------------------------------------------------------
      virtual uint8_t OnIncoming( Message *msg  );


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
      //! Set user buffer that will be used as a destination for read and
      //! readv requests
      //------------------------------------------------------------------------
      void SetUserBuffer( char *buffer, uint32_t size )
      {
        pUserBuffer     = buffer;
        pUserBufferSize = size;
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
      //! Set stateful status
      //------------------------------------------------------------------------
      void SetStateful( bool stateful )
      {
        pStateful = stateful;
      }

      //------------------------------------------------------------------------
      //! Set host list
      //------------------------------------------------------------------------
      void SetHostList( HostList *hostList )
      {
        delete pHosts;
        pHosts = hostList;
      }

    private:
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
      AnyObject *ParseResponse();

      //------------------------------------------------------------------------
      //! Perform the changes to the original request needed by the redirect
      //! procedure - allocate new streamid, append redirection data and such
      //------------------------------------------------------------------------
      Status RewriteRequestRedirect( const URL::ParamsMap &newCgi );

      //------------------------------------------------------------------------
      //! Some requests need to be rewriten also after getting kXR_wait - sigh
      //------------------------------------------------------------------------
      Status RewriteRequestWait();

      //------------------------------------------------------------------------
      //! Unpack vector read
      //------------------------------------------------------------------------
      Status UnpackVectorRead( VectorReadInfo *vReadInfo,
                               char           *targetBuffer,
                               uint32_t        targetBufferSize,
                               char           *sourceBuffer,
                               uint32_t        sourceBufferSize );

      //------------------------------------------------------------------------
      //! Update the "tried=" part of the CGI of the current message
      //------------------------------------------------------------------------
      void UpdateTriedCGI();

      //------------------------------------------------------------------------
      //! Switch on the refresh flag for some requests
      //------------------------------------------------------------------------
      void SwitchOnRefreshFlag();

      //------------------------------------------------------------------------
      //! Merge CGI in the request
      //!
      //! @param newCgi  the new cgi
      //! @param replace indicates whether, in case of a conflict, the new CGI
      //!                parameter should replace an existing one or be
      //!                appended to it using a comma
      //------------------------------------------------------------------------
      void MergeCGI( const URL::ParamsMap &newCgi, bool replace );

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
      char                      *pUserBuffer;
      uint32_t                   pUserBufferSize;
      bool                       pHasLoadBalancer;
      HostInfo                   pLoadBalancer;
      bool                       pStateful;
      bool                       pCalledBack;
  };
}

#endif // __XRD_CL_XROOTD_MSG_HANDLER_HH__
