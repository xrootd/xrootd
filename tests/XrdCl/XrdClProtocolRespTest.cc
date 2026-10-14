//------------------------------------------------------------------------------
// Copyright (c) 2026 by European Organization for Nuclear Research (CERN)
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

//------------------------------------------------------------------------------
// The client must survive a rogue server that sends a malformed kXR_protocol
// response during the handshake, before any authentication takes place.
//------------------------------------------------------------------------------

#include <gtest/gtest.h>

#include "Utils.hh"
#include "XProtocol/XProtocol.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClFileSystem.hh"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace
{
  //----------------------------------------------------------------------------
  // Server that answers the handshake, then sends a fixed kXR_protocol
  // response header and body, then closes the connection
  //----------------------------------------------------------------------------
  class RogueServer
  {
    public:
      RogueServer( uint32_t dlen, std::vector<char> body ) :
        dlen( dlen ), body( std::move( body ) ), done( false )
      {
        lsock = socket( AF_INET, SOCK_STREAM, 0 );
        sockaddr_in addr{};
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = htonl( INADDR_LOOPBACK );
        addr.sin_port        = 0;
        socklen_t len = sizeof( addr );
        if( bind( lsock, (sockaddr*)&addr, len ) != 0 || listen( lsock, 8 ) != 0 ||
            getsockname( lsock, (sockaddr*)&addr, &len ) != 0 )
          port = 0;
        else
          port = ntohs( addr.sin_port );
        thread = std::thread( &RogueServer::Run, this );
      }

      ~RogueServer()
      {
        done = true;
        thread.join();
        close( lsock );
      }

      int GetPort() const { return port; }

    private:
      void Run()
      {
        while( !done )
        {
          pollfd pfd{ lsock, POLLIN, 0 };
          if( poll( &pfd, 1, 100 ) <= 0 )
            continue;
          int fd = accept( lsock, nullptr, nullptr );
          if( fd < 0 )
            continue;
          Serve( fd );
          close( fd );
        }
      }

      void Serve( int fd )
      {
        //----------------------------------------------------------------------
        // Initial handshake (20 bytes) followed by kXR_protocol
        //----------------------------------------------------------------------
        char req[20 + sizeof( ClientProtocolRequest )];
        if( XrdClTests::Utils::Read( fd, req, sizeof( req ) ) != (ssize_t)sizeof( req ) )
          return;

        struct
        {
          ServerResponseHeader hdr;
          ServerInitHandShake  hs;
        } hsResp{};
        hsResp.hdr.dlen    = htonl( 8 );
        hsResp.hs.protover = htonl( kXR_PROTOCOLVERSION );
        hsResp.hs.msgval   = htonl( kXR_DataServer );
        XrdClTests::Utils::Write( fd, &hsResp, 16 );

        ServerResponseHeader hdr{};
        memcpy( hdr.streamid, req + 20, 2 );
        hdr.status = htons( kXR_ok );
        hdr.dlen   = htonl( dlen );
        XrdClTests::Utils::Write( fd, &hdr, sizeof( hdr ) );
        XrdClTests::Utils::Write( fd, body.data(), body.size() );
      }

      uint32_t          dlen;
      std::vector<char> body;
      std::atomic<bool> done;
      int               lsock;
      int               port;
      std::thread       thread;
  };

  //----------------------------------------------------------------------------
  // Build a kXR_protocol body: pval, flags, and a security requirements
  // header with the given secvsz, padded with zeros to the given size
  //----------------------------------------------------------------------------
  std::vector<char> ProtocolBody( uint8_t secvsz, size_t size )
  {
    std::vector<char> body( size, 0 );
    uint32_t pval  = htonl( kXR_PROTOCOLVERSION );
    uint32_t flags = htonl( kXR_isServer );
    memcpy( body.data(),     &pval,  4 );
    memcpy( body.data() + 4, &flags, 4 );
    body[8]  = 'S';
    body[13] = secvsz;
    return body;
  }

  XrdCl::XRootDStatus StatRogue( const RogueServer &server )
  {
    XrdCl::Env *env = XrdCl::DefaultEnv::GetEnv();
    env->PutInt( "ConnectionRetry", 1 );
    env->PutInt( "ConnectionWindow", 2 );
    env->PutInt( "RequestTimeout", 5 );
    env->PutInt( "StreamErrorWindow", 0 );

    XrdCl::URL url( "root://127.0.0.1:" + std::to_string( server.GetPort() ) + "/" );
    XrdCl::FileSystem fs( url );
    XrdCl::StatInfo *info = nullptr;
    XrdCl::XRootDStatus st = fs.Stat( "/file", info, 10 );
    delete info;
    return st;
  }
}

//------------------------------------------------------------------------------
// A security requirements tail much larger than the declared vector must not
// be copied past the stored protocol response
//------------------------------------------------------------------------------
TEST( ProtocolRespTest, OversizedSecReqs )
{
  const size_t size = 8 + 8 + 4096;
  RogueServer server( size, ProtocolBody( 0, size ) );
  ASSERT_NE( server.GetPort(), 0 );
  EXPECT_FALSE( StatRogue( server ).IsOK() );
}

//------------------------------------------------------------------------------
// A security vector longer than the response body must be rejected
//------------------------------------------------------------------------------
TEST( ProtocolRespTest, TruncatedSecVec )
{
  const size_t size = 8 + 6;
  RogueServer server( size, ProtocolBody( 10, size ) );
  ASSERT_NE( server.GetPort(), 0 );
  EXPECT_FALSE( StatRogue( server ).IsOK() );
}
