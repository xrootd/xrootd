#include <iostream>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string>
#include <unistd.h>

#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "XrdCl/XrdClXRootDTransport.hh"
#include "XProtocol/XProtocol.hh"
#include "XrdSys/XrdSysPlatform.hh"

/* Global SSL context */
SSL_CTX *ctx;

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

void ssl_init(const char * certfile, const char* keyfile)
{
  printf("initialising SSL\n");

  /* SSL library initialisation */
  SSL_library_init();
  OpenSSL_add_all_algorithms();
  SSL_load_error_strings();
  ERR_load_BIO_strings();
  ERR_load_crypto_strings();

  /* create the SSL server context */
  ctx = SSL_CTX_new(SSLv23_method());
  if (!ctx)
    die("SSL_CTX_new()");

  /* Load certificate and private key files, and check consistency */
  if (certfile && keyfile) {
    if (SSL_CTX_use_certificate_file(ctx, certfile,  SSL_FILETYPE_PEM) != 1)
      int_error("SSL_CTX_use_certificate_file failed");

    if (SSL_CTX_use_PrivateKey_file(ctx, keyfile, SSL_FILETYPE_PEM) != 1)
      int_error("SSL_CTX_use_PrivateKey_file failed");

    /* Make sure the key and certificate file match. */
    if (SSL_CTX_check_private_key(ctx) != 1)
      int_error("SSL_CTX_check_private_key failed");
    else
      printf("certificate and private key loaded and verified\n");
  }

  /* Recommended to avoid SSLv2 & SSLv3 */
  SSL_CTX_set_options(ctx, SSL_OP_ALL|SSL_OP_NO_SSLv2|SSL_OP_NO_SSLv3);

  SSL_CTX_set_mode( ctx, SSL_MODE_AUTO_RETRY );
}

void DoInitHS( SSL *ssl )
{
  ServerResponseHeader respHdr;
  memset( &respHdr, 0, sizeof( ServerResponseHeader ) );
  respHdr.status = kXR_ok;
  respHdr.dlen = htonl( 2 * sizeof( kXR_int32 ) );
  SSL_write( ssl, &respHdr, sizeof(ServerResponseHeader) );

  ServerInitHandShake  hs;
  memset( &hs, 0, sizeof( ServerInitHandShake ) );
  hs.protover = htonl( 0x310 );
  SSL_write( ssl, &hs.protover, sizeof( kXR_int32 ) );
  hs.msgval   = htonl( kXR_DataServer );
  SSL_write( ssl, &hs.msgval, sizeof( kXR_int32 ) );
}

void HandleProtocolReq( SSL *ssl, ClientRequestHdr *hdr )
{
  ClientProtocolRequest *req = (ClientProtocolRequest*)hdr;

  std::cout << "HandleProtocolReq" << std::endl;
  std::cout << "Client protocol version : " << std::hex << ntohl(req->clientpv) << std::endl;
  std::cout << "Flags : " << (int)req->flags << std::endl;

  ServerResponseHeader respHdr;
  memset( &respHdr, 0, sizeof( ServerResponseHeader ) );
  respHdr.status = kXR_ok;
  respHdr.dlen = htonl( sizeof( ServerResponseBody_Protocol ) );
  SSL_write( ssl, &respHdr, sizeof(ServerResponseHeader) );

  ServerResponseBody_Protocol body;
  body.pval  = htonl( 0x310 );
  body.flags = htonl( kXR_DataServer );
  SSL_write( ssl, &body, sizeof(ServerResponseBody_Protocol) );
}

void HandleLoginReq( SSL *ssl, ClientRequestHdr *hdr )
{
  ClientLoginRequest *req = (ClientLoginRequest*) hdr;

  std::cout << __func__ << std::endl;
  std::cout << "Client PID : " << std::dec << ntohl( req->pid ) << std::endl;

  char *buffer = new char[hdr->dlen];
//  ::read( sfd, buffer, hdr->dlen );
  SSL_read( ssl, buffer, hdr->dlen );
  std::cout << "Token : " << std::string( buffer, hdr->dlen ) << std::endl;
  delete[] buffer;

  ServerResponseHeader respHdr;
  memset( &respHdr, 0, sizeof( ServerResponseHeader ) );
  respHdr.status = kXR_ok;
  respHdr.dlen   = htonl( 16 );
  SSL_write( ssl, &respHdr, sizeof( ServerResponseHeader ) );

  ServerResponseBody_Login body;
  memset( body.sessid, 0, 16 );
  SSL_write( ssl, &body, 16 );
}

