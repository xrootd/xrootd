//------------------------------------------------------------------------------
// Copyright (c) 2011 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#include "XrdCl/XrdClStream.hh"
#include "XrdCl/XrdClSocket.hh"
#include "XrdCl/XrdClChannel.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClUtils.hh"
#include "XrdCl/XrdClMessage.hh"

namespace XrdClient
{
  //----------------------------------------------------------------------------
  // Message helper
  //----------------------------------------------------------------------------
  struct OutMessageHelper
  {
    OutMessageHelper( Message *message, MessageStatusHandler *hndlr ):
      msg( message ), handler( hndlr )  {}
    Message              *msg;
    MessageStatusHandler *handler;
  };

  //----------------------------------------------------------------------------
  // Constructor
  //----------------------------------------------------------------------------
  Stream::Stream( Channel          *channel,
                  uint16_t          streamNum,
                  TransportHandler *transport,
                  Socket           *socket,
                  Poller           *poller,
                  InQueue          *incoming ):
    pChannel( channel ), pStreamNum( streamNum ), pTransport( transport ),
    pSocket( socket ), pPoller( poller ), pCurrentOut( 0 ),
    pIncomingQueue( incoming ), pIncoming( 0 )
  {
  }

  //----------------------------------------------------------------------------
  // Destructor
  //----------------------------------------------------------------------------
  Stream::~Stream()
  {
  }

  //----------------------------------------------------------------------------
  // Handle a socket event
  //----------------------------------------------------------------------------
  void Stream::Event( uint8_t type, Socket *socket )
  {
    Log *log = Utils::GetDefaultLog();
    switch( type )
    {
      case ReadyToRead:
        ReadMessage();
        break;
      case ReadTimeOut:
        HandleReadTimeout();
        break;
      case ReadyToWrite:
        WriteMessage();
        break;
      case WriteTimeOut:
        HandleWriteTimeout();
        break;
    }
  }

  //----------------------------------------------------------------------------
  // Queue the message for sending
  //----------------------------------------------------------------------------
  Status Stream::QueueOut( Message              *msg,
                           MessageStatusHandler *handler,
                           uint32_t              timeout )
  {
    //--------------------------------------------------------------------------
    // Check if the stream is connected and if it may be reconnected
    //--------------------------------------------------------------------------
    if( !pSocket->IsConnected() )
    {
      Status sc = pChannel->HandleStreamFault( pStreamNum );
      if( sc.status != stOK )
      {
        handler->HandleStatus( msg, sc );
        return sc;
      }
    }

    //--------------------------------------------------------------------------
    // The stream seems to be OK
    //--------------------------------------------------------------------------
    Lock();
    if( pOutQueue.empty() )
      pPoller->EnableWriteNotification( pSocket, true, 15 );

    pOutQueue.push_back( new OutMessageHelper( msg, handler )  );
    UnLock();
    return Status();
  }

  //----------------------------------------------------------------------------
  // Pick a message from the outgoing queue and write it
  //----------------------------------------------------------------------------
  void Stream::WriteMessage()
  {
    Log *log = Utils::GetDefaultLog();
    XrdSysMutexHelper scopedLock( pMutex );

    //--------------------------------------------------------------------------
    // Pick up a message if we're not in process of writing something
    //--------------------------------------------------------------------------
    if( !pCurrentOut && !pOutQueue.empty() )
    {
      pCurrentOut = pOutQueue.front();
      pCurrentOut->msg->SetCursor(0);
      pOutQueue.pop_front();
    }

    //--------------------------------------------------------------------------
    // Try to write down the current message
    //--------------------------------------------------------------------------
    int       sock            = pSocket->GetFD();
    Message  *msg             = pCurrentOut->msg;
    uint32_t  leftToBeWritten = msg->GetSize()-msg->GetCursor();

    while( leftToBeWritten )
    {
      int status = ::write( sock, msg->GetBufferAtCursor(), leftToBeWritten );
      if( status <= 0 )
      {
        //----------------------------------------------------------------------
        // Writing operation would block!
        //----------------------------------------------------------------------
        if( errno == EAGAIN || errno == EWOULDBLOCK )
          return;

        //----------------------------------------------------------------------
        // A stream error that needs to be handled, stream needs to be
        // reconnected so we need to restart from scratch
        //----------------------------------------------------------------------
        // FIXME handle error here!!
        // pChannel->HandleStreamFault( pStreamId );
        pCurrentOut->msg->SetCursor( 0 );
        return;
      }
      msg->AdvanceCursor( status );
      leftToBeWritten -= status;
    }

    //--------------------------------------------------------------------------
    // We have written the message successfully
    //--------------------------------------------------------------------------
    if( pCurrentOut->handler )
    {
      pCurrentOut->handler->HandleStatus( pCurrentOut->msg, Status() );
    }

    delete pCurrentOut;
    pCurrentOut = 0;

    if( pOutQueue.empty() )
    {
      log->Dump( PostMasterMsg, "%s Nothing to write, disable write "
                                "notifications", pSocket->GetName().c_str() );
      pPoller->EnableWriteNotification( pSocket, false );
    }
  }

  //----------------------------------------------------------------------------
  // Read message
  //----------------------------------------------------------------------------
  void Stream::ReadMessage()
  {
    Log *log = Utils::GetDefaultLog();
    if( !pIncoming )
      pIncoming = new Message();

    Status sc = pTransport->GetMessage( pIncoming, pSocket );

    //--------------------------------------------------------------------------
    // The entire message has been read fine
    //--------------------------------------------------------------------------
    if( sc.status == stOK )
    {
      log->Dump( PostMasterMsg, "%s Got a message of %d bytes",
                                pSocket->GetName().c_str(),
                                pIncoming->GetSize() );
      pIncomingQueue->AddMessage( pIncoming );
      pIncoming = 0;
      return;
    }

    //--------------------------------------------------------------------------
    // An error has occured while reading the message
    //--------------------------------------------------------------------------
    if( sc.IsError() && sc.errorType != errRetry )
    {
      delete pIncoming;
      pIncoming = 0;
      // handle stream error here
    }
  }

  //----------------------------------------------------------------------------
  // Handle read timeout
  //----------------------------------------------------------------------------
  void Stream::HandleReadTimeout()
  {
  }

  //----------------------------------------------------------------------------
  // Handle write timeout
  //----------------------------------------------------------------------------
  void Stream::HandleWriteTimeout()
  {
  }
}
