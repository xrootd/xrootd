#include <iostream>
#include <sys/socket.h>
#include <cstdlib>
#include <netinet/in.h>
#include <string>
#include <unistd.h>

#include <memory>
#include <mutex>
#include <sstream>
#include <thread>
#include <queue>
#include <condition_variable>
#include <fcntl.h>

#include <cstdio>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>


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
    SocketIO( int socket ) : socket( socket )
    {
    }

    ~SocketIO()
    {
    }

    int read( void *buffer, int size )
    {
      int ret = 0;

      char *buff = static_cast<char*>( buffer );
      while ( size != 0 )
      {
        int rc = ::read( socket, buff, size );

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
      int rc = ::write( socket, buffer, size );
      return rc;
    }

    void set_fn( const std::string &fn )
    {
      this->fn = fn;
    }

    const std::string& get_fn()
    {
      return fn;
    }

  private:
    int  socket;
    std::string fn;

};

struct wrt_queue
{
  wrt_queue() : working( true ), worker( work, this )
  {
  }

  ~wrt_queue()
  {
    working = false;
    cv.notify_all();
    worker.join();
  }

  static void work( wrt_queue *myself )
  {
    while( myself->working )
    {
      wrt_request req = myself->pop();
      if( !myself->working ) return;
      int rc = pwrite( req.fd, req.buf, req.len, req.off );
      if( rc < 0 )
      {
	stdio.write( std::to_string( req.fd ) + ", " + std::to_string( errno ) );
        stdio.write( std::string( "Write failed: " ) + strerror( errno ) );
      }
      free( req.buf );
      myself->done();
    }
  }

  struct wrt_request
  {
    wrt_request( int fd, char *buf, size_t len, off_t off ) :
      fd( fd ), buf( buf ), len( len ), off( off )
    {
    }

    wrt_request() : fd( -1 ), buf( nullptr ), len( 0 ), off( 0 ){ }

    int     fd;
    char   *buf;
    size_t  len;
    off_t   off;
  };

  void write( int fd, char *buf, size_t len, off_t off )
  {
    wrt_request req( fd, buf, len, off );
    push( req );
  }

  void push( wrt_request &req )
  {
    std::unique_lock<std::mutex> lck( mtx );
    q.push( req );
    cv.notify_all();
  }

  wrt_request pop()
  {
    std::unique_lock<std::mutex> lck( mtx );
    while( q.empty() && working )
      cv.wait( lck );
    if( !working ) return wrt_request();
    wrt_request req = q.front();
    q.pop();
    return req;
  }

  void wait_done()
  {
    std::unique_lock<std::mutex> lck( mtx2 );
    while( !q.empty() )
    {
      cv2.wait( lck );
    }
    std::cout << "done waiting" << std::endl;
  }

  void done()
  {
    std::unique_lock<std::mutex> lck( mtx2 );
    cv2.notify_all();
  }

  bool working;
  std::thread worker;
  std::queue<wrt_request> q;
  std::mutex mtx;
  std::condition_variable cv;

  std::mutex mtx2;
  std::condition_variable cv2;
};

