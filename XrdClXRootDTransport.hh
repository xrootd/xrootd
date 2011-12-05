//------------------------------------------------------------------------------
// Copyright (c) 2011 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#ifndef __XRD_CL_XROOTD_TRANSPORT_HH__
#define __XRD_CL_XROOTD_TRANSPORT_HH__

#include "XrdCl/XrdClPostMaster.hh"
#include "XProtocol/XProtocol.hh"

namespace XrdClient
{
  //----------------------------------------------------------------------------
  //! Information holder for xrootd channels
  //----------------------------------------------------------------------------
  struct XRootDChannelInfo
  {
    XRootDChannelInfo(): serverFlags(0), protocolVersion(0) {}
    uint32_t serverFlags;
    uint32_t protocolVersion;
    uint8_t  sessionId[16];
  };

  //----------------------------------------------------------------------------
  // XRootD transport handler
  //----------------------------------------------------------------------------
  class XRootDTransport: public TransportHandler
  {
    public:
      //------------------------------------------------------------------------
      //! Read a message from the socket, the socket is non blocking, so if
      //! there is not enough data the function should retutn errRetry in which
      //! case it will be called again when more data arrives with the data
      //! previousely read stored in the message buffer
      //!
      //! @param message the message
      //! @param socket  the socket
      //! @return        stOK if the message has been processed properly,
      //!                stError & errRetry when the method needs to be called
      //!                again to finish reading the message
      //!                stError on faiure
      //------------------------------------------------------------------------
      virtual Status GetMessage( Message *message, Socket *socket );

      //------------------------------------------------------------------------
      //! Initialize channel
      //------------------------------------------------------------------------
      virtual void InitializeChannel( void *&channelData );

      //------------------------------------------------------------------------
      //! Finalize channel
      //------------------------------------------------------------------------
      virtual void FinalizeChannel( void *&channelData );

      //------------------------------------------------------------------------
      //! HandShake
      //------------------------------------------------------------------------
      virtual Status HandShake( Socket    *socket,
                                const URL &url,
                                uint16_t   streamNumber,
                                void      *channelData );

      //------------------------------------------------------------------------
      //! Disconnect
      //------------------------------------------------------------------------
      virtual Status Disconnect( Socket   *socket,
                                 uint16_t  streamNumber,
                                 void     *channelData );

      //------------------------------------------------------------------------
      //! Return a stream number by which the message should be sent and/or
      //! alter the message to include the info by which stream the response
      //! should be sent back
      //------------------------------------------------------------------------
      virtual uint16_t Multiplex( Message *msg, void *channelData );

      //------------------------------------------------------------------------
      //! Return the information whether a control connection needs to be
      //! valid before establishing other connections
      //------------------------------------------------------------------------
      virtual bool NeedControlConnection()
      {
        return true;
      }

      //------------------------------------------------------------------------
      //! Marshal the outgoing message
      //------------------------------------------------------------------------
      static Status Marshall( Message *msg );

      //------------------------------------------------------------------------
      //! Unmarshal the body of the incomming message
      //------------------------------------------------------------------------
      static Status UnMarshallBody( Message *msg, XRequestTypes reqType );

      //------------------------------------------------------------------------
      //! Unmarshal the header incomming message
      //------------------------------------------------------------------------
      static void UnMarshallHeader( Message *msg );

      //------------------------------------------------------------------------
      //! Log server error response
      //------------------------------------------------------------------------
      static void LogErrorResponse( const Message &msg );

    private:

      //------------------------------------------------------------------------
      //! Perform the initial handshake
      //------------------------------------------------------------------------
      Status InitialHS( Socket            *socket,
                        const URL         &url,
                        uint16_t           streamNumber,
                        XRootDChannelInfo *info );

      //------------------------------------------------------------------------
      //! Log in
      //------------------------------------------------------------------------
      Status LogIn( Socket            *socket,
                    const URL         &url,
                    uint16_t           streamNumber,
                    XRootDChannelInfo *info );

      //------------------------------------------------------------------------
      //! Get a string representation of the server flags
      //------------------------------------------------------------------------
      static std::string ServerFlagsToStr( uint32_t flags );

      //------------------------------------------------------------------------
      //! Get a message - blocking
      //------------------------------------------------------------------------
      virtual Status GetMessageBlock( Message *message,
                                      Socket  *socket,
                                      uint16_t timeout );

  };
}

#endif // __XRD_CL_XROOTD_TRANSPORT_HANDLER_HH__
