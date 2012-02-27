//------------------------------------------------------------------------------
// Copyright (c) 2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#ifndef __XRD_CL_MESSAGE_UTILS_HH__
#define __XRD_CL_MESSAGE_UTILS_HH__

#include "XrdCl/XrdClXRootDResponses.hh"
#include "XrdCl/XrdClURL.hh"
#include "XrdCl/XrdClMessage.hh"
#include "XrdSys/XrdSysPthread.hh"
#include <memory>

namespace XrdClient
{
  //----------------------------------------------------------------------------
  //! Synchronize the response
  //----------------------------------------------------------------------------
  class SyncResponseHandler: public ResponseHandler
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      SyncResponseHandler(): pStatus(0), pResponse(0), pSem(0) {}

      //------------------------------------------------------------------------
      //! Handle the response
      //------------------------------------------------------------------------
      virtual void HandleResponse( XRootDStatus *status,
                                   AnyObject    *response )
      {
        pStatus = status;
        pResponse = response;
        pSem.Post();
      }

      //------------------------------------------------------------------------
      //! Get the status
      //------------------------------------------------------------------------
      XRootDStatus *GetStatus()
      {
        return pStatus;
      }

      //------------------------------------------------------------------------
      //! Get the response
      //------------------------------------------------------------------------
      AnyObject *GetResponse()
      {
        return pResponse;
      }

      //------------------------------------------------------------------------
      //! Wait for the arrival of the response
      //------------------------------------------------------------------------
      void WaitForResponse()
      {
        pSem.Wait();
      }

    private:
      XRootDStatus    *pStatus;
      AnyObject       *pResponse;
      XrdSysSemaphore  pSem;
  };

  class MessageUtils
  {
    public:
      //------------------------------------------------------------------------
      //! Wait and return the status of the query
      //------------------------------------------------------------------------
      static XRootDStatus WaitForStatus( SyncResponseHandler *handler )
      {
        handler->WaitForResponse();
        XRootDStatus *status = handler->GetStatus();
        XRootDStatus ret( *status );
        delete status;
        return ret;
      }

      //------------------------------------------------------------------------
      //! Wait for the response
      //------------------------------------------------------------------------
      template<class Type>
      static XrdClient::XRootDStatus WaitForResponse(
                            SyncResponseHandler  *handler,
                            Type                *&response )
      {
        handler->WaitForResponse();

        std::auto_ptr<AnyObject> resp( handler->GetResponse() );
        XRootDStatus *status = handler->GetStatus();
        XRootDStatus ret( *status );
        delete status;

        if( ret.IsOK() )
        {
          if( !resp.get() )
            return XRootDStatus( stError, errInternal );
          resp->Get( response );
          resp->Set( (int *)0 );
          if( !response )
            return XRootDStatus( stError, errInternal );
        }

        return ret;
      }

      //------------------------------------------------------------------------
      //! Create a message
      //------------------------------------------------------------------------
      template<class Type>
      static void CreateRequest( Message  *&msg,
                                 Type     *&req,
                                 uint32_t  payloadSize = 0 )
      {
        msg = new Message( sizeof(Type)+payloadSize );
        req = (Type*)msg->GetBuffer();
        msg->Zero();
      }

      //------------------------------------------------------------------------
      //! Send message
      //------------------------------------------------------------------------
      static Status SendMessage( const URL       &url,
                                 Message         *msg,
                                 ResponseHandler *handler,
                                 uint16_t         timeout,
                                 bool             followRedirects = true );

  };
}

#endif // __XRD_CL_MESSAGE_UTILS_HH__
