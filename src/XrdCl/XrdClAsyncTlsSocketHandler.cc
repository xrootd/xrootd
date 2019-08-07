//------------------------------------------------------------------------------
// Copyright (c) 2011-2012 by European Organization for Nuclear Research (CERN)
// Author: Michal Simon <simonm@cern.ch>
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

#include "XrdCl/XrdClAsyncTlsSocketHandler.hh"
#include "XrdCl/XrdClXRootDMsgHandler.hh"
#include "XrdCl/XrdClXRootDTransport.hh"
#include "XrdCl/XrdClTls.hh"
#include "XrdCl/XrdClStream.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClTlsSocket.hh"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

namespace XrdCl
{
  //----------------------------------------------------------------------------
  // Constructor
  //----------------------------------------------------------------------------
  AsyncTlsSocketHandler::AsyncTlsSocketHandler( Poller           *poller,
                                                TransportHandler *transport,
                                                AnyObject        *channelData,
                                                uint16_t          subStreamNum ):
    AsyncSocketHandler( poller, transport, channelData, subStreamNum ),
    pTransport( transport ),
    pWrtHdrDone( false )
  {
  }

  //----------------------------------------------------------------------------
  // Destructor
  //----------------------------------------------------------------------------
  AsyncTlsSocketHandler::~AsyncTlsSocketHandler()
  {
  }

  //----------------------------------------------------------------------------
  // Handler a socket event
  //----------------------------------------------------------------------------
  void AsyncTlsSocketHandler::Event( uint8_t type, XrdCl::Socket *socket )
  {
    type = pSocket->MapEvent( type );
    AsyncSocketHandler::Event( type, socket );
  }

  //----------------------------------------------------------------------------
  // Connect returned
  //----------------------------------------------------------------------------
  void AsyncTlsSocketHandler::OnConnectionReturn()
  {
    AsyncSocketHandler::OnConnectionReturn();

    if( pSocket->GetStatus() == Socket::Connected )
    {
      //------------------------------------------------------------------------
      // Upgrade socket to TLS
      //------------------------------------------------------------------------
      // what to do with status ??? todo
      pSocket->EnableEncryption( this );
    }
  }

  //----------------------------------------------------------------------------
  // Got a write readiness event
  //----------------------------------------------------------------------------
  void AsyncTlsSocketHandler::OnWrite()
  {
    //--------------------------------------------------------------------------
    // Pick up a message if we're not in process of writing something
    //--------------------------------------------------------------------------
    if( !pOutgoing )
    {
      pOutMsgDone = false;
      std::pair<Message *, OutgoingMsgHandler *> toBeSent;
      toBeSent = pStream->OnReadyToWrite( pSubStreamNum );
      pOutgoing = toBeSent.first; pOutHandler = toBeSent.second;

      if( !pOutgoing )
        return;

      pOutgoing->SetCursor( 0 );
      pOutMsgSize = pOutgoing->GetSize();
      pWrtHdrDone = false;
    }

    if( !pWrtHdrDone )
    {
      Status st = WriteCurrentMessage( pOutgoing );
      if( !st.IsOK() )
      {
        OnFault( st );
        return;
      }

      if( st.code == suRetry ) return;

      pWrtHdrDone = true;
    }

    if( pOutHandler->IsRaw() )
    {
      uint32_t bytesWritten = 0;
      Status st = pOutHandler->WriteMessageBody( pSocket, bytesWritten );

      if( !st.IsOK() )
      {
        OnFault( st );
        return;
      }

      if( st.code == suRetry ) return;
    }

    //----------------------------------------------------------------------------
    // Send everything with one TCP frame if possible
    //----------------------------------------------------------------------------
    Status st = pSocket->Flash();
    if( !st.IsOK() )
    {
      Log *log = DefaultEnv::GetLog();
      log->Error( AsyncSockMsg, "[%s] Unable to flash the socket: %s",
                  pStreamName.c_str(), strerror( errno ) );
      OnFault( st );
      return;
    }

    Log *log = DefaultEnv::GetLog();
    log->Dump( AsyncSockMsg, "[%s] Successfully sent message: %s (0x%x).",
               pStreamName.c_str(), pOutgoing->GetDescription().c_str(),
               pOutgoing );

    pStream->OnMessageSent( pSubStreamNum, pOutgoing, pOutMsgSize );
    pOutgoing = 0;

    //--------------------------------------------------------------------------
    // Disable the respective substream if empty
    //--------------------------------------------------------------------------
    pStream->DisableIfEmpty( pSubStreamNum );
  }

