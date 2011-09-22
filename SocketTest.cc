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

      if( write( socket, &packets, 1 ) != 1 )
      {
        log->Error( 1, "Unable to send the packet number" );
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

        if( write( socket, (char *)&packetSize, 2 ) != 2 )
        {
          log->Error( 1, "Unable to send the %d bytes of random data",
                      packetSize );
          return;
        }
        if( write( socket, buffer, packetSize ) != packetSize )
        {
          log->Error( 1, "Unable to send the %d bytes of random data",
                      packetSize );
          return;
        }
      }

      //------------------------------------------------------------------------
      // Receive some data
      //------------------------------------------------------------------------

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
  // Get the amount of packets
  //----------------------------------------------------------------------------
  uint8_t  packets;
  uint32_t bytesRead;
  Status   sc;
  sc = sock.ReadRaw( (char*)&packets, 1, 60, bytesRead );
  CPPUNIT_ASSERT( sc.status == stOK );

  //----------------------------------------------------------------------------
  // Read each packet
  //----------------------------------------------------------------------------
  char buffer[50000];
  for( int i = 0; i < packets; ++i )
  {
    uint16_t packetSize;
    sc = sock.ReadRaw( (char *)&packetSize, 2, 60, bytesRead );
    CPPUNIT_ASSERT( sc.status == stOK );
    sc = sock.ReadRaw( buffer, packetSize, 60, bytesRead );
    CPPUNIT_ASSERT( sc.status == stOK );
  }

  sock.Disconnect();
  CPPUNIT_ASSERT( serv.Stop() );
}
