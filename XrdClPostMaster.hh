//------------------------------------------------------------------------------
// Author: Lukasz Janyst <ljanyst@cern.ch>
//------------------------------------------------------------------------------

#ifndef __XRD_CL_POST_MASTER_HH__
#define __XRD_CL_POST_MASTER_HH__

#include <stdint.h>
#include <map>

#include "XrdCl/XrdClStatus.hh"
#include "XrdCl/XrdClURL.hh"
#include "XrdCl/XrdClPoller.hh"

namespace XrdClient
{
  class Channel;
  class Message;

  //----------------------------------------------------------------------------
  //! Message filter
  //----------------------------------------------------------------------------
  class MessageFilter
  {
    public:
      //------------------------------------------------------------------------
      //! Examine the message and return true if the message should be picked
      //! up (usually removed from the queue and to the caller)
      //------------------------------------------------------------------------
      virtual bool Filter( const Message *msg ) = 0;
  };

  //----------------------------------------------------------------------------
  //! Message listener
  //----------------------------------------------------------------------------
  class MessageListener
  {
    public:
      //------------------------------------------------------------------------
      //! Examine the message
      //!
      //! @param msg the message
      //! @return    true if you want to take the ownership of the message, false
      //!            if it is of no interest
      //------------------------------------------------------------------------
      virtual bool HandleMessage( const Message *msg ) = 0;
  };

  //----------------------------------------------------------------------------
  //! Message status listener
  //----------------------------------------------------------------------------
  class MessageStatusListener
  {
    public:
      //------------------------------------------------------------------------
      //! The requested action has been performed and the status is available
      //------------------------------------------------------------------------
      virtual void HandleStatus( const Message *message,
                                 Status         status ) = 0;
  };

  //----------------------------------------------------------------------------
  //! Perform the handshake and the authentication for each physical stream
  //----------------------------------------------------------------------------
  class TransportHandler
  {
    public:
      //------------------------------------------------------------------------
      //! Size of the data header. The value is used to determine how many
      //! initial bytes should be read from the stream before the message
      //! size can be determined
      //------------------------------------------------------------------------
      uint16_t HeaderSize;

      //------------------------------------------------------------------------
      //! Define how many more bytes should be read from the stream to
      //! reconstruct a complete message
      //!
      //! @param message the part of the message that has been reconstructed
      //!                so far not smaller than HeaderSize
      //! @param size    size of the message
      //! @return        number of bytes still to be read to finish the
      //!                message reconstruction
      //------------------------------------------------------------------------
      virtual int32_t GetMessageSize( const char *message, uint32_t size ) = 0;

      //------------------------------------------------------------------------
      //! Create a new stream in the channel
      //------------------------------------------------------------------------
      virtual Status CreateStream( Channel *channel ) = 0;

      //------------------------------------------------------------------------
      //! Close one of the streams
      //------------------------------------------------------------------------
      virtual Status CloseStream( Channel *channel )  = 0;

      //------------------------------------------------------------------------
      //! Return a stream number by which the message should be sent and/or
      //! alter the message to include the info by which stream the response
      //! should be sent back
      //------------------------------------------------------------------------
      virtual uint16_t Multiplex( Message *msg ) = 0;
  };

  //----------------------------------------------------------------------------
  //! A communication channel between the client and the server
  //----------------------------------------------------------------------------
  class Channel
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @param url        address of the server to connect to
      //! @param poller     poller object to be used for non-blocking IO
      //! @param transport  protocol speciffic transport handler
      //------------------------------------------------------------------------
      Channel( URL              &url,
               Poller           *poller,
               TransportHandler *transport );

      //------------------------------------------------------------------------
      //! Add a new physical stream
      //------------------------------------------------------------------------
      Status AddStream( Socket *socket );

      //------------------------------------------------------------------------
      //! Close a stream associated to a given index
      //------------------------------------------------------------------------
      Status CloseStream( uint16_t num );

      //------------------------------------------------------------------------
      //! Get number of streams
      //------------------------------------------------------------------------
      uint16_t GetNumStreams() const;

      //------------------------------------------------------------------------
      //! Get the URL
      //------------------------------------------------------------------------
      const URL &GetURL() const;

      //------------------------------------------------------------------------
      //! Send a message synchronously - synchronously means that
      //! it will block until the message is written to a socket
      //!
      //! @param msg    message to be sent
      //! @param timout timout after which a failure should be reported if
      //!               sending was unsuccessful
      //! @return       success if the message has been pushed through the wire,
      //!               failure otherwise
      //------------------------------------------------------------------------
      Status Send( Message *msg, int32_t timeout );

      //------------------------------------------------------------------------
      //! Send the message asynchronously - the message is inserted into the
      //! send queue and a listener is called when the message is successuly
      //! pushed through the wire or when the timeout elapses
      //!
      //! @param msg      message to be sent
      //! @param timeout  timeout after which a failure is reported to the
      //!                 listener
      //! @param listener listener to be notified about the status
      //! @return         success if the message was successfuly inserted
      //!                 into the send quees, failure otherwise
      //------------------------------------------------------------------------
      Status Send( Message               *msg,
                   int32_t                timeout,
                   MessageStatusListener *listener );

      //------------------------------------------------------------------------
      //! Synchronously receive a message - blocks until a message maching
      //! a filter is found in the incomming queue or the timout passes
      //!
      //! @param msg     message buffer
      //! @param filter  filter object defining what to look for
      //! @param timeout timeout
      //! @return        success when the message has been received
      //!                successfuly, failure otherwise
      //------------------------------------------------------------------------
      Status Receive( Message *msg, MessageFilter *filter, uint16_t timeout );

      //------------------------------------------------------------------------
      //! Listen to incomming messages, the listener is notified when a new
      //! message arrives and when the timeout passes
      //!
      //! @param listener listener to be notified about new messages
      //! @param timeout  timout
      //! @return         success when the listener has been inserted correctly
      //------------------------------------------------------------------------
      Status Receive( MessageListener *listener, uint16_t timeout );
    private:
      URL url;
  };

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
      //! @param url      recipient of the message
      //! @param msg      message to be sent
      //! @param timeout  timeout after which a failure is reported to the
      //!                 listener
      //! @param listener listener to be notified about the status
      //! @return         success if the message was successfuly inserted
      //!                 into the send quees, failure otherwise
      //------------------------------------------------------------------------
      Status Send( const URL             &url,
                   Message               *msg,
                   MessageStatusListener *listener,
                   uint16_t               timeout );

      //------------------------------------------------------------------------
      //! Synchronously receive a message - blocks until a message maching
      //! a filter is found in the incomming queue or the timout passes
      //!
      //! @param url     sender of the message
      //! @param msg     message buffer
      //! @param filter  filter object defining what to look for
      //! @param timeout timeout
      //! @return        success when the message has been received
      //!                successfuly, failure otherwise
      //------------------------------------------------------------------------
      Status Receive( const URL     &url,
                      Message       *msg,
                      MessageFilter *filter,
                      uint16_t       timeout );

      //------------------------------------------------------------------------
      //! Listen to incomming messages, the listener is notified when a new
      //! message arrives and when the timeout passes
      //!
      //! @param url      sender of the message
      //! @param listener listener to be notified about new messages
      //! @param timeout  timout
      //! @return         success when the listener has been inserted correctly
      //------------------------------------------------------------------------
      Status Receive( const URL       &url,
                      MessageListener *listener,
                      uint16_t         timeout );
  };
}

#endif // __XRD_CL_POST_MASTER_HH__