void handle_error(const char *file, int lineno, const char *msg) {
  fprintf(stderr, "** %s:%i %s\n", file, lineno, msg);
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

  kXR_int32 flags = kXR_DataServer;
  std::cout << "Server flags = " << flags << std::endl;

  ServerResponseBody_Protocol body;
  body.pval  = htonl( 0x500 );
  body.flags = htonl( flags );
  io.write( &body, sizeof(ServerResponseBody_Protocol) );
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

int HandleOpenReq( SocketIO &io, ClientRequestHdr *hdr )
{
  ClientOpenRequest *req = (ClientOpenRequest*) hdr;
  std::stringstream ss;
  ss << __func__ << std::endl;
  ss << "Open mode : 0x" << std::hex << ntohs( req->mode ) << std::dec << std::endl;

  static const std::string statstr = "ABCD 1024 0 0";

  char *buffer = new char[req->dlen];
  io.read( buffer, req->dlen );
  std::string path( buffer, req->dlen );
  io.set_fn( path );
  ss << "Path : " << std::string( buffer, req->dlen ) << std::endl;
  delete[] buffer;

  ss << "opening : " << path << std::endl;
  int fd = open( path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_DIRECT, 0664 );
  if( fd < 0 )
    stdio.write( strerror( errno ) );
  else
    ss << "file opened : " << fd << std::endl;


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
  stdio.write( ss.str() );

  return fd;
}

 void HandleStatReq( SocketIO &io, ClientRequestHdr *hdr )
{
  ClientStatRequest *req = (ClientStatRequest*) hdr;
  std::stringstream ss;
  ss << __func__ << std::endl;

  static const std::string statstr = "ABCD 1024 0 0";

  char *buffer = new char[req->dlen];
  io.read( buffer, req->dlen );
  std::string path( buffer, req->dlen );
  ss << "Path : " << std::string( buffer, req->dlen ) << std::endl;
  delete[] buffer;

  ServerResponseHeader respHdr;
  memset( &respHdr, 0, sizeof( ServerResponseHeader ) );
  respHdr.streamid[0] = req->streamid[0];
  respHdr.streamid[1] = req->streamid[1];
  respHdr.status = kXR_ok;
  respHdr.dlen   = htonl( statstr.size() );
  io.write( &respHdr, sizeof( ServerResponseHeader ) );
  io.write( statstr.c_str(), statstr.size() );
  stdio.write( ss.str() );
}

void HandleWriteReq( SocketIO &io, ClientRequestHdr *hdr, int fd, wrt_queue &wq )
{
  ClientWriteRequest *req = (ClientWriteRequest*) hdr;
  std::stringstream ss;
  ss << __func__ << " : " << "control stream." << std::endl;
  ss << "req->dlen = " << req->dlen << std::endl;
  req->offset = ntohll( req->offset );
  ss << std::dec << "Read " << req->dlen << " bytes from socket.";
  void *ptr = nullptr;
  posix_memalign( &ptr, 512, req->dlen );
  char *buffer = (char*)ptr;
  io.read( buffer, req->dlen );
  wq.write( fd, buffer, req->dlen, req->offset );
  ServerResponseHeader respHdr;
  memset( &respHdr, 0, sizeof( ServerResponseHeader ) );
  respHdr.streamid[0] = req->streamid[0];
  respHdr.streamid[1] = req->streamid[1];
  respHdr.status = kXR_ok;
  respHdr.dlen   = 0;

  // pick up the I/O based on the pathid
  io.write( &respHdr, sizeof( ServerResponseHeader ) );
  //stdio.write( ss.str() );
}

void HandleReadReq( SocketIO &io, ClientRequestHdr *hdr )
{
  ClientReadRequest *req = (ClientReadRequest*) hdr;
  std::stringstream ss;
  ss << __func__ << " : " << "control stream." << std::endl;

  req->rlen = ntohl( req->rlen );
  req->offset = ntohll( req->offset );
  ss << std::dec << "Read " << req->rlen << " bytes at " << req->offset << " offset" << std::endl;

  static const std::string readstr = "ala ma kota, a ola ma psa, a ela ma rybke";

  if( req->dlen )
  {
    ss << "alen : " << req->dlen << std::endl;
    char *buffer = new char[req->dlen];
    io.read( buffer, req->dlen );

    read_args* rargs = (read_args*)buffer;
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
  io.write( &respHdr, sizeof( ServerResponseHeader ) );
  io.write( readstr.c_str(), dlen );
  stdio.write( ss.str() );
}

void HandleCloseReq( SocketIO &io, ClientRequestHdr *hdr, int fd, wrt_queue &wq )
{
  ClientReadRequest *req = (ClientReadRequest*) hdr;
  std::stringstream ss;
  ss << __func__ << std::endl;
  ss << "Closing: " << io.get_fn() << std::endl;
  stdio.write( ss.str() );

  wq.wait_done();
  int rc = close( fd );
  if( rc < 0 )
    stdio.write( strerror( errno ) );

  ServerResponseHeader respHdr;
  memset( &respHdr, 0, sizeof( ServerResponseHeader ) );
  respHdr.streamid[0] = req->streamid[0];
  respHdr.streamid[1] = req->streamid[1];
  respHdr.status = kXR_ok;
  respHdr.dlen   = 0;
  io.write( &respHdr, sizeof( ServerResponseHeader ) );
}


int HandleRequest( SocketIO &io )
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

  wrt_queue wrtq;
  int fd = -1;

  while( true )
  {
    std::stringstream ss;
    ss << std::endl;
    ss << "Waiting for client ..." << std::endl;
    ss << "reading : " << sizeof( ClientRequestHdr ) << " bytes." << std::endl;
    valread = io.read( buffer, sizeof( ClientRequestHdr ) );
    ss << "valread : " << valread << std::endl;
    if( valread < 0 )
    {
      return -1;
    }
    else if( valread == 0 )
    {
      stdio.write( "client terminated the connection");
      return 0;
    }
    else if( valread < 8 )
    {
      std::cout << "Got bogus header : " << valread << std::endl;
      std::cout << std::string( buffer, valread ) << std::endl;
      return -1;
    }

    ClientRequestHdr *hdr = (ClientRequestHdr*)buffer;
    hdr->dlen   = ntohl( hdr->dlen );
    hdr->requestid = ntohs( hdr->requestid );

    ss << "Got request: " << hdr->requestid << std::endl;
    //stdio.write( ss.str() );

    switch( hdr->requestid )
    {
      case kXR_close:
      {
        stdio.write( "Got kXR_close!" );
        HandleCloseReq( io, hdr, fd, wrtq );
        fd = -1;
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

      case kXR_open:
      {
        stdio.write( "Got kXR_open!" );
        fd = HandleOpenReq( io, hdr );
        break;
      }

      case kXR_read:
      {
        stdio.write( "Got kXR_read!" );
        HandleReadReq( io, hdr );
        break;
      }

      case kXR_stat:
      {
        stdio.write(  "Got kXR_stat!" );
        HandleStatReq( io, hdr );
        break;
      }

      case kXR_write:
      {
        //stdio.write( "Got kXR_write!" );
        HandleWriteReq( io, hdr, fd, wrtq );
        break;
      }

      default:
      {
        stdio.write(  "Got unsupported request!" );
        break;
      }
    };
  }

  return 0;
}


void control_stream( int socket )
{
  std::stringstream ss;
  ss << '\n' << __func__ << '\n';
  stdio.write( ss.str() );
  SocketIO io( socket );
  HandleRequest( io );
}


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
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,
                                                  &opt, sizeof(opt)))
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( 2094 );

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

    std::list<std::thread> threads;

    while( true )
    {
      std::stringstream ss;
      ss << "Waiting to accept new TCP connection!" << std::endl;
      stdio.write( ss.str() );
      if ((new_socket = accept(server_fd, (struct sockaddr *)&address, // TODO
                         (socklen_t*)&addrlen))<0)
      {
          perror("accept");
          exit(EXIT_FAILURE);
      }
      ss << "New TCP connection accepted!" << std::endl;
      stdio.write( ss.str() );

      threads.emplace_back( control_stream, new_socket );
    }

    std::cout << "The End." << std::endl;

    return 0;
}
