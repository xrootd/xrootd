//------------------------------------------------------------------------------
// author: Lukasz Janyst <ljanyst@cern.ch>
// desc:   Socket test
//------------------------------------------------------------------------------

#include <cppunit/extensions/HelperMacros.h>
#include <cstdlib>
#include <ctime>
#include "Server.hh"
#include "XrdCl/XrdClSocket.hh"
#include "XrdCl/XrdClUtils.hh"
#include "Utils.hh"

//------------------------------------------------------------------------------
// Client handler
//------------------------------------------------------------------------------
class RandomHandler: public ClientHandler
{
  public:
    virtual void HandleConnection( int socket, XrdClient::Log *log )
    {
      XrdClient::ScopedDescriptor scopetDesc( socket );

      //------------------------------------------------------------------------
      // Pump some data
      //------------------------------------------------------------------------
      uint8_t  packets = random() % 100;
      uint16_t packetSize;
      char     buffer[50000];
      log->Debug( 1, "Sending %d packets to the client", packets );

      if( ::write( socket, &packets, 1 ) != 1 )
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

        if( ::write( socket, &packetSize, 2 ) != 2 )
        {
          log->Error( 1, "Unable to send the packet size" );
          return;
        }
        if( ::write( socket, buffer, packetSize ) != packetSize )
        {
          log->Error( 1, "Unable to send the %d bytes of random data",
                      packetSize );
          return;
        }
      }

      //------------------------------------------------------------------------
      // Receive some data
      //------------------------------------------------------------------------
      ssize_t  totalRead;
      char    *current;

      if( ::read( socket, &packets, 1 ) != 1 )
      {
        log->Error( 1, "Unable to receive the packet count" );
        return;
      }

      log->Debug( 1, "Receivng %d packets from the client", packets );

      for( int i = 0; i < packets; ++i )
      {
        totalRead = 0;
        current   = buffer;
        if( ::read( socket, &packetSize, 2 ) != 2 )
        {
          log->Error( 1, "Unable to receive the packet size" );
          return;
        }

        while(1)
        {
          ssize_t dataRead  = ::read( socket, current, packetSize );
          if( dataRead <= 0 )
          {
            log->Error( 1, "Unable to receive the %d bytes of data",
                        packetSize );
            return;
          }

          totalRead += dataRead;
          current   += dataRead;
          if( totalRead == packetSize )
            break;
        }
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
// Declaration
//------------------------------------------------------------------------------
class SocketTest: public CppUnit::TestCase
{
  public:
    CPPUNIT_TEST_SUITE( SocketTest );
      CPPUNIT_TEST( TransferTest );
    CPPUNIT_TEST_SUITE_END();
    void TransferTest();
};

CPPUNIT_TEST_SUITE_REGISTRATION( SocketTest );

//------------------------------------------------------------------------------
// Test the transfer
//------------------------------------------------------------------------------
void SocketTest::TransferTest()
{
  using namespace XrdClient;
  srandom( time(0) );
  Server serv;
  Socket sock;

  //----------------------------------------------------------------------------
  // Start up the server and connect to it
  //----------------------------------------------------------------------------
  CPPUNIT_ASSERT( serv.Setup( 9999, 1, new RandomHandlerFactory() ) );
  CPPUNIT_ASSERT( serv.Start() );
  CPPUNIT_ASSERT( sock.Connect( URL( "localhost:9999" ) ).status == stOK );
  CPPUNIT_ASSERT( sock.IsConnected() );

  //----------------------------------------------------------------------------
  // Get the number of packets
  //----------------------------------------------------------------------------
  uint8_t  packets;
  uint32_t bytesTransmitted;
  uint16_t packetSize;
  Status   sc;
  char     buffer[50000];
  sc = sock.ReadRaw( &packets, 1, 60, bytesTransmitted );
  CPPUNIT_ASSERT( sc.status == stOK );

  //----------------------------------------------------------------------------
  // Read each packet
  //----------------------------------------------------------------------------

  for( int i = 0; i < packets; ++i )
  {
    sc = sock.ReadRaw( &packetSize, 2, 60, bytesTransmitted );
    CPPUNIT_ASSERT( sc.status == stOK );
    sc = sock.ReadRaw( buffer, packetSize, 60, bytesTransmitted );
    CPPUNIT_ASSERT( sc.status == stOK );
  }

  //----------------------------------------------------------------------------
  // Send the number of packets
  //----------------------------------------------------------------------------
  packets = random() % 100;

  sc = sock.WriteRaw( &packets, 1, 60, bytesTransmitted );
  CPPUNIT_ASSERT( (sc.status == stOK) && (bytesTransmitted == 1) );

  for( int i = 0; i < packets; ++i )
  {
    packetSize = random() % 50000;
    CPPUNIT_ASSERT( ::Utils::GetRandomBytes( buffer, packetSize ) == packetSize );

    sc = sock.WriteRaw( (char *)&packetSize, 2, 60, bytesTransmitted );
    CPPUNIT_ASSERT( (sc.status == stOK) && (bytesTransmitted == 2) );
    sc = sock.WriteRaw( buffer, packetSize, 60, bytesTransmitted );
    CPPUNIT_ASSERT( (sc.status == stOK) && (bytesTransmitted == packetSize) );
  }

  sock.Disconnect();
  CPPUNIT_ASSERT( serv.Stop() );
}
