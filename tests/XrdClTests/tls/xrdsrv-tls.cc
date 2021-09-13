#include <iostream>
#include <sys/socket.h>
#include <cstdlib>
#include <netinet/in.h>
#include <string>
#include <unistd.h>

#include <memory>
#include <mutex>
#include <sstream>

#include <cstdio>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "XrdCl/XrdClXRootDTransport.hh"
#include "XProtocol/XProtocol.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdTls/XrdTlsContext.hh"


struct StdIO
{
    void write( const std::string &str )
    {
      std::unique_lock<std::mutex> lck( mtx );
      std::cout << str << std::endl;
    }

  private:

    std::mutex mtx;
} stdio;

struct SocketIO
{
    SocketIO() : tlson( false ), socket( -1 ), ssl( 0 )
    {
    }

    ~SocketIO()
    {
      if( ssl ) SSL_free( ssl );
    }

    void SetSocket( int socket )
    {
      this->socket = socket;
    }

    void TlsHandShake()
    {
      std::stringstream ss;
      ss << std::endl << __func__ << std::endl << std::endl;
      static XrdTlsContext tlsctx("server.crt", "server.key");

      BIO *sbio = BIO_new_socket( socket, BIO_NOCLOSE );
      ssl = SSL_new( static_cast<SSL_CTX*>(tlsctx.Context()) );
      SSL_set_accept_state( ssl );
      SSL_set_bio( ssl, sbio, sbio );
      ss << "SSL_accept is waiting." << std::endl;
      int rc = SSL_accept( ssl );
      int code = SSL_get_error( ssl, rc );

      if( code != SSL_ERROR_NONE )
      {
        char *str = ERR_error_string( code , 0 );
        std::cerr << "SSL_accept failed : code : " << code << std::endl;
        std::cerr << str << std::endl;
        exit( -1 );
      }

      tlson = true;

      ss << "SSL hand shake done!" << std::endl;
      stdio.write( ss.str() );
    }

    int read( void *buffer, int size )
    {
      int ret = 0;

      char *buff = static_cast<char*>( buffer );
      while ( size != 0 )
      {
        int rc = 0;
        if( tlson )
          rc = SSL_read( ssl, buff, size );
        else
          rc = ::read( socket, buff, size );

        if( rc <= 0 )
          break;

        ret  += rc;
        buff += rc;
        size -= rc;
      }

      return ret;
    }

    int write( const void *buffer, int size )
    {
      int rc = 0;
      if( tlson )
        rc = SSL_write( ssl, buffer, size );
      else
        rc = ::write( socket, buffer, size );

      return rc;
    }

    void renegotiate()
    {
      if( tlson )
        SSL_renegotiate( ssl );
    }

  private:

    bool tlson;
    int  socket;
    SSL *ssl;

} mainio, dataio;

void die(const char *msg) {
  perror(msg);
  exit(1);
}

void handle_error(const char *file, int lineno, const char *msg) {
  fprintf(stderr, "** %s:%i %s\n", file, lineno, msg);
  ERR_print_errors_fp(stderr);
  exit(-1);
}

#define int_error(msg) handle_error(__FILE__, __LINE__, msg)

void DoInitHS( SocketIO &io )
{
  ServerResponseHeader respHdr;
  memset( &respHdr, 0, sizeof( ServerResponseHeader ) );
  respHdr.status = kXR_ok;
  respHdr.dlen = htonl( 2 * sizeof( kXR_int32 ) );
  io.write( &respHdr, sizeof(ServerResponseHeader) );

  ServerInitHandShake  hs;
  memset( &hs, 0, sizeof( ServerInitHandShake ) );
  hs.protover = htonl( 0x500 );
  io.write( &hs.protover, sizeof( kXR_int32 ) );
  hs.msgval   = htonl( kXR_DataServer );
  io.write( &hs.msgval, sizeof( kXR_int32 ) );
}

void HandleProtocolReq( SocketIO &io, ClientRequestHdr *hdr )
{
  std::stringstream ss;
  ss << __func__ << std::endl;

  ClientProtocolRequest *req = (ClientProtocolRequest*)hdr;

  ss << "Client protocol version : " << std::hex << ntohl(req->clientpv) << std::dec << std::endl;
  ss << "Flags : " << (int)req->flags << std::endl;
  ss << "Expect : " << (int)req->expect << std::endl;
  stdio.write( ss.str() );

  ServerResponseHeader respHdr;
  memset( &respHdr, 0, sizeof( ServerResponseHeader ) );
  respHdr.status = kXR_ok;
  respHdr.dlen = htonl( sizeof( ServerResponseBody_Protocol ) );
  io.write( &respHdr, sizeof(ServerResponseHeader) );

  kXR_int32 flags = kXR_DataServer | kXR_haveTLS | kXR_gotoTLS | kXR_tlsLogin;// | kXR_tlsData;
  std::cout << "Server flags = " << flags << std::endl;

  ServerResponseBody_Protocol body;
  body.pval  = htonl( 0x500 );
  body.flags = htonl( flags );
  io.write( &body, sizeof(ServerResponseBody_Protocol) );

  if( &io == &mainio && ( flags & kXR_tlsLogin ) )
    io.TlsHandShake();
  else if( &io == &dataio && (flags & kXR_tlsData ) )
    io.TlsHandShake();
}

