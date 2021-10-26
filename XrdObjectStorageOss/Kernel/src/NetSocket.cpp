/*-----------------------------------------------------------------------------
Copyright(c) 2010 - 2018 ViSUS L.L.C.,
Scientific Computing and Imaging Institute of the University of Utah

ViSUS L.L.C., 50 W.Broadway, Ste. 300, 84101 - 2044 Salt Lake City, UT
University of Utah, 72 S Central Campus Dr, Room 3750, 84112 Salt Lake City, UT

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met :

* Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

* Neither the name of the copyright holder nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

For additional information about this project contact : pascucci@acm.org
For support : support@visus.net
-----------------------------------------------------------------------------*/

#include <Visus/NetSocket.h>
#include <Visus/StringTree.h>
#include "osdep.hxx"


namespace Visus {


int  NetSocket::Defaults::send_buffer_size=0;
int  NetSocket::Defaults::recv_buffer_size = 0;
bool NetSocket::Defaults::tcp_no_delay=true;

//////////////////////////////////////////////////////////////
class NetSocket::Pimpl
{
public:

  VISUS_NON_COPYABLE_CLASS(Pimpl)

     int socketfd=-1;

  //constructor
  Pimpl() {
  }

  //destructor
  ~Pimpl()
  { 
    if (socketfd>=0)
      closesocket(socketfd);
  }

  //getNativeHandle
  void* getNativeHandle() 
  {return &socketfd;}

  //close
  void close() 
  {
    if (socketfd<0) return;
    closesocket(socketfd);
    socketfd = -1;
  }

  //shutdownSend
  void shutdownSend() 
  {
    if (socketfd<0) return;
    ::shutdown(socketfd, SHUT_WR);
  }

