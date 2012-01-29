//------------------------------------------------------------------------------
// Copyright (c) 2011 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#ifndef __XRD_CL_POST_MASTER_HH__
#define __XRD_CL_POST_MASTER_HH__

#include <stdint.h>
#include <map>
#include <vector>

#include "XrdCl/XrdClStatus.hh"
#include "XrdCl/XrdClURL.hh"
#include "XrdCl/XrdClPoller.hh"
#include "XrdCl/XrdClTaskManager.hh"
#include "XrdCl/XrdClPostMasterInterfaces.hh"
#include "XrdCl/XrdClChannel.hh"

#include "XrdSys/XrdSysPthread.hh"

namespace XrdClient
{
  //----------------------------------------------------------------------------
  //! A hub for dispaching and receiving messages
  //----------------------------------------------------------------------------
  class PostMaster
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //------------------------------------------------------------------------
      PostMaster();

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~PostMaster();

      //------------------------------------------------------------------------
      //! Initializer
      //------------------------------------------------------------------------
      bool Initialize();

      //------------------------------------------------------------------------
      //! Finalizer
      //------------------------------------------------------------------------
      bool Finalize();

      //------------------------------------------------------------------------
      //! Start the post master
      //------------------------------------------------------------------------
      bool Start();

      //------------------------------------------------------------------------
      //! Stop the postmaster
      //------------------------------------------------------------------------
      bool Stop();

      //------------------------------------------------------------------------
      //! Reinitialize after fork
      //------------------------------------------------------------------------
      bool Reinitialize();

      //------------------------------------------------------------------------
      //! Send a message synchronously - synchronously means that
      //! it will block until the message is written to a socket
      //!
      //! @param url    recipient of the message
      //! @param msg    message to be sent
      //! @param timout timout after which a failure should be reported if
      //!               sending was unsuccessful
      //! @return       success if the message has been pushed through the wire,
      //!               failure otherwise
      //------------------------------------------------------------------------
      Status Send( const URL &url, Message *msg, uint16_t timeout );

      //------------------------------------------------------------------------
      //! Send the message asynchronously - the message is inserted into the
      //! send queue and a listener is called when the message is successuly
      //! pushed through the wire or when the timeout elapses
      //!
      //! @param url           recipient of the message
      //! @param msg           message to be sent
      //! @param timeout       timeout after which a failure is reported to the
      //!                      handler
      //! @param statusHandler handler will be notified about the status
      //! @return              success if the message was successfuly inserted
      //!                      into the send quees, failure otherwise
      //------------------------------------------------------------------------
      Status Send( const URL            &url,
                   Message              *msg,
                   MessageStatusHandler *statusHandler,
                   uint16_t              timeout );

      //------------------------------------------------------------------------
      //! Synchronously receive a message - blocks until a message maching
      //! a filter is found in the incomming queue or the timout passes
      //!
      //! @param url     sender of the message
      //! @param msg     reference to a message pointer, the pointer will
      //!                point to the received message
      //! @param filter  filter object defining what to look for
      //! @param timeout timeout
      //! @return        success when the message has been received
      //!                successfuly, failure otherwise
      //------------------------------------------------------------------------
      Status Receive( const URL      &url,
                      Message       *&msg,
                      MessageFilter *filter,
                      uint16_t       timeout );

      //------------------------------------------------------------------------
      //! Listen to incomming messages, the listener is notified when a new
      //! message arrives and when the timeout passes
      //!
      //! @param url      sender of the message
      //! @param listener handler to be notified about new messages
      //! @param timeout  timout
      //! @return         success when the listener has been inserted correctly
      //------------------------------------------------------------------------
      Status Receive( const URL      &url,
                      MessageHandler *handler,
                      uint16_t        timeout );

      //------------------------------------------------------------------------
      //! Query the transport handler for a given URL
      //!
      //! @param url    the channel to be queried
      //! @param query  the query as defined in the TransportQuery struct or
      //!               others that may be recognized by the protocol transport
      //! @param result the result of the query
      //! @return       status of the query
      //------------------------------------------------------------------------
      Status QueryTransport( const URL &url,
                             uint16_t   query,
                             AnyObject &result );

      //------------------------------------------------------------------------
      //! Get the task manager object user by the post master
      //------------------------------------------------------------------------
      TaskManager *GetTaskManager()
      {
        return pTaskManager;
      }

    private:
      Channel *GetChannel( const URL &url );

      typedef std::map<std::string, Channel*> ChannelMap;
      Poller           *pPoller;
      TaskManager      *pTaskManager;
      ChannelMap        pChannelMap;
      XrdSysMutex       pChannelMapMutex;
      TransportHandler *pTransportHandler; // to be removed when protocol
                                           // factory is implemented
  };
}

#endif // __XRD_CL_POST_MASTER_HH__