void HandleLoginReq( SocketIO &io, ClientRequestHdr *hdr )
{
  ClientLoginRequest *req = (ClientLoginRequest*) hdr;

  std::stringstream ss;
  ss << __func__ << std::endl;
  ss << "Client PID : " << std::dec << ntohl( req->pid ) << std::endl;

  char *buffer = new char[hdr->dlen];
  io.read( buffer, hdr->dlen );
  ss << "Token : " << std::string( buffer, hdr->dlen ) << std::endl;
  stdio.write( ss.str() );
  delete[] buffer;

  ServerResponseHeader respHdr;
  memset( &respHdr, 0, sizeof( ServerResponseHeader ) );
  respHdr.status = kXR_ok;
  respHdr.dlen   = htonl( 16 );
  io.write( &respHdr, sizeof( ServerResponseHeader ) );

  ServerResponseBody_Login body;
  memset( body.sessid, 0, 16 );
  io.write( &body, 16 );
}

void HandleOpenReq( SocketIO &io, ClientRequestHdr *hdr )
{
  ClientOpenRequest *req = (ClientOpenRequest*) hdr;
  std::stringstream ss;
  ss << __func__ << std::endl;
  ss << "Open mode : 0x" << std::hex << ntohs( req->mode ) << std::dec << std::endl;

  static const std::string statstr = "ABCD 1024 0 0";

  char *buffer = new char[req->dlen];
  io.read( buffer, req->dlen );
  ss << "Path : " << std::string( buffer, req->dlen ) << std::endl;
  delete[] buffer;

  ServerResponseHeader respHdr;
  memset( &respHdr, 0, sizeof( ServerResponseHeader ) );
  respHdr.streamid[0] = req->streamid[0];
  respHdr.streamid[1] = req->streamid[1];
  respHdr.status = kXR_ok;
  respHdr.dlen   = htonl( 12 + statstr.size() );
  io.write( &respHdr, sizeof( ServerResponseHeader ) );

  ServerResponseBody_Open body;
  memset( body.fhandle, 0, 4 );
  memset( body.cptype,  0, 4 );
  body.cpsize = 0;
  io.write( &body, 12 );

  io.write( statstr.c_str(), statstr.size() );

  ss << "Renegotiating session ..." << std::endl;
  stdio.write( ss.str() );
  io.renegotiate(); //< test session re-negotiation !!!
}

void HandleReadReq( SocketIO &io, ClientRequestHdr *hdr )
{
  ClientReadRequest *req = (ClientReadRequest*) hdr;
  std::stringstream ss;
  ss << __func__ << " : " << ( &io == &dataio ? "data stream." : "control stream." ) << std::endl;

  req->rlen = ntohl( req->rlen );
  req->offset = ntohll( req->offset );
  ss << std::dec << "Read " << req->rlen << " bytes at " << req->offset << " offset" << std::endl;

  static const std::string readstr = "ala ma kota, a ola ma psa, a ela ma rybke";

  kXR_char pathid = 0;
  if( req->dlen )
  {
    ss << "alen : " << req->dlen << std::endl;
    char *buffer = new char[req->dlen];
    io.read( buffer, req->dlen );

    read_args* rargs = (read_args*)buffer;
    pathid = rargs->pathid;
    ss << "Path ID : " << (int)rargs->pathid << std::endl;
    delete[] buffer;
  }

  int dlen = req->rlen > int( readstr.size() ) ? readstr.size() : req->rlen;

  ServerResponseHeader respHdr;
  memset( &respHdr, 0, sizeof( ServerResponseHeader ) );
  respHdr.streamid[0] = req->streamid[0];
  respHdr.streamid[1] = req->streamid[1];
  respHdr.status = kXR_ok;
  respHdr.dlen   = htonl( dlen );

  // pick up the I/O based on the pathid
  if( !pathid )
  {
    mainio.write( &respHdr, sizeof( ServerResponseHeader ) );
    mainio.write( readstr.c_str(), dlen );
  }
  else
  {
    ss << "Writing to data stream!" << std::endl;
    dataio.write( &respHdr, sizeof( ServerResponseHeader ) );
    dataio.write( readstr.c_str(), dlen );
  }

  stdio.write( ss.str() );
}

