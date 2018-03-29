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

#include "Server.hh"
#include "Utils.hh"
#include "XrdNet/XrdNetUtils.hh"
#include "XrdCl/XrdClLog.hh"
#include "TestEnv.hh"

#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>


namespace XrdClTests
{
  //----------------------------------------------------------------------------
  // Define the client helper
  //----------------------------------------------------------------------------
  struct ClientHelper
  {
    ClientHandler  *handler;
    int             socket;
    pthread_t       thread;
    std::string     name;
  };
}

//------------------------------------------------------------------------------
// Stuff that needs C linkage
//------------------------------------------------------------------------------
extern "C"
{
  void *HandleConnections( void *arg )
  {
    XrdClTests::Server *srv = (XrdClTests::Server*)arg;
    long ret = srv->HandleConnections();
    return (void *)ret;
  }

  void *HandleClient( void *arg )
  {
    XrdClTests::ClientHelper *helper = (XrdClTests::ClientHelper*)arg;
    helper->handler->HandleConnection( helper->socket );
    return 0;
  }
}

namespace XrdClTests {

//------------------------------------------------------------------------------
// Constructor
//------------------------------------------------------------------------------
ClientHandler::ClientHandler(): pSentBytes(0), pReceivedBytes(0)
{
  pSentChecksum     = Utils::ComputeCRC32( 0, 0 );
  pReceivedChecksum = Utils::ComputeCRC32( 0, 0 );
}

//------------------------------------------------------------------------------
// Destructor
//------------------------------------------------------------------------------
ClientHandler::~ClientHandler()
{
}

//------------------------------------------------------------------------------
// Update statistics of the received data
//------------------------------------------------------------------------------
void ClientHandler::UpdateSentData( char *buffer, uint32_t size )
{
  pSentBytes += size;
  pSentChecksum = Utils::UpdateCRC32( pSentChecksum, buffer, size );
}

//------------------------------------------------------------------------------
// Update statistics of the sent data
//------------------------------------------------------------------------------
void ClientHandler::UpdateReceivedData( char *buffer, uint32_t size )
{
  pReceivedBytes += size;
  pReceivedChecksum = Utils::UpdateCRC32( pReceivedChecksum, buffer, size );
}

//------------------------------------------------------------------------------
// Constructor
//------------------------------------------------------------------------------
Server::Server( ProtocolFamily family ):
  pServerThread(0), pListenSocket(-1), pHandlerFactory(0),
  pProtocolFamily( family )
{
}

//------------------------------------------------------------------------------
// Destructor
//------------------------------------------------------------------------------
Server::~Server()
{
  delete pHandlerFactory;
  close( pListenSocket );
}


//------------------------------------------------------------------------------
// Listen for incomming connections and handle clients
//------------------------------------------------------------------------------
bool Server::Setup( int port, int accept, ClientHandlerFactory *factory )
{
  XrdCl::Log *log = TestEnv::GetLog();
  log->Debug( 1, "Seting up the server, port: %d, clients: %d", port, accept );

  //----------------------------------------------------------------------------
  // Set up the handler factory
  //----------------------------------------------------------------------------
  if( !factory )
  {
    log->Error( 1, "Invalid client handler factory" );
    return false;
  }
  pHandlerFactory = factory;

  //----------------------------------------------------------------------------
  // Create the socket
  //----------------------------------------------------------------------------
  int protocolFamily = AF_INET;
  if( pProtocolFamily == Inet6 || pProtocolFamily == Both )
    protocolFamily = AF_INET6;

  pListenSocket = socket( protocolFamily, SOCK_STREAM, 0 );
  if( pListenSocket < 0 )
  {
    log->Error( 1, "Unable to create listening socket: %s", strerror( errno ) );
    return false;
  }

  int optVal = 1;
  if( setsockopt( pListenSocket, SOL_SOCKET, SO_REUSEADDR,
                  &optVal, sizeof(optVal) ) == -1 )
  {
    log->Error( 1, "Unable to set the REUSEADDR option: %s", strerror( errno ) );
    return false;
  }

  if( pProtocolFamily == Both )
  {
    optVal = 0;
    if( setsockopt( pListenSocket, IPPROTO_IPV6, IPV6_V6ONLY, &optVal,
                    sizeof(optVal) ) == -1 )
    {
      log->Error( 1, "Unable to disable the IPV6_V6ONLY option: %s",
                  strerror( errno ) );
      return false;
    }
  }

  //----------------------------------------------------------------------------
  // Bind the socket
  //----------------------------------------------------------------------------
  sockaddr     *servAddr     = 0;
  sockaddr_in6  servAddr6;
  sockaddr_in   servAddr4;
  int           servAddrSize = 0;

  if( pProtocolFamily == Inet4 )
  {
    memset( &servAddr4, 0, sizeof(servAddr4) );
    servAddr4.sin_family      = AF_INET;
    servAddr4.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr4.sin_port        = htons(port);
    servAddr                  = (sockaddr*)&servAddr4;
    servAddrSize              = sizeof(servAddr4);
  }
  else
  {
    memset( &servAddr6, 0, sizeof(servAddr6) );
    servAddr6.sin6_family = AF_INET6;
    servAddr6.sin6_addr   = in6addr_any;
    servAddr6.sin6_port   = htons( port );
    servAddr              = (sockaddr*)&servAddr6;
    servAddrSize          = sizeof(servAddr6);
  }

  if( bind( pListenSocket, servAddr, servAddrSize ) < 0 )
  {
    log->Error( 1, "Unable to bind the socket: %s", strerror( errno ) );
    return false;
  }

  if( listen( pListenSocket, accept ) < 0 )
  {
    log->Error( 1, "Unable to listen on the socket: %s", strerror( errno ) );
    return false;
  }

  pClients.resize( accept, 0 );

  return true;
}

//------------------------------------------------------------------------------
// Start the server
//------------------------------------------------------------------------------
bool Server::Start()
{
  XrdCl::Log *log = TestEnv::GetLog();
  log->Debug( 1, "Spawning the server thread" );
  if( pthread_create( &pServerThread, 0, ::HandleConnections, this ) < 0 )
  {
    log->Error( 1, "Unable to spawn the server thread: %s", strerror(errno) );
    return false;
  }
  return true;
}

//------------------------------------------------------------------------------
// Stop the server
//------------------------------------------------------------------------------
bool Server::Stop()
{
  XrdCl::Log *log = TestEnv::GetLog();
  log->Debug( 1, "Waiting for the server thread to finish" );
  long ret;
  if( pthread_join( pServerThread, (void**)&ret ) < 0 )
  {
    log->Error( 1, "Unable to join the server thread: %s", strerror(errno) );
    return false;
  }

  if( ret < 0 )
    return false;

  return true;
}

//------------------------------------------------------------------------------
  // Get the statisctics of the sent data
//------------------------------------------------------------------------------
std::pair<uint64_t, uint32_t> Server::GetSentStats( const std::string host) const
{
  TransferMap::const_iterator it = pSent.find( host );
  if( it == pSent.end() )
    return std::make_pair( 0, 0 );
  return it->second;
}

//------------------------------------------------------------------------------
// Get the stats of the received data
//------------------------------------------------------------------------------
std::pair<uint64_t, uint32_t> Server::GetReceivedStats( const std::string host ) const
{
  TransferMap::const_iterator it = pReceived.find( host );
  if( it == pReceived.end() )
    return std::make_pair( 0, 0 );
  return it->second;
}

//------------------------------------------------------------------------------
// Handle clients
//------------------------------------------------------------------------------
int Server::HandleConnections()
{
  XrdCl::Log *log = TestEnv::GetLog();

  //----------------------------------------------------------------------------
  // Initiate the connections
  //----------------------------------------------------------------------------
  for( uint32_t i = 0; i < pClients.size(); ++i )
  {
    //--------------------------------------------------------------------------
    // Accept the connection
    //--------------------------------------------------------------------------
    sockaddr     *inetAddr = 0;
    socklen_t     inetLen  = 0;
    sockaddr_in   inetAddr4;
    sockaddr_in6  inetAddr6;
    if( pProtocolFamily == Inet4 )
    {
      inetAddr = (sockaddr*)&inetAddr4;
      inetLen = sizeof( inetAddr4 );
    }
    else
    {
      inetAddr = (sockaddr*)&inetAddr6;
      inetLen = sizeof( inetAddr6 );
    }

    int connectionSocket = accept( pListenSocket, inetAddr, &inetLen );
    if( connectionSocket < 0 )
    {
      log->Error( 1, "Unable to accept a connection: %s", strerror( errno ) );
      return 1;
    }

    //--------------------------------------------------------------------------
    // Create the handler
    //--------------------------------------------------------------------------
    ClientHandler *handler = pHandlerFactory->CreateHandler();
    char           nameBuff[1024];
    ClientHelper  *helper = new ClientHelper();
    XrdNetUtils::IPFormat( connectionSocket, nameBuff, sizeof(nameBuff) );
    log->Debug( 1, "Accepted a connection from %s", nameBuff );

    //--------------------------------------------------------------------------
    // Spawn the thread
    //--------------------------------------------------------------------------
    helper->name    = nameBuff;
    helper->handler = handler;
    helper->socket  = connectionSocket;
    if( pthread_create( &helper->thread, 0, ::HandleClient, helper ) < 0 )
    {
      log->Error( 1, "Unable to spawn a new thread for client no %d: %s",
                  i, nameBuff );
      delete handler;
      close( connectionSocket );
      delete helper;
      helper = 0;
    }
    pClients[i] = helper;
  }

  //----------------------------------------------------------------------------
  // Wait forr the clients to finish
  //----------------------------------------------------------------------------
  std::vector<ClientHelper*>::iterator it;
  for( it = pClients.begin(); it != pClients.end(); ++it )
  {
    //--------------------------------------------------------------------------
    // Have we managed to start properly?
    //--------------------------------------------------------------------------
    if( *it == 0 )
    {
      log->Debug( 1, "Skipping client that falied to start" );
      continue;
    }

    //--------------------------------------------------------------------------
    // Join the client thread
    //--------------------------------------------------------------------------
    if( pthread_join( (*it)->thread, 0 ) < 0 )
      log->Error( 1, "Unable to join the clint thread for: %s",
                  (*it)->name.c_str() );

    pSent[(*it)->name]     = std::make_pair(
                                (*it)->handler->GetSentBytes(),
                                (*it)->handler->GetSentChecksum() );
    pReceived[(*it)->name] = std::make_pair(
                                (*it)->handler->GetReceivedBytes(),
                                (*it)->handler->GetReceivedChecksum() );
    delete (*it)->handler;
    delete *it;
  }
  return 0;
}

}