void HandleOpenReq( SSL *ssl, ClientRequestHdr *hdr )
{
  ClientOpenRequest *req = (ClientOpenRequest*) hdr;
  std::cout << __func__ << std::endl;
  std::cout << "Open mode : 0x" << std::hex << ntohs( req->mode ) << std::endl;

  static const std::string statstr = "ABCD 1024 0 0";

  char *buffer = new char[req->dlen];
  SSL_read( ssl, buffer, req->dlen );
  std::cout << "Path : " << std::string( buffer, req->dlen ) << std::endl;
  delete[] buffer;

  ServerResponseHeader respHdr;
  memset( &respHdr, 0, sizeof( ServerResponseHeader ) );
  respHdr.streamid[0] = req->streamid[0];
  respHdr.streamid[1] = req->streamid[1];
  respHdr.status = kXR_ok;
  respHdr.dlen   = htonl( 12 + statstr.size() );
  SSL_write( ssl, &respHdr, sizeof( ServerResponseHeader ) );

  ServerResponseBody_Open body;
  memset( body.fhandle, 0, 4 );
  memset( body.cptype,  0, 4 );
  body.cpsize = 0;
  SSL_write( ssl, &body, 12 );

  SSL_write( ssl, statstr.c_str(), statstr.size() );

  std::cout << "Renegotiating session ..." << std::endl;
  SSL_renegotiate( ssl ); //< test session re-negotiation
}

void HandleReadReq( SSL *ssl, ClientRequestHdr *hdr )
{
  ClientReadRequest *req = (ClientReadRequest*) hdr;
  std::cout << __func__ << std::endl;

  req->rlen = ntohl( req->rlen );
  req->offset = ntohll( req->offset );
  std::cout << std::dec << "Read " << req->rlen << " bytes at " << req->offset << " offset" << std::endl;

  static const std::string readstr = "ala ma kota, a ola ma psa, a ela ma rybke";

  if( req->dlen )
  {
    std::cout << "alen : " << req->dlen << std::endl;
    char *buffer = new char[req->dlen];
    SSL_read( ssl, buffer, req->dlen );
    delete[] buffer;
  }

  int dlen = req->rlen > int( readstr.size() ) ? readstr.size() : req->rlen;

  ServerResponseHeader respHdr;
  memset( &respHdr, 0, sizeof( ServerResponseHeader ) );
  respHdr.streamid[0] = req->streamid[0];
  respHdr.streamid[1] = req->streamid[1];
  respHdr.status = kXR_ok;
  respHdr.dlen   = htonl( dlen );
  SSL_write( ssl, &respHdr, sizeof( ServerResponseHeader ) );

  SSL_write( ssl, readstr.c_str(), dlen );
}

void HandleCloseReq( SSL *ssl, ClientRequestHdr *hdr )
{
  ClientReadRequest *req = (ClientReadRequest*) hdr;
  std::cout << __func__ << std::endl;


  ServerResponseHeader respHdr;
  memset( &respHdr, 0, sizeof( ServerResponseHeader ) );
  respHdr.streamid[0] = req->streamid[0];
  respHdr.streamid[1] = req->streamid[1];
  respHdr.status = kXR_ok;
  respHdr.dlen   = 0;
  SSL_write( ssl, &respHdr, sizeof( ServerResponseHeader ) );
}