void HandleCloseReq( SocketIO &io, ClientRequestHdr *hdr )
{
  ClientReadRequest *req = (ClientReadRequest*) hdr;
  std::stringstream ss;
  ss << __func__ << std::endl;
  stdio.write( ss.str() );

  ServerResponseHeader respHdr;
  memset( &respHdr, 0, sizeof( ServerResponseHeader ) );
  respHdr.streamid[0] = req->streamid[0];
  respHdr.streamid[1] = req->streamid[1];
  respHdr.status = kXR_ok;
  respHdr.dlen   = 0;
  io.write( &respHdr, sizeof( ServerResponseHeader ) );
}

void HandleBindReq( SocketIO &io, ClientRequestHdr *hdr )
{
  ClientBindRequest *req = (ClientBindRequest*) hdr;
  std::stringstream ss;
  ss << __func__ << std::endl;
  stdio.write( ss.str() );

  ServerResponseHeader respHdr;
  memset( &respHdr, 0, sizeof( ServerResponseHeader ) );
  memset( &respHdr, 0, sizeof( ServerResponseHeader ) );
  respHdr.streamid[0] = req->streamid[0];
  respHdr.streamid[1] = req->streamid[1];
  respHdr.status = kXR_ok;
  respHdr.dlen   = htonl( 1 );
  io.write( &respHdr, sizeof( ServerResponseHeader ) );

  ServerResponseBody_Bind body;
  body.substreamid = 1;
  io.write( &body, sizeof( ServerResponseBody_Bind ) );
}


int HandleRequest( SocketIO &io, int iterations )
{
  char buffer[1024] = {0};
  std::fill( buffer, buffer + 1024, 'A' );
  int valread = io.read( buffer, 20 );
  std::stringstream ss;
  ss << "valread : " << valread << std::endl;
  if( valread != 20 ) return -1;
  ClientInitHandShake   *init  = (ClientInitHandShake*)buffer;
  ss << "First : "  << ntohl( init->first )  << std::endl;
  ss << "Second : " << ntohl( init->second ) << std::endl;
  ss << "Third : "  << ntohl( init->third )  << std::endl;
  ss << "Fourth : " << ntohl( init->fourth ) << std::endl;
  ss << "Fifth : "  << ntohl( init->fifth ) << std::endl;
  stdio.write( ss.str() );

  DoInitHS( io );

  int count = 0;
  while( count < iterations )
  {
    ++count;

    std::stringstream ss;
    ss << std::endl;
    ss << "Step : " << count << std::endl;
    ss << "Waiting for client ..." << std::endl;
    ss << "reading : " << sizeof( ClientRequestHdr ) << " bytes." << std::endl;
    valread = io.read( buffer, sizeof( ClientRequestHdr ) );
    ss << "valread : " << valread << std::endl;
    if( valread < 0 )
    {
      return -1;
    }
    else if( valread < 8 )
    {
      std::cout << "Got bogus header!" << std::endl;
      std::cout << std::string( buffer, valread ) << std::endl;
      return -1;
    }

    ClientRequestHdr *hdr = (ClientRequestHdr*)buffer;
    hdr->dlen   = ntohl( hdr->dlen );
    hdr->requestid = ntohs( hdr->requestid );

    ss << "Got request: " << hdr->requestid << std::endl;
    stdio.write( ss.str() );

    switch( hdr->requestid )
    {
      case kXR_auth:
      {
        stdio.write( "Got kXR_auth!" );
        break;
      }

      case kXR_query:
      {
        stdio.write( "Got kXR_query!" );
        break;
      }

      case kXR_chmod:
      {
        stdio.write( "Got kXR_chmod!" );
        break;
      }

      case kXR_close:
      {
        stdio.write( "Got kXR_close!" );
        HandleCloseReq( io, hdr );
        break;
      }

      case kXR_dirlist:
      {
        stdio.write(  "Got kXR_dirlist!" );
        break;
      }

      case kXR_gpfile:
      {
        stdio.write(  "Got kXR_gpfile!" );
        break;
      }

      case kXR_protocol:
      {
        stdio.write( "Got kXR_protocol!" );
        HandleProtocolReq( io, hdr );
        break;
      }

      case kXR_login:
      {
        stdio.write(  "Got kXR_login!" );
        HandleLoginReq( io, hdr );
        break;
      }

      case kXR_mkdir:
      {
        stdio.write( "Got kXR_mkdir!" );
        break;
      }

      case kXR_mv:
      {
        stdio.write( "Got kXR_mv!" );
        break;
      }

      case kXR_open:
      {
        stdio.write( "Got kXR_open!" );
        HandleOpenReq( io, hdr );
        break;
      }

      case kXR_ping:
      {
        stdio.write( "Got kXR_ping!" );
        break;
      }

      case kXR_chkpoint:
      {
        stdio.write( "Got kXR_chkpoint!" );
        break;
      }

      case kXR_read:
      {
        stdio.write( "Got kXR_read!" );
        HandleReadReq( io, hdr );
        break;
      }

      case kXR_rm:
      {
        stdio.write( "Got kXR_rm!" );
        break;
      }

      case kXR_rmdir:
      {
        stdio.write( "Got kXR_rmdir!" );
        break;
      }

      case kXR_sync:
      {
        stdio.write( "Got kXR_sync!" );
        break;
      }

      case kXR_stat:
      {
        stdio.write(  "Got kXR_stat!" );
        break;
      }

      case kXR_set:
      {
        stdio.write( "Got kXR_set!" );
        break;
      }

      case kXR_write:
      {
        stdio.write( "Got kXR_write!" );
        break;
      }

      case kXR_fattr:
      {
        stdio.write( "Got kXR_fattr!" );
        break;
      }

      case kXR_prepare:
      {
        stdio.write( "Got kXR_prepare!" );
        break;
      }

      case kXR_statx:
      {
        stdio.write( "Got kXR_statx!" );
        break;
      }

      case kXR_endsess:
      {
        stdio.write( "Got kXR_endsess!" );
        break;
      }

      case kXR_bind:
      {
        stdio.write( "Got kXR_bind!" );
        HandleBindReq( io, hdr );
        break;
      }

      case kXR_readv:
      {
        stdio.write( "Got kXR_readv!" );
        break;
      }

      case kXR_pgwrite:
      {
        stdio.write( "Got kXR_pgwrite!" );
        break;
      }

      case kXR_locate:
      {
        stdio.write( "Got kXR_locate!" );
        break;
      }

      case kXR_truncate:
      {
        stdio.write( "Got kXR_truncate!" );
        break;
      }

      case kXR_sigver:
      {
        stdio.write( "Got kXR_sigver!" );
        break;
      }

      case kXR_pgread:
      {
        stdio.write( "Got kXR_pgread!" );
        break;
      }

      case kXR_writev:
      {
        stdio.write( "Got kXR_writev!" );
        break;
      }
    };

  }

  return 0;
}


