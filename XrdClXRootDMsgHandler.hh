//------------------------------------------------------------------------------
// Copyright (c) 2011-2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
//------------------------------------------------------------------------------
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
  class XRootDMsgHandler: public MessageHandler,
                          public MessageStatusHandler
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
        pTimeout( 300 ),
        pRedirectAsAnswer( false ),
        pUserBuffer( 0 ),
        pUserBufferSize( 0 )
      {
        pPostMaster   = DefaultEnv::GetPostMaster();
        pRedirections = new ResponseHandler::URLList;
        pRedirections->push_back( *url );
      }

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      ~XRootDMsgHandler()
      {
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
      virtual uint8_t HandleMessage( Message *msg  );

      //------------------------------------------------------------------------
      //! Handle an event other that a message arrival - may be timeout
      //! or stream failure
      //!
      //! @param status info about the fault that occured
      //------------------------------------------------------------------------
      virtual void HandleFault( Status status );

      //------------------------------------------------------------------------
      //! The requested action has been performed and the status is available
      //------------------------------------------------------------------------
      virtual void HandleStatus( const Message *message,
                                 Status         status );

      //------------------------------------------------------------------------
      //! Called after the wait time for kXR_wait has elapsed
      //!
      //! @param  now current timestamp
      //------------------------------------------------------------------------
      void WaitDone( time_t now );

      //------------------------------------------------------------------------
      //! Set timeout
      //------------------------------------------------------------------------
      void SetTimeout( uint16_t timeout )
      {
        pTimeout = timeout;
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

    private:
      //------------------------------------------------------------------------
      // Unpack the message and call the response handler
      //------------------------------------------------------------------------
      void HandleResponse();

      //------------------------------------------------------------------------
      // Extract the status information from the stuff that we got
      //------------------------------------------------------------------------
      XRootDStatus *ProcessStatus();

      //------------------------------------------------------------------------
      // Parse the response and put it in an object that could be passed to
      // the user
      //------------------------------------------------------------------------
      AnyObject *ParseResponse();

      //------------------------------------------------------------------------
      // Perform the changes to the original request needed by the redirect
      // procedure - allocate new streamid, append redirection data and such
      //------------------------------------------------------------------------
      Status RewriteRequestRedirect( const std::string &newCgi );

      //------------------------------------------------------------------------
      // Some requests need to be rewriten also after getting kXR_wait - sigh
      //------------------------------------------------------------------------
      Status RewriteRequestWait();

      //------------------------------------------------------------------------
      // Unpack vector read
      //------------------------------------------------------------------------
      Status UnpackVectorRead( VectorReadInfo *vReadInfo,
                               char           *targetBuffer,
                               uint32_t        targetBufferSize,
                               char           *sourceBuffer,
                               uint32_t        sourceBufferSize );

      Message                  *pRequest;
      Message                  *pResponse;
      std::vector<Message *>    pPartialResps;
      ResponseHandler          *pResponseHandler;
      URL                       pUrl;
      PostMaster               *pPostMaster;
      SIDManager               *pSidMgr;
      Status                    pStatus;
      uint16_t                  pTimeout;
      bool                      pRedirectAsAnswer;
      ResponseHandler::URLList *pRedirections;
      char                     *pUserBuffer;
      uint32_t                  pUserBufferSize;
  };
}

#endif // __XRD_CL_XROOTD_MSG_HANDLER_HH__