  //----------------------------------------------------------------------------
  // Got a write readiness event while handshaking
  //----------------------------------------------------------------------------
  void AsyncTlsSocketHandler::OnWriteWhileHandshaking()
  {
    Status st;
    if( !pHSOutgoing )
    {
      if( !(st = DisableUplink()).IsOK() )
        OnFaultWhileHandshaking( st );
      return;
    }

    st = WriteCurrentMessage( pHSOutgoing );
    if( !st.IsOK() )
    {
      OnFaultWhileHandshaking( st );
      return;
    }

    if( st.code == suRetry ) return;

    delete pHSOutgoing;
    pHSOutgoing = 0;

    st = pSocket->Flash();
    if( !st.IsOK() )
    {
      Log *log = DefaultEnv::GetLog();
      log->Error( AsyncSockMsg, "[%s] Unable to flash the socket: %s",
                  pStreamName.c_str(), strerror( st.errNo ) );
      OnFaultWhileHandshaking( st );
    }

    if( !( st = DisableUplink() ).IsOK() )
      OnFaultWhileHandshaking( st );
  }

  //----------------------------------------------------------------------------
  // Write the current message
  //----------------------------------------------------------------------------
  Status AsyncTlsSocketHandler::WriteCurrentMessage( Message *toWrite )
  {
    Log *log = DefaultEnv::GetLog();

    //--------------------------------------------------------------------------
    // Try to write down the current message
    //--------------------------------------------------------------------------
    Message  *msg             = toWrite;
    uint32_t  leftToBeWritten = msg->GetSize()-msg->GetCursor();

    while( leftToBeWritten )
    {
      int bytesWritten = 0;
      Status status = pSocket->Send( msg->GetBufferAtCursor(), leftToBeWritten, bytesWritten );

      //------------------------------------------------------------------------
      // Writing operation would block! So we are done for now, but we will
      // return
      //------------------------------------------------------------------------
      if( status.IsOK() && status.code == suRetry )
        return status;

      if( !status.IsOK() )
      {
        //----------------------------------------------------------------------
        // Actual tls error error!
        //----------------------------------------------------------------------
        toWrite->SetCursor( 0 );
        return status;
      }

      msg->AdvanceCursor( bytesWritten );
      leftToBeWritten -= bytesWritten;
    }

    //--------------------------------------------------------------------------
    // We have written the message successfully
    //--------------------------------------------------------------------------
    log->Dump( AsyncSockMsg, "[%s] Wrote a message: %s (0x%x), %d bytes",
               pStreamName.c_str(), toWrite->GetDescription().c_str(),
               toWrite, toWrite->GetSize() );
    return Status();
  }