  //connect
  bool connect(String url_) 
  {
    Url url(url_);

    close();

    this->socketfd = (int)socket(AF_INET, SOCK_STREAM, 0);
    if (this->socketfd < 0)
    {
      PrintError("connect failed, reason socket(AF_INET, SOCK_STREAM, 0) returned <0",strerror(errno));
      return false;
    }

    struct sockaddr_in serv_addr;
    memset((char *)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
  
    serv_addr.sin_addr.s_addr = getIPAddress(url.getHostname().c_str());

    configureOptions();

    serv_addr.sin_port = htons(url.getPort());
    if (::connect(this->socketfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0)
    {
      PrintError("connect failed, cannot connect",strerror(errno));
      return false;
    }

    return true;
  }

  //bind
  bool bind(String url_) 
  {
    close();

    Url url(url_);
    VisusAssert(url.getHostname()=="*" || url.getHostname()=="localhost" || url.getHostname()=="127.0.0.1");

    this->socketfd = (int)socket(AF_INET, SOCK_STREAM, 0);
    if (this->socketfd<0)
    {
      PrintError("bind failed (socketfd<0) a server-side socket", strerror(errno));
      return false;
    }

    if (bool bReuseAddress=true)
    {
      const int reuse_addr = 1;
      setsockopt(this->socketfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse_addr, sizeof(reuse_addr));
    }

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons((unsigned short)url.getPort());
    sin.sin_addr.s_addr = getIpCat(INADDR_ANY);
    if (::bind(this->socketfd, (struct sockaddr*)(&sin), sizeof(struct sockaddr)))
    {
      close();
      PrintError("bind failed. can't bind for server-side socket",strerror(errno));
      return false;
    }

    const int max_connections = SOMAXCONN;
    this->configureOptions();
    if (::listen(this->socketfd, max_connections))
    {
      close();
      PrintError("listen failed. Can't listen (listen(...) method) for server-side socket",strerror(errno));
      return false;
    }

    PrintInfo("NetSocket::bind ok url",url);
    return true;
  }

  //acceptConnection
  SharedPtr<NetSocket> acceptConnection() 
  {
    if (socketfd<0) 
      return SharedPtr<NetSocket>();

    UniquePtr<Pimpl> pimpl(new Pimpl());

    struct sockaddr_in client_addr;
    socklen_t sin_size = sizeof(struct sockaddr_in);

    pimpl->socketfd = (int)::accept(this->socketfd, (struct sockaddr*)(&client_addr), &sin_size);
    if (pimpl->socketfd < 0)
    {
      PrintError("accept failed ",strerror(errno));
      return SharedPtr<NetSocket>();
    }

    pimpl->configureOptions();

    PrintInfo("NetSocket accepted new connection");
    return std::make_shared<NetSocket>(pimpl.release());
  }

  //sendRequest
  bool sendRequest(NetRequest request) 
  {
    String headers=request.getHeadersAsString();

    if (!sendBytes((const Uint8*)headers.c_str(),(int)headers.size()))
      return false;

    if (request.body && request.body->c_size())
    {
      VisusAssert(request.getContentLength()==request.body->c_size());
      if (!sendBytes(request.body->c_ptr(),(int)request.body->c_size()))
        return false;
    }

    return true;
  }

  //sendResponse
  bool sendResponse(NetResponse response) 
  {
    String headers=response.getHeadersAsString();

    if (!sendBytes((const Uint8*)headers.c_str(),(int)headers.size()))
      return false;

    if (response.body && response.body->c_size())
    {
      VisusAssert(response.getContentLength()==response.body->c_size());
      if (!sendBytes(response.body->c_ptr(),(int)response.body->c_size()))
        return false;
    }

    return true;
  }

  //receiveRequest
  NetRequest receiveRequest() 
  {
    String headers;
    headers.reserve(8192);
    while (!StringUtils::endsWith(headers, "\r\n\r\n"))
    {
      if (headers.capacity() == headers.size())
        headers.reserve(headers.capacity() << 1);

      char ch = 0;
      if (!receiveBytes((unsigned char*)&ch, 1))
        return NetRequest();

      headers.push_back(ch);
    }

    NetRequest request;
    if (!request.setHeadersFromString(headers))
      return NetRequest();
    
    if (int ContentLength=(int)request.getContentLength())
    {
      request.body=std::make_shared<HeapMemory>();
      if (!request.body->resize(ContentLength,__FILE__,__LINE__))
        return NetRequest();

      if (!receiveBytes(request.body->c_ptr(), (int)ContentLength))
        return NetRequest();
    }

    return request;
  }

  //receiveResponse
  NetResponse receiveResponse() 
  {
    String headers;
    headers.reserve(8192);
    while (!StringUtils::endsWith(headers, "\r\n\r\n"))
    {
      if (headers.capacity() == headers.size())
        headers.reserve(headers.capacity() << 1);

      char ch = 0;
      if (!receiveBytes((unsigned char*)&ch, 1))
        return NetResponse();

      headers.push_back(ch);
    }

    NetResponse response;
    if (!response.setHeadersFromString(headers))
      return NetResponse();
    
    if (int ContentLength=(int)response.getContentLength())
    {
      response.body=std::make_shared<HeapMemory>();
      if (!response.body->resize(ContentLength,__FILE__,__LINE__))
        return NetResponse();

      if (!receiveBytes(response.body->c_ptr(), (int)ContentLength))
        return NetResponse();
    }

    return response;
  }

private:

  //configureOptions
  void configureOptions()
  {
    if (auto value= NetSocket::Defaults::send_buffer_size)
      setSendBufferSize(value);

    if (auto value= NetSocket::Defaults::recv_buffer_size)
      setReceiveBufferSize(value);

    //I think the no delay should be always enabled in Visus
    setNoDelay(NetSocket::Defaults::tcp_no_delay);
  }

  //setNoDelay
  void setNoDelay(bool bValue)
  {
    int value=bValue?1:0;
    setsockopt(this->socketfd, IPPROTO_TCP, TCP_NODELAY, (const char*)&value, sizeof(value));
  }

  //setSendBufferSize
  void setSendBufferSize(int value)
  {
    setsockopt(this->socketfd, SOL_SOCKET, SO_SNDBUF, (const char*)&value, sizeof(value));
  }

  //setReceiveBufferSize
  void setReceiveBufferSize(int value)
  {
    setsockopt(this->socketfd, SOL_SOCKET, SO_RCVBUF, (const char*)&value, sizeof(value));
  }

  //sendBytes
  bool sendBytes(const unsigned char *buf, int len)
  {
    if (socketfd<0) 
      return false;

    int flags=0;
  
    while (len)
    {
      int n = (int)::send(socketfd, (const char*)buf, len, flags);
      if (n <= 0)
      {
        PrintError("Failed to send data to socket errdescr",getSocketErrorDescription(n));
        return false;
      }
      buf += n;
      len -= n;
    }
    return true;
  }

  //receiveBytes
  bool receiveBytes(unsigned char *buf, int len)
  {
    if (socketfd<0) 
      return false;

    int flags=0;

    while (len)
    {
      int n = (int)::recv(socketfd, (char*)buf, len, flags);
      if (n <= 0)
      {
        PrintError("Failed to recv data to socket errdescr",getSocketErrorDescription(n));
        return false;
      }
      buf += n;
      len -= n;
    }
    return true;
  }

  //getSocketErrorDescription
  static inline const char* getSocketErrorDescription(int retcode)
  {
    switch (retcode)
    {
    case EACCES:return "EACCES";
    case EAGAIN:return "EAGAIN";
    #if EWOULDBLOCK!=EAGAIN
    case EWOULDBLOCK:return "EWOULDBLOCK";
    #endif
    case EBADF:return "EBADF";
    case ECONNRESET:return "ECONNRESET";
    case EDESTADDRREQ:return "EDESTADDRREQ";
    case EFAULT:return "EFAULT";
    case EINTR:return "EINTR";
    case EINVAL:return "EINVAL";
    case EISCONN:return "EISCONN";
    case EMSGSIZE:return "EMSGSIZE";
    case ENOBUFS:return "ENOBUFS";
    case ENOMEM:return "ENOMEM";
    case ENOTCONN:return "ENOTCONN";
    case ENOTSOCK:return "ENOTSOCK";
    case EOPNOTSUPP:return "EOPNOTSUPP";
    case EPIPE:return "EPIPE";
    case ECONNREFUSED: return "ECONNREFUSED";
    case EDOM: return "EDOM";
    case ENOPROTOOPT: return "ENOPROTOOPT";
    }
    return "Unknown";
  }

  //getIPAddress
  static inline unsigned long getIPAddress(const char* pcHost)
  {
    u_long nRemoteAddr = inet_addr(pcHost);
    if (nRemoteAddr == INADDR_NONE)
    {
      hostent* pHE = gethostbyname(pcHost);
      if (pHE == 0) return INADDR_NONE;
      nRemoteAddr = *((u_long*)pHE->h_addr_list[0]);
    }
    return nRemoteAddr;
  }

};

////////////////////////////////////////////////////////////////////
NetSocket::NetSocket() 
{
  this->pimpl=new Pimpl();
}


NetSocket::NetSocket(Pimpl* pimpl_) : pimpl(pimpl_) 
{
}

NetSocket::~NetSocket() {
  if (pimpl) delete pimpl;
}

void NetSocket::shutdownSend() {
  return pimpl->shutdownSend();
}

void NetSocket::close() {
  return pimpl->close();
}

bool NetSocket::connect(String url) {
  return pimpl->connect(url);
}

bool NetSocket::bind(String url) {
  return pimpl->bind(url);
}

SharedPtr<NetSocket> NetSocket::acceptConnection() {
  return pimpl->acceptConnection();
}

bool NetSocket::sendRequest(NetRequest request) {
  return pimpl->sendRequest(request);
}

bool NetSocket::sendResponse(NetResponse response) {
  return pimpl->sendResponse(response);
}

NetRequest NetSocket::receiveRequest() {
  return pimpl->receiveRequest();
}

NetResponse NetSocket::receiveResponse() {
  return pimpl->receiveResponse();
}


} //namespace Visus

