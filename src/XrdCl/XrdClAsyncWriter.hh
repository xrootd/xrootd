/*
 * XrdClSocketWriter.h
 *
 *  Created on: 3 May 2021
 *      Author: simonm
 */

#ifndef SRC_XRDCL_XRDCLASYNCWRITER_HH_
#define SRC_XRDCL_XRDCLASYNCWRITER_HH_

#include "XrdCl/XrdClStatus.hh"
#include "XrdCl/XrdClSocket.hh"
#include "XrdCl/XrdClMessage.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClConstants.hh"

namespace XrdCl
{

  class AsyncWriter
  {
    public:

      AsyncWriter( Socket &socket,
                   const std::string &streamName ) : socket( socket ),
                                                     streamName( streamName ),
                                                     status( stOK, suNotStarted )
      {
      }

      virtual ~AsyncWriter()
      {
      }

      Status Write()
      {
        if( !status.IsOK() ) return status;
        switch( status.code )
        {
          case suDone :        return status;
          case suPartial :     return status;
          case suAlreadyDone : return status;
          case suNotStarted :  return ( status = WriteImpl() );
          case suRetry :       return ( status = WriteImpl() );
          case suContinue :    return ( status = WriteImpl() );
          default:             return status;
        }
      }

    protected:

      virtual Status WriteImpl() = 0;

      Socket      &socket;
      std::string  streamName;
      Status       status;
  };

  class MsgWriter : public AsyncWriter
  {
    public:
      MsgWriter( Socket            &socket,
                 const std::string &streamName ) : AsyncWriter( socket, streamName ),
                                                   msg( nullptr )
      {
      }

      inline bool HasMsg()
      {
        return bool( msg );
      }

      void Reset( Message *msg = nullptr )
      {
        this->msg.reset( msg );
        status = Status( stOK, suNotStarted );
      }

      Status WriteImpl()
      {
        if( !msg ) return status;
        Log *log = DefaultEnv::GetLog();

        //--------------------------------------------------------------------------
        // Try to write down the current message
        //--------------------------------------------------------------------------
        size_t leftToBeWritten = msg->GetSize()-msg->GetCursor();
        while( leftToBeWritten )
        {
          int bytesWritten = 0;
          status = socket.Send( msg->GetBufferAtCursor(), leftToBeWritten, bytesWritten );
          if( !status.IsOK() )
          {
            msg->SetCursor( 0 );
            return status;
          }
          if( status.code == suRetry ) return status;
          msg->AdvanceCursor( bytesWritten );
          leftToBeWritten -= bytesWritten;
        }

        //--------------------------------------------------------------------------
        // We have written the message successfully
        //--------------------------------------------------------------------------
        log->Dump( AsyncSockMsg, "[%s] Wrote a message: %s (0x%x), %d bytes",
                   streamName.c_str(), msg->GetDescription().c_str(),
                   msg.get(), msg->GetSize() );
        return XRootDStatus();
      }

    private:
      std::unique_ptr<Message> msg;
  };

  class ChunkWriter : public AsyncWriter // TODO
  {

  };

  class KBuffWriter : public AsyncWriter // TODO
  {

  };

} /* namespace XrdCl */

#endif /* SRC_XRDCL_XRDCLASYNCWRITER_HH_ */
