//------------------------------------------------------------------------------
// Copyright (c) 2011-2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
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

#include <cppunit/extensions/HelperMacros.h>
#include "XrdCl/XrdClPoller.hh"
#include "Server.hh"
#include "Utils.hh"
#include "TestEnv.hh"
#include "CppUnitXrdHelpers.hh"
#include "XrdCl/XrdClURL.hh"
#include "XrdCl/XrdClUtils.hh"
#include "XrdCl/XrdClSocket.hh"

#include <vector>


#include "XrdCl/XrdClPollerBuiltIn.hh"

using namespace XrdClTests;

//------------------------------------------------------------------------------
// Declaration
//------------------------------------------------------------------------------
class PollerTest: public CppUnit::TestCase
{
  public:
    CPPUNIT_TEST_SUITE( PollerTest );
    CPPUNIT_TEST( FunctionTestBuiltIn );
    CPPUNIT_TEST_SUITE_END();
    void FunctionTestBuiltIn();
    void FunctionTest( XrdCl::Poller *poller );
};

CPPUNIT_TEST_SUITE_REGISTRATION( PollerTest );

//------------------------------------------------------------------------------
// Client handler
//------------------------------------------------------------------------------
class RandomPumpHandler: public ClientHandler
{
  public:
    //--------------------------------------------------------------------------
    // Pump some random data through the socket
    //--------------------------------------------------------------------------
    virtual void HandleConnection( int socket )
    {
      XrdCl::ScopedDescriptor scopetDesc( socket );
      XrdCl::Log *log = TestEnv::GetLog();

      uint8_t  packets = random() % 100;
      uint16_t packetSize;
      char     buffer[50000];
      log->Debug( 1, "Sending %d packets to the client", packets );

      for( int i = 0; i < packets; ++i )
      {
        packetSize = random() % 50000;
        log->Dump( 1, "Sending %d packet, %d bytes of data", i, packetSize );
        if( Utils::GetRandomBytes( buffer, packetSize ) != packetSize )
        {
          log->Error( 1, "Unable to get %d bytes of random data", packetSize );
          return;
        }

        if( ::write( socket, buffer, packetSize ) != packetSize )
        {
          log->Error( 1, "Unable to send the %d bytes of random data",
                      packetSize );
          return;
        }
        UpdateSentData( buffer, packetSize );
      }
    }
};

//------------------------------------------------------------------------------
// Client handler factory
//------------------------------------------------------------------------------
class RandomPumpHandlerFactory: public ClientHandlerFactory
{
  public:
    virtual ClientHandler *CreateHandler()
    {
      return new RandomPumpHandler();
    }
};

//------------------------------------------------------------------------------
// Socket listener
//------------------------------------------------------------------------------
class SocketHandler: public XrdCl::SocketHandler
{
  public:
    //--------------------------------------------------------------------------
    // Initializer
    //--------------------------------------------------------------------------
    virtual void Initialize( XrdCl::Poller *poller )
    {
      pPoller = poller;
    }

    //--------------------------------------------------------------------------
    // Handle an event
    //--------------------------------------------------------------------------
    virtual void Event( uint8_t type,
                        XrdCl::Socket *socket )
    {
      //------------------------------------------------------------------------
      // Read event
      //------------------------------------------------------------------------
      if( type & ReadyToRead )
      {
        char    buffer[50000];
        int     desc = socket->GetFD();
        ssize_t ret = 0;

        while( 1 )
        {
          char     *current   = buffer;
          uint32_t  spaceLeft = 50000;
          while( (spaceLeft > 0) &&
                 ((ret = ::read( desc, current, spaceLeft )) > 0) )
          {
            current   += ret;
            spaceLeft -= ret;
          }

          UpdateTransferMap( socket->GetSockName(), buffer, 50000-spaceLeft );

          if( ret == 0 )
          {
            pPoller->RemoveSocket( socket );
            return;
          }

          if( ret < 0 )
          {
            if( errno != EAGAIN && errno != EWOULDBLOCK )
              pPoller->EnableReadNotification( socket, false );
            return;
          }
        }
      }

      //------------------------------------------------------------------------
      // Timeout
      //------------------------------------------------------------------------
      if( type & ReadTimeOut )
        pPoller->RemoveSocket( socket );
    }