int main(int argc, char const *argv[])
{
    ssl_init("server.crt", "server.key"); // see README to create these files

    int server_fd, new_socket, valread;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[1024] = {0};

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
    if ((new_socket = accept(server_fd, (struct sockaddr *)&address,
                       (socklen_t*)&addrlen))<0)
    {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    BIO *sbio = BIO_new_socket( new_socket, BIO_NOCLOSE );
    SSL *ssl = SSL_new( ctx );
    SSL_set_accept_state( ssl );
    SSL_set_bio( ssl, sbio, sbio );
    int rc = SSL_accept( ssl );
    int code = SSL_get_error( ssl, rc );

    if( code != SSL_ERROR_NONE )
    {
      char *str = ERR_error_string( code , 0 );
      std::cerr << "SSL_accept failed : code : " << code << std::endl;
      std::cerr << str << std::endl;
      return -1;
    }

    std::cout << "SSL hand shake done!" << std::endl;

//    valread = ::read( new_socket, buffer, 20 );
    valread = SSL_read( ssl, buffer, 20 );
    std::cout << "valread : " << valread << std::endl;
    ClientInitHandShake   *init  = (ClientInitHandShake*)buffer;
    std::cout << "First : "  << ntohl( init->first )  << std::endl;
    std::cout << "Second : " << ntohl( init->second ) << std::endl;
    std::cout << "Third : "  << ntohl( init->third )  << std::endl;
    std::cout << "Fourth : " << ntohl( init->fourth ) << std::endl;
    std::cout << "Fifth : "  << ntohl( init->fifth ) << std::endl;

    DoInitHS( ssl );

    int count = 0;
    while( count < 5 )
    {
      ++count;

      std::cout << std::endl;
      std::cout << "Waiting for client ..." << std::endl;
//      valread = ::read( new_socket , buffer, sizeof( ClientRequestHdr ) );
      valread = SSL_read( ssl, buffer, sizeof( ClientRequestHdr ) );
      if( valread < 0 )
      {
        return -1;
      }
      else if( valread < 8 )
      {
        std::cout << "Got bogus header!" << std::endl;
        return -1;
      }

      ClientRequestHdr *hdr = (ClientRequestHdr*)buffer;
      hdr->dlen   = ntohl( hdr->dlen );
      hdr->requestid = ntohs( hdr->requestid );

//      std::cout << "Read header : " << std::endl;
//      std::cout << "requestid = " << hdr->requestid << std::endl;
//      std::cout << "dlen = " << hdr->dlen << std::endl;

      switch( hdr->requestid )
      {
        case kXR_auth:
        {
          std::cout << "Got kXR_auth!" << std::endl;
          break;
        }

        case kXR_query:
        {
          std::cout << "Got kXR_query!" << std::endl;
          break;
        }

        case kXR_chmod:
        {
          std::cout << "Got kXR_chmod!" << std::endl;
          break;
        }

        case kXR_close:
        {
          std::cout << "Got kXR_close!" << std::endl;
          HandleCloseReq( ssl, hdr );
          break;
        }

        case kXR_dirlist:
        {
          std::cout << "Got kXR_dirlist!" << std::endl;
          break;
        }

        case kXR_getfile:
        {
          std::cout << "Got kXR_getfile!" << std::endl;
          break;
        }

        case kXR_protocol:
        {
          std::cout << "Got kXR_protocol!" << std::endl;
          HandleProtocolReq( ssl, hdr );
          break;
        }

        case kXR_login:
        {
          std::cout << "Got kXR_login!" << std::endl;
          HandleLoginReq( ssl, hdr );
          break;
        }

        case kXR_mkdir:
        {
          std::cout << "Got kXR_mkdir!" << std::endl;
          break;
        }

        case kXR_mv:
        {
          std::cout << "Got kXR_mv!" << std::endl;
          break;
        }

        case kXR_open:
        {
          std::cout << "Got kXR_open!" << std::endl;
          HandleOpenReq( ssl, hdr );
          break;
        }

        case kXR_ping:
        {
          std::cout << "Got kXR_ping!" << std::endl;
          break;
        }

        case kXR_putfile:
        {
          std::cout << "Got kXR_putfile!" << std::endl;
          break;
        }

        case kXR_read:
        {
          std::cout << "Got kXR_read!" << std::endl;
          HandleReadReq( ssl, hdr );
          break;
        }

        case kXR_rm:
        {
          std::cout << "Got kXR_rm!" << std::endl;
          break;
        }

        case kXR_rmdir:
        {
          std::cout << "Got kXR_rmdir!" << std::endl;
          break;
        }

        case kXR_sync:
        {
          std::cout << "Got kXR_sync!" << std::endl;
          break;
        }

        case kXR_stat:
        {
          std::cout << "Got kXR_stat!" << std::endl;
          break;
        }

        case kXR_set:
        {
          std::cout << "Got kXR_set!" << std::endl;
          break;
        }

        case kXR_write:
        {
          std::cout << "Got kXR_write!" << std::endl;
          break;
        }

        case kXR_admin:
        {
          std::cout << "Got kXR_admin!" << std::endl;
          break;
        }

        case kXR_prepare:
        {
          std::cout << "Got kXR_prepare!" << std::endl;
          break;
        }

        case kXR_statx:
        {
          std::cout << "Got kXR_statx!" << std::endl;
          break;
        }

        case kXR_endsess:
        {
          std::cout << "Got kXR_endsess!" << std::endl;
          break;
        }

        case kXR_bind:
        {
          std::cout << "Got kXR_bind!" << std::endl;
          break;
        }

        case kXR_readv:
        {
          std::cout << "Got kXR_readv!" << std::endl;
          break;
        }

        case kXR_verifyw:
        {
          std::cout << "Got kXR_verifyw!" << std::endl;
          break;
        }

        case kXR_locate:
        {
          std::cout << "Got kXR_locate!" << std::endl;
          break;
        }

        case kXR_truncate:
        {
          std::cout << "Got kXR_truncate!" << std::endl;
          break;
        }

        case kXR_sigver:
        {
          std::cout << "Got kXR_sigver!" << std::endl;
          break;
        }

        case kXR_decrypt:
        {
          std::cout << "Got kXR_decrypt!" << std::endl;
          break;
        }

        case kXR_writev:
        {
          std::cout << "Got kXR_writev!" << std::endl;
          break;
        }
      };

    }

    sleep( 2 );

    std::cout << "The End." << std::endl;

    return 0;
}
