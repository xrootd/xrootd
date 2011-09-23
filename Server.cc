//------------------------------------------------------------------------------
// author: Lukasz Janyst <ljanyst@cern.ch>
// desc:   Testing server
//------------------------------------------------------------------------------

#include "Server.hh"
#include "Utils.hh"
#include "XrdSys/XrdSysDNS.hh"

#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>

//------------------------------------------------------------------------------
// Define the client helper
//------------------------------------------------------------------------------
struct ClientHelper
{
  ClientHandler  *handler;
  int             socket;
  pthread_t       thread;
  XrdClient::Log *log;
  std::string     name;
};

//------------------------------------------------------------------------------
// Stuff that needs C linkage
//------------------------------------------------------------------------------
extern "C"
{
  void *HandleConnections( void *arg )
  {
    Server *srv = (Server*)arg;
    long ret = srv->HandleConnections();
    return (void *)ret;
  }

  void *HandleClient( void *arg )
  {
    ClientHelper *helper = (ClientHelper*)arg;
    helper->handler->HandleConnection( helper->socket, helper->log );
    return 0;
  }
}

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
Server::Server(): pListenSocket(-1), pHandlerFactory(0)
{
  pLog = new XrdClient::Log();
  char *level = getenv( "XRD_TEST_LOGLEVEL" );
  if( level )
     pLog->SetLevel( level );
}

//------------------------------------------------------------------------------
// Destructor
//------------------------------------------------------------------------------
Server::~Server()
{
  delete pLog;
  delete pHandlerFactory;
  close( pListenSocket );
}


//------------------------------------------------------------------------------
// Listen for incomming connections and handle clients
//------------------------------------------------------------------------------
bool Server::Setup( int port, int accept, ClientHandlerFactory *factory )
{
  pLog->Debug( 1, "Seting up the server, port: %d, clients: %d", port, accept );

  //----------------------------------------------------------------------------
  // Set up the handler factory
  //----------------------------------------------------------------------------
  if( !factory )
  {
    pLog->Error( 1, "Invalid client handler factory" );
    return false;
  }
  pHandlerFactory = factory;

  //----------------------------------------------------------------------------
  // Create and bind the socket
  //----------------------------------------------------------------------------
  pListenSocket = socket( AF_INET, SOCK_STREAM, 0 );
  if( pListenSocket < 0 )
  {
    pLog->Error( 1, "Unable to create listening socket: %s", strerror( errno ) );
    return false;
  }

  int optVal = 1;
  if( setsockopt( pListenSocket, SOL_SOCKET, SO_REUSEADDR,
                  &optVal, sizeof(optVal) ) == -1 )
  {
    pLog->Error( 1, "Unable to set the REUSEADDR option: %s", strerror( errno ) );
    return false;
  }

  sockaddr_in servAddr;
  memset( &servAddr, 0, sizeof(servAddr) );
  servAddr.sin_family      = AF_INET;
  servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servAddr.sin_port        = htons(port);

  if( bind( pListenSocket, (sockaddr *)&servAddr, sizeof( servAddr ) ) < 0 )
  {
    pLog->Error( 1, "Unable to bind the socket: %s", strerror( errno ) );
    return false;
  }

  if( listen( pListenSocket, accept ) < 0 )
  {
    pLog->Error( 1, "Unable to listen on the socket: %s", strerror( errno ) );
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
  pLog->Debug( 1, "Spawning the server thread" );
  if( pthread_create( &pServerThread, 0, ::HandleConnections, this ) < 0 )
  {
    pLog->Error( 1, "Unable to spawn the server thread: %s", strerror(errno) );
    return false;
  }
  return true;
}

//------------------------------------------------------------------------------
// Stop the server
//------------------------------------------------------------------------------
bool Server::Stop()
{
  pLog->Debug( 1, "Waiting for the server thread to finish" );
  long ret;
  if( pthread_join( pServerThread, (void**)&ret ) < 0 )
  {
    pLog->Error( 1, "Unable to join the server thread: %s", strerror(errno) );
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
  //----------------------------------------------------------------------------
  // Initiate the connections
  //----------------------------------------------------------------------------
  for( int i = 0; i < pClients.size(); ++i )
  {
    //--------------------------------------------------------------------------
    // Accept the connection
    //--------------------------------------------------------------------------
    sockaddr_in inetAddr;
    socklen_t   inetLen = sizeof( inetAddr );
    int connectionSocket = accept( pListenSocket, (sockaddr*)&inetAddr,
                                   &inetLen );
    if( connectionSocket < 0 )
    {
      pLog->Error( 1, "Unable to accept a connection: %s", strerror( errno ) );
      return 1;
    }

    //--------------------------------------------------------------------------
    // Create the handler
    //--------------------------------------------------------------------------
    ClientHandler *handler = pHandlerFactory->CreateHandler();
    char           nameBuff[1024];
    ClientHelper  *helper = new ClientHelper();
    XrdSysDNS::IPFormat( (sockaddr*)&inetAddr, nameBuff, sizeof( nameBuff ) );
    pLog->Debug( 1, "Accepted a connection from %s", nameBuff );

    //--------------------------------------------------------------------------
    // Spawn the thread
    //--------------------------------------------------------------------------
    helper->log     = pLog;
    helper->name    = nameBuff;
    helper->handler = handler;
    helper->socket  = connectionSocket;
    if( pthread_create( &helper->thread, 0, ::HandleClient, helper ) < 0 )
    {
      pLog->Error( 1, "Unable to spawn a new thread for client no %d: %s",
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
      pLog->Debug( 1, "Skipping client that falied to start" );
      continue;
    }

    //--------------------------------------------------------------------------
    // Join the client thread
    //--------------------------------------------------------------------------
    if( pthread_join( (*it)->thread, 0 ) < 0 )
      pLog->Error( 1, "Unable to join the clint thread for: %s",
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