    //--------------------------------------------------------------------------
    // Update the checksums
    //--------------------------------------------------------------------------
    void UpdateTransferMap( const std::string &sockName,
                            const void        *buffer,
                            uint32_t           size )
    {
      //------------------------------------------------------------------------
      // Check if we have an entry in the map
      //------------------------------------------------------------------------
      std::pair<Server::TransferMap::iterator, bool> res;
      Server::TransferMap::iterator it;
      res = pMap.insert( std::make_pair( sockName, std::make_pair( 0, 0 ) ) );
      it = res.first;
      if( res.second == true )
      {
        it->second.first  = 0;
        it->second.second = Utils::ComputeCRC32( 0, 0 );
      }

      //------------------------------------------------------------------------
      // Update the entry
      //------------------------------------------------------------------------
      it->second.first += size;
      it->second.second = Utils::UpdateCRC32( it->second.second, buffer, size );
    }

    //--------------------------------------------------------------------------
    //! Get the stats of the received data
    //--------------------------------------------------------------------------
    std::pair<uint64_t, uint32_t> GetReceivedStats(
                                      const std::string sockName ) const
    {
      Server::TransferMap::const_iterator it = pMap.find( sockName );
      if( it == pMap.end() )
        return std::make_pair( 0, 0 );
      return it->second;
    }

  private:
    Server::TransferMap  pMap;
    XrdCl::Poller   *pPoller;
};

//------------------------------------------------------------------------------
// Test the functionality of a poller
//------------------------------------------------------------------------------
void PollerTest::FunctionTest( XrdCl::Poller *poller )
{
  using XrdCl::Socket;
  using XrdCl::URL;

  //----------------------------------------------------------------------------
  // Initialize
  //----------------------------------------------------------------------------
  Server server( Server::Both );
  Socket s[3];
  CPPUNIT_ASSERT( server.Setup( 9999, 3, new RandomPumpHandlerFactory() ) );
  CPPUNIT_ASSERT( server.Start() );
  CPPUNIT_ASSERT( poller->Initialize() );
  CPPUNIT_ASSERT( poller->Start() );

  //----------------------------------------------------------------------------
  // Connect the sockets
  //----------------------------------------------------------------------------
  SocketHandler *handler = new SocketHandler();
  for( int i = 0; i < 3; ++i )
  {
    CPPUNIT_ASSERT_XRDST( s[i].Initialize() );
    CPPUNIT_ASSERT_XRDST( s[i].Connect( "localhost", 9999 ) );
    CPPUNIT_ASSERT( poller->AddSocket( &s[i], handler ) );
    CPPUNIT_ASSERT( poller->EnableReadNotification( &s[i], true, 60 ) );
    CPPUNIT_ASSERT( poller->IsRegistered( &s[i] ) );
  }

  //----------------------------------------------------------------------------
  // All the business happens elsewhere so we have nothing better to do
  // here that wait, otherwise server->stop will hang.
  //----------------------------------------------------------------------------
  ::sleep(5);
  
  //----------------------------------------------------------------------------
  // Cleanup
  //----------------------------------------------------------------------------
  CPPUNIT_ASSERT( poller->Stop() );
  CPPUNIT_ASSERT( server.Stop() );
  CPPUNIT_ASSERT( poller->Finalize() );

  std::pair<uint64_t, uint32_t> stats[3];
  std::pair<uint64_t, uint32_t> statsServ[3];
  for( int i = 0; i < 3; ++i )
  {
    CPPUNIT_ASSERT( !poller->IsRegistered( &s[i] ) );
    stats[i] = handler->GetReceivedStats( s[i].GetSockName() );
    statsServ[i] = server.GetSentStats( s[i].GetSockName() );
    CPPUNIT_ASSERT( stats[i].first == statsServ[i].first );
    CPPUNIT_ASSERT( stats[i].second == statsServ[i].second );
  }

  for( int i = 0; i < 3; ++i )
    s[i].Close();

  delete handler;
}

//------------------------------------------------------------------------------
// Test the functionality the built-in poller
//------------------------------------------------------------------------------
void PollerTest::FunctionTestBuiltIn()
{
  XrdCl::Poller *poller = new XrdCl::PollerBuiltIn();
  FunctionTest( poller );
  delete poller;
}
