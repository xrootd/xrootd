//------------------------------------------------------------------------------
// Copyright (c) 2023 by European Organization for Nuclear Research (CERN)
// Author: Angelo Galavotti <agalavottib@gmail.com>
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

#include <gtest/gtest.h>
#include <cstdlib>
#include <ctime>
#include <random>
#include <chrono>
#include "Server.hh"
#include "Utils.hh"
#include "TestEnv.hh"
#include "XrdCl/XrdClSocket.hh"
#include "XrdCl/XrdClUtils.hh"

using namespace XrdClTests;

//------------------------------------------------------------------------------
// Mock socket for testing
//------------------------------------------------------------------------------
struct MockSocket : public XrdCl::Socket
{
  public:

    MockSocket() : size( sizeof( ServerResponseHeader ) + sizeof( ServerResponseBody_Protocol ) ),
                   buffer( reinterpret_cast<char*>( &response ) ), offset( 0 ),
                   random_engine( std::chrono::system_clock::now().time_since_epoch().count() ),
                   retrygen( 0, 9 ),
                   retry_threshold( retrygen( random_engine ) )
    {
      response.hdr.status = kXR_ok;
      response.hdr.streamid[0] = 1;
      response.hdr.streamid[1] = 2;
      response.hdr.dlen = htonl( sizeof( ServerResponseBody_Protocol ) );

      response.body.protocol.flags = 123;
      response.body.protocol.pval  = 4567;
      response.body.protocol.secreq.rsvd   = 'A';
      response.body.protocol.secreq.seclvl = 'B';
      response.body.protocol.secreq.secopt = 'C';
      response.body.protocol.secreq.secver = 'D';
      response.body.protocol.secreq.secvsz = 'E';
      response.body.protocol.secreq.theTag = 'F';
      response.body.protocol.secreq.secvec.reqindx = 'G';
      response.body.protocol.secreq.secvec.reqsreq = 'H';
    }

    virtual XrdCl::XRootDStatus Read( char *outbuf, size_t rdsize, int &bytesRead )
    {
      size_t btsleft = size - offset;
      if( btsleft == 0 || nodata() )
        return XrdCl::XRootDStatus( XrdCl::stOK, XrdCl::suRetry );

      if( rdsize > btsleft )
        rdsize = btsleft;

      std::uniform_int_distribution<size_t> sizegen( 0, rdsize );
      rdsize = sizegen( random_engine );

      if( rdsize == 0 )
        return XrdCl::XRootDStatus( XrdCl::stOK, XrdCl::suRetry );

      memcpy( outbuf, buffer + offset, rdsize );
      offset += rdsize;
      bytesRead = rdsize;

      return XrdCl::XRootDStatus();
    }

    virtual XrdCl::XRootDStatus Send( const char *buffer, size_t size, int &bytesWritten )
    {
      return XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errNotSupported );
    }

    inline bool IsEqual( XrdCl::Message &msg )
    {
      response.hdr.dlen = ntohl( response.hdr.dlen );
      bool ok = ( memcmp( msg.GetBuffer(), &response, size ) == 0 );
      response.hdr.dlen = htonl( response.hdr.dlen );
      return ok;
    }

  private:

    inline bool nodata()
    {
      size_t doretry = retrygen( random_engine );
      return doretry > retry_threshold;
    }

    ServerResponse response;
    const size_t size;
    char *buffer;
    size_t offset;

    std::default_random_engine random_engine;
    std::uniform_int_distribution<size_t> retrygen;
    const size_t retry_threshold;
};

//------------------------------------------------------------------------------
// Client handler
//------------------------------------------------------------------------------
class RandomHandler: public ClientHandler
{
  public:
    virtual void HandleConnection( int socket )
    {
      XrdCl::ScopedDescriptor scopedDesc( socket );
      XrdCl::Log *log = TestEnv::GetLog();

      //------------------------------------------------------------------------
      // Pump some data
      //------------------------------------------------------------------------
      uint8_t  packets = random() % 100;
      uint16_t packetSize;
      char     buffer[50000];
      log->Debug( 1, "Sending %d packets to the client", packets );

      if( ::Utils::Write( socket, &packets, 1 ) != 1 )
      {
        log->Error( 1, "Unable to send the packet count" );
        return;
      }

      for( int i = 0; i < packets; ++i )
      {
        packetSize = random() % 50000;
        log->Dump( 1, "Sending %d packet, %d bytes of data", i, packetSize );
        if( Utils::GetRandomBytes( buffer, packetSize ) != packetSize )
        {
          log->Error( 1, "Unable to get %d bytes of random data", packetSize );
          return;
        }

        if( ::Utils::Write( socket, &packetSize, 2 ) != 2 )
        {
          log->Error( 1, "Unable to send the packet size" );
          return;
        }
        if( ::Utils::Write( socket, buffer, packetSize ) != packetSize )
        {
          log->Error( 1, "Unable to send the %d bytes of random data",
                      packetSize );
          return;
        }
        UpdateSentData( buffer, packetSize );
      }

      //------------------------------------------------------------------------
      // Receive some data
      //------------------------------------------------------------------------
      if( ::Utils::Read( socket, &packets, 1 ) != 1 )
      {
        log->Error( 1, "Unable to receive the packet count" );
        return;
      }

      log->Debug( 1, "Receivng %d packets from the client", packets );

      for( int i = 0; i < packets; ++i )
      {
        if( ::Utils::Read( socket, &packetSize, 2 ) != 2 )
        {
          log->Error( 1, "Unable to receive the packet size" );
          return;
        }

        if ( ::Utils::Read( socket, buffer, packetSize ) != packetSize )
        {
          log->Error( 1, "Unable to receive the %d bytes of data",
                      packetSize );
          return;
        }
        UpdateReceivedData( buffer, packetSize );
        log->Dump( 1, "Received %d bytes from the client", packetSize );
      }
    }
};

