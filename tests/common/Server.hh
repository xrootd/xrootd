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

#ifndef SERVER_HH
#define SERVER_HH

#include <map>
#include <string>
#include <utility>
#include <vector>
#include <stdint.h>
#include <pthread.h>

namespace XrdClTests {

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
    //--------------------------------------------------------------------------
    virtual void HandleConnection( int socket ) = 0;

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
struct ClientHelper;

//------------------------------------------------------------------------------
//! Server emulator
//------------------------------------------------------------------------------
class Server
{
  public:
    enum ProtocolFamily
    {
      Inet4,
      Inet6,
      Both
    };

    typedef std::map<std::string, std::pair<uint64_t, uint32_t> > TransferMap;

    //--------------------------------------------------------------------------
    //! Constructor
    //--------------------------------------------------------------------------
    Server( ProtocolFamily family );

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
    ProtocolFamily              pProtocolFamily;

};

}

#endif // SERVER_HH