  //----------------------------------------------------------------------------
  // Got a read readiness event
  //----------------------------------------------------------------------------
  void AsyncTlsSocketHandler::OnRead()
  {
    //--------------------------------------------------------------------------
    // There is no incoming message currently being processed so we create
    // a new one
    //--------------------------------------------------------------------------
    if( !pIncoming )
    {
      pHeaderDone  = false;
      pIncoming    = new Message();
      pIncHandler  = std::make_pair( (IncomingMsgHandler*)0, false );
      pIncMsgSize  = 0;
    }

    Status  st;
    Log    *log = DefaultEnv::GetLog();

    //--------------------------------------------------------------------------
    // We need to read the header first
    //--------------------------------------------------------------------------
    if( !pHeaderDone )
    {
      st = pTransport->GetHeader( pIncoming, pSocket );
      if( !st.IsOK() )
      {
        OnFault( st );
        return;
      }

      if( st.code == suRetry ) return;

      log->Dump( AsyncSockMsg, "[%s] Received message header for 0x%x size: %d",
                pStreamName.c_str(), pIncoming, pIncoming->GetCursor() );
      pIncMsgSize = pIncoming->GetCursor();
      pHeaderDone = true;
      std::pair<IncomingMsgHandler *, bool> raw;
      pIncHandler = pStream->InstallIncHandler( pIncoming, pSubStreamNum );

      if( pIncHandler.first )
      {
        log->Dump( AsyncSockMsg, "[%s] Will use the raw handler to read body "
                   "of message 0x%x", pStreamName.c_str(), pIncoming );
      }
    }

    //--------------------------------------------------------------------------
    // We need to call a raw message handler to get the data from the socket
    //--------------------------------------------------------------------------
    if( pIncHandler.first )
    {
      uint32_t bytesRead = 0;
      st = pIncHandler.first->ReadMessageBody( pIncoming, pSocket, bytesRead );
      if( !st.IsOK() )
      {
        OnFault( st );
        return;
      }
      pIncMsgSize += bytesRead;

      if( st.code == suRetry ) return;
    }
    //--------------------------------------------------------------------------
    // No raw handler, so we read the message to the buffer
    //--------------------------------------------------------------------------
    else
    {
      st = pTransport->GetBody( pIncoming, pSocket );
      if( !st.IsOK() )
      {
        OnFault( st );
        return;
      }

      if( st.code == suRetry ) return;

      pIncMsgSize = pIncoming->GetSize();
    }

    //--------------------------------------------------------------------------
    // Report the incoming message
    //--------------------------------------------------------------------------
    log->Dump( AsyncSockMsg, "[%s] Received message 0x%x of %d bytes",
               pStreamName.c_str(), pIncoming, pIncMsgSize );

    pStream->OnIncoming( pSubStreamNum, pIncoming, pIncMsgSize );
    pIncoming = 0;
  }

  //----------------------------------------------------------------------------
  // Got a read readiness event while handshaking
  //----------------------------------------------------------------------------
  void AsyncTlsSocketHandler::OnReadWhileHandshaking()
  {
    //--------------------------------------------------------------------------
    // Read the message and let the transport handler look at it when
    // reading has finished
    //--------------------------------------------------------------------------
    Status st = ReadMessage( pHSIncoming );
    if( !st.IsOK() )
    {
      OnFaultWhileHandshaking( st );
      return;
    }

    if( st.code == suRetry ) return;

    AsyncSocketHandler::HandleHandShake();
  }

  //----------------------------------------------------------------------------
  // Read a message
  //----------------------------------------------------------------------------
  Status AsyncTlsSocketHandler::ReadMessage( Message *&toRead )
  {
    if( !toRead )
    {
      pHeaderDone = false;
      toRead      = new Message();
    }

    Status  st;
    Log    *log = DefaultEnv::GetLog();
    if( !pHeaderDone )
    {
      st = pTransport->GetHeader( toRead, pSocket );
      if( st.IsOK() && st.code == suDone )
      {
        log->Dump( AsyncSockMsg,
                  "[%s] Received message header, size: %d",
                  pStreamName.c_str(), toRead->GetCursor() );
        pHeaderDone = true;
      }
      else
        return st;
    }

    st = pTransport->GetBody( toRead, pSocket );
    if( st.IsOK() && st.code == suDone )
    {
      log->Dump( AsyncSockMsg, "[%s] Received a message of %d bytes",
                 pStreamName.c_str(), toRead->GetSize() );
    }
    return st;
  }
}