//------------------------------------------------------------------------------
// Client handler factory
//------------------------------------------------------------------------------
class RandomHandlerFactory: public ClientHandlerFactory
{
  public:
    virtual ClientHandler *CreateHandler()
    {
      return new RandomHandler();
    }
};

//------------------------------------------------------------------------------
// SocketTest class declaration
//------------------------------------------------------------------------------
class SocketTest : public ::testing::Test {};

//------------------------------------------------------------------------------
// Test the transfer
//------------------------------------------------------------------------------
TEST(SocketTest, TransferTest)
{
  using namespace XrdCl;
  srandom( time(0) );
  Server serv( Server::Both );
  Socket sock;

  //----------------------------------------------------------------------------
  // Start up the server and connect to it
  //----------------------------------------------------------------------------
  uint16_t port = 9998; // was 9999, but we need to change ports from other
                    // tests so that we can run all of them in parallel. 
                    // Will find another, better way to ensure this in the future 
  EXPECT_TRUE( serv.Setup( port, 1, new RandomHandlerFactory() ) );
  EXPECT_TRUE( serv.Start() );

  EXPECT_EQ( sock.GetStatus(), Socket::Disconnected );
  EXPECT_TRUE( sock.Initialize( AF_INET6 ).IsOK() );
  EXPECT_TRUE( sock.Connect( "localhost", port ).IsOK() );
  EXPECT_EQ( sock.GetStatus(), Socket::Connected );

  //----------------------------------------------------------------------------
  // Get the number of packets
  //----------------------------------------------------------------------------
  uint8_t  packets;
  uint32_t bytesTransmitted;
  uint16_t packetSize;
  Status   sc;
  char     buffer[50000];
  uint64_t sentCounter = 0;
  uint32_t sentChecksum = ::Utils::ComputeCRC32( 0, 0 );
  uint64_t receivedCounter = 0;
  uint32_t receivedChecksum = ::Utils::ComputeCRC32( 0, 0 );
  sc = sock.ReadRaw( &packets, 1, 60, bytesTransmitted );
  EXPECT_EQ( sc.status, stOK );

  //----------------------------------------------------------------------------
  // Read each packet
  //----------------------------------------------------------------------------
  for( int i = 0; i < packets; ++i )
  {
    sc = sock.ReadRaw( &packetSize, 2, 60, bytesTransmitted );
    EXPECT_EQ( sc.status, stOK );
    sc = sock.ReadRaw( buffer, packetSize, 60, bytesTransmitted );
    EXPECT_EQ( sc.status, stOK );
    receivedCounter += bytesTransmitted;
    receivedChecksum = ::Utils::UpdateCRC32( receivedChecksum, buffer,
                                             bytesTransmitted );
  }

  //----------------------------------------------------------------------------
  // Send the number of packets
  //----------------------------------------------------------------------------
  packets = random() % 100;

  sc = sock.WriteRaw( &packets, 1, 60, bytesTransmitted );
  EXPECT_EQ( sc.status, stOK );
  EXPECT_EQ( bytesTransmitted, 1 );

  for( int i = 0; i < packets; ++i )
  {
    packetSize = random() % 50000;
    EXPECT_EQ( ::Utils::GetRandomBytes( buffer, packetSize ), packetSize );

    sc = sock.WriteRaw( (char *)&packetSize, 2, 60, bytesTransmitted );
    EXPECT_EQ( sc.status, stOK );
    EXPECT_EQ( bytesTransmitted, 2 );
    sc = sock.WriteRaw( buffer, packetSize, 60, bytesTransmitted );
    EXPECT_EQ( sc.status, stOK );
    EXPECT_EQ( bytesTransmitted, packetSize );
    sentCounter += bytesTransmitted;
    sentChecksum = ::Utils::UpdateCRC32( sentChecksum, buffer,
                                         bytesTransmitted );
  }

  //----------------------------------------------------------------------------
  // Check the counters and the checksums
  //----------------------------------------------------------------------------
  std::string socketName = sock.GetSockName();

  sock.Close();
  EXPECT_TRUE( serv.Stop() );

  std::pair<uint64_t, uint32_t> sent     = serv.GetSentStats( socketName );
  std::pair<uint64_t, uint32_t> received = serv.GetReceivedStats( socketName );
  EXPECT_EQ( sentCounter, received.first );
  EXPECT_EQ( receivedCounter, sent.first );
  EXPECT_EQ( sentChecksum, received.second );
  EXPECT_EQ( receivedChecksum, sent.second );
}