void* control_stream( void *arg )
{
  std::stringstream ss;
  ss << '\n' << __func__ << '\n';
  stdio.write( ss.str() );

  int socket = *(int*)arg;
  mainio.SetSocket( socket );
  int rc = HandleRequest( mainio, 6 );
  return new int( rc );
}

void* data_stream( void *arg )
{
  std::stringstream ss;
  ss << '\n' << __func__ << '\n';
  stdio.write( ss.str() );

  int socket = *(int*)arg;
  dataio.SetSocket( socket );
  int rc = HandleRequest( dataio, 2 );
  return new int( rc );
}


// TODO we need control thread and data thread !!!

int main(int argc, char const *argv[])
{
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the port 8080
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                                                  &opt, sizeof(opt)))
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( 1094 );

    // Forcefully attaching socket to the port 8080
    if (bind(server_fd, (struct sockaddr *)&address,
                                 sizeof(address))<0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    pthread_t threads[2];

    int count = 0;
    while( count < 2 )
    {
      std::stringstream ss;
      ss << "Waiting to accept new TCP connection!" << std::endl;
      if ((new_socket = accept(server_fd, (struct sockaddr *)&address, // TODO
                         (socklen_t*)&addrlen))<0)
      {
          perror("accept");
          exit(EXIT_FAILURE);
      }
      ss << "New TCP connection accepted!" << std::endl;
      stdio.write( ss.str() );

      if( count == 0 )
        pthread_create( &threads[count], 0, control_stream, &new_socket );
      else
        pthread_create( &threads[count], 0, data_stream,    &new_socket );
      ++count;
    }

    count = 0;
    while( count < 2 )
    {
      void *ret = 0;
      pthread_join( threads[count], &ret );
      std::unique_ptr<int> rc( (int*)ret );
      if( *rc ) return *rc;
      ++count;
    }

    sleep( 2 );

    std::cout << "The End." << std::endl;

    return 0;
}
