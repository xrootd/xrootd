//------------------------------------------------------------------------------
// Copyright (c) 2011 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#ifndef __XRD_CL_POST_CHANNEL_HH__
#define __XRD_CL_POST_CHANNEL_HH__

#include <stdint.h>
#include <vector>
#include <ctime>

#include "XrdCl/XrdClStatus.hh"
#include "XrdCl/XrdClURL.hh"
#include "XrdCl/XrdClPoller.hh"
#include "XrdCl/XrdClInQueue.hh"
#include "XrdCl/XrdClPostMasterInterfaces.hh"
#include "XrdCl/XrdClAnyObject.hh"
#include "XrdCl/XrdClTaskManager.hh"

#include "XrdSys/XrdSysPthread.hh"

namespace XrdClient
{
  class Stream;

  //----------------------------------------------------------------------------
  //! A communication channel between the client and the server
  //----------------------------------------------------------------------------
  class Channel
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @param url         address of the server to connect to
      //! @param poller      poller object to be used for non-blocking IO
      //! @param transport   protocol speciffic transport handler
      //! @param taskManager async task handler to be used by the channel
      //------------------------------------------------------------------------
      Channel( const URL        &url,
               Poller           *poller,
               TransportHandler *transport,
               TaskManager      *taskManager );

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      ~Channel();

      //------------------------------------------------------------------------
      //! Get the URL
      //------------------------------------------------------------------------
      const URL &GetURL() const
      {
        return pUrl;
      }

      //------------------------------------------------------------------------
      //! Send a message synchronously - synchronously means that
      //! it will block until the message is written to a socket
      //!
      //! @param msg     message to be sent
      //! @param timeout timout after which a failure should be reported if
      //!                sending was unsuccessful
      //! @return        success if the message has been pushed through the wire,
      //!                failure otherwise
      //------------------------------------------------------------------------
      Status Send( Message *msg, int32_t timeout );

      //------------------------------------------------------------------------
      //! Send the message asynchronously - the message is inserted into the
      //! send queue and a listener is called when the message is successuly
      //! pushed through the wire or when the timeout elapses
      //!
      //! @param msg     message to be sent
      //! @param timeout timeout after which a failure is reported to the
      //!                listener
      //! @param handler handler to be notified about the status
      //! @return        success if the message was successfuly inserted
      //!                into the send quees, failure otherwise
      //------------------------------------------------------------------------
      Status Send( Message              *msg,
                   MessageStatusHandler *handler,
                   int32_t               timeout );


      //------------------------------------------------------------------------
      //! Synchronously receive a message - blocks until a message maching
      //! a filter is found in the incomming queue or the timout passes
      //!
      //! @param msg     reference to a message pointer, the pointer will
      //!                point to the received message
      //! @param filter  filter object defining what to look for
      //! @param timeout timeout
      //! @return        success when the message has been received
      //!                successfuly, failure otherwise
      //------------------------------------------------------------------------
      Status Receive( Message *&msg, MessageFilter *filter, uint16_t timeout );

      //------------------------------------------------------------------------
      //! Listen to incomming messages, the listener is notified when a new
      //! message arrives and when the timeout passes
      //!
      //! @param handler handler to be notified about new messages
      //! @param timeout timout
      //! @return        success when the handler has been registered correctly
      //------------------------------------------------------------------------
      Status Receive( MessageHandler *handler, uint16_t timeout );

      //------------------------------------------------------------------------
      //! Query the transport handler
      //!
      //! @param query  the query as defined in the TransportQuery struct or
      //!               others that may be recognized by the protocol transport
      //! @param result the result of the query
      //! @return       status of the query
      //------------------------------------------------------------------------
      Status QueryTransport( uint16_t query, AnyObject &result );

      //------------------------------------------------------------------------
      //! Handle a time event
      //------------------------------------------------------------------------
      void Tick( time_t now );

    private:

      URL                    pUrl;
      Poller                *pPoller;
      TransportHandler      *pTransport;
      TaskManager           *pTaskManager;
      std::vector<Stream *>  pStreams;
      XrdSysMutex            pMutex;
      AnyObject              pChannelData;
      InQueue                pIncoming;
      Task                  *pTickGenerator;
  };
}

#endif // __XRD_CL_POST_CHANNEL_HH__
