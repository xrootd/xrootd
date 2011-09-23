//------------------------------------------------------------------------------
// author: Lukasz Janyst <ljanyst@cern.ch>
// desc:   Testing server
//------------------------------------------------------------------------------

#ifndef SERVER_HH
#define SERVER_HH

#include <map>
#include <string>
#include <utility>
#include <vector>
#include <stdint.h>
#include <pthread.h>
#include "XrdCl/XrdClLog.hh"

//------------------------------------------------------------------------------
//! Interface for the client handler
//------------------------------------------------------------------------------
class ClientHandler
{
  public:
    //--------------------------------------------------------------------------
    //! Constructor
    //--------------------------------------------------------------------------
    ClientHandler();

    //--------------------------------------------------------------------------
    //! Destructor
    //--------------------------------------------------------------------------
    virtual ~ClientHandler();

    //--------------------------------------------------------------------------
    //! Handle connection
    //!
    //! @param socket the connection socket - needs to be closed when not needed
    //! @param log    a log object that may be use to report diagnostics
    //--------------------------------------------------------------------------
    virtual void HandleConnection( int socket, XrdClient::Log *log ) = 0;

    //--------------------------------------------------------------------------
    //! Update statistics of the received data
    //--------------------------------------------------------------------------
    void UpdateSentData( char *buffer, uint32_t size );

    //--------------------------------------------------------------------------
    //! Update statistics of the sent data
    //--------------------------------------------------------------------------
    void UpdateReceivedData( char *buffer, uint32_t size );

    //--------------------------------------------------------------------------
    //! Get sent bytes count
    //--------------------------------------------------------------------------
    uint64_t GetSentBytes() const
    {
      return pSentBytes;
    }

    //--------------------------------------------------------------------------
    //! Get the checksum of the sent data buffers
    //--------------------------------------------------------------------------
    uint32_t GetSentChecksum() const
    {
      return pSentChecksum;
    }

    //--------------------------------------------------------------------------
    //! Get the received bytes count
    //--------------------------------------------------------------------------
    uint64_t GetReceivedBytes() const
    {
      return pReceivedBytes;
    }

    //--------------------------------------------------------------------------
    //! Get the checksum of the received data buffers
    //--------------------------------------------------------------------------
    uint32_t GetReceivedChecksum() const
    {
      return pReceivedChecksum;
    }


  private:
    uint64_t    pSentBytes;
    uint64_t    pReceivedBytes;
    uint32_t    pSentChecksum;
    uint32_t    pReceivedChecksum;
};

//------------------------------------------------------------------------------
//! Client hander factory interface
//------------------------------------------------------------------------------
class ClientHandlerFactory
{
  public:
    //--------------------------------------------------------------------------
    //! Destructor
    //--------------------------------------------------------------------------
    virtual ~ClientHandlerFactory() {};

    //--------------------------------------------------------------------------
    //! Create a client handler
    //--------------------------------------------------------------------------
    virtual ClientHandler *CreateHandler() = 0;
};

//------------------------------------------------------------------------------
// Forward declaration
//------------------------------------------------------------------------------
class ClientHelper;

//------------------------------------------------------------------------------
//! Server emulator
//------------------------------------------------------------------------------
class Server
{
  public:
    typedef std::map<std::string, std::pair<uint64_t, uint32_t> > TransferMap;

    //--------------------------------------------------------------------------
    //! Constructor
    //--------------------------------------------------------------------------
    Server();

    //--------------------------------------------------------------------------
    //! Destructor
    //--------------------------------------------------------------------------
    ~Server();

    //--------------------------------------------------------------------------
    //! Listen for incomming connections and handle clients
    //!
    //! @param port    port to listen on
    //! @param accept  number of clients to accept
    //! @param factory client handler factory, the server takes ownership
    //!                of this object
    //--------------------------------------------------------------------------
    bool Setup( int port, int accept, ClientHandlerFactory *factory );

    //--------------------------------------------------------------------------
    //! Start the server
    //--------------------------------------------------------------------------
    bool Start();

    //--------------------------------------------------------------------------
    //! Wait for the server to finish - it blocks until all the clients
    //! have been handled
    //--------------------------------------------------------------------------
    bool Stop();

    //--------------------------------------------------------------------------
    //! Get the statisctics of the sent data
    //--------------------------------------------------------------------------
    std::pair<uint64_t, uint32_t> GetSentStats( const std::string host ) const;

    //--------------------------------------------------------------------------
    //! Get the stats of the received data
    //--------------------------------------------------------------------------
    std::pair<uint64_t, uint32_t> GetReceivedStats( const std::string host ) const;

    //--------------------------------------------------------------------------
    //! Handle clients
    //--------------------------------------------------------------------------
    int HandleConnections();

  private:

    TransferMap                 pSent;
    TransferMap                 pReceived;
    pthread_t                   pServerThread;
    std::vector<ClientHelper*>  pClients;
    int                         pListenSocket;
    ClientHandlerFactory       *pHandlerFactory;
    XrdClient::Log             *pLog;
};

#endif // SERVER_HH
