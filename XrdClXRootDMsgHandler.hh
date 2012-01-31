//------------------------------------------------------------------------------
// Copyright (c) 2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#ifndef __XRD_CL_XROOTD_MSG_HANDLER_HH__
#define __XRD_CL_XROOTD_MSG_HANDLER_HH__

#include "XrdCl/XrdClPostMasterInterfaces.hh"
#include "XrdCl/XrdClXRootDResponses.hh"
#include "XrdCl/XrdClMessage.hh"

namespace XrdClient
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
      //! @param postMaster  the post master object to be used to handle
      //!                    redirections
      //! @param sidMgr      the sid manager used to allocate SID for the initial
      //!                    message
      //! @param timeout     timeout
      //------------------------------------------------------------------------
      XRootDMsgHandler( Message         *msg,
                        ResponseHandler *respHandler,
                        URL             *url,
                        PostMaster      *postMaster,
                        SIDManager      *sidMgr,
                        uint16_t         timeout ):
        pRequest( msg ),
        pResponse( 0 ),
        pResponseHandler( respHandler ),
        pUrl( new URL( url->GetHostId() ) ),
        pPostMaster( postMaster ),
        pSidMgr( sidMgr ),
        pTimeout( timeout )
      {
      }

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      ~XRootDMsgHandler()
      {
        delete pUrl;
        delete pRequest;
        delete pResponse;
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
      Status RewriteRequestRedirect();

      //------------------------------------------------------------------------
      // Some requests need to be rewriten also after getting kXR_wait - sigh
      //------------------------------------------------------------------------
      Status RewriteRequestWait();

      Message                *pRequest;
      Message                *pResponse;
      std::vector<Message *>  pPartialResps;
      ResponseHandler        *pResponseHandler;
      URL                    *pUrl;
      PostMaster             *pPostMaster;
      SIDManager             *pSidMgr;
      Status                  pStatus;
      uint16_t                pTimeout;
  };
}

#endif // __XRD_CL_XROOTD_MSG_HANDLER_HH__
