//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientSock                                                        //
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
// Adapted from TXNetFile (root.cern.ch) originally done by             //
//  Alvise Dorigo, Fabrizio Furano                                      //
//          INFN Padova, 2003                                           //
//                                                                      //
// Client Socket with timeout features                                  //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#ifndef XRD_CLIENTSOCK_H
#define XRD_CLIENTSOCK_H

#include "XrdOuc/XrdOucSocket.hh"

#include "XrdClientUrlInfo.hh"
#include "XrdClientConst.hh"
#include <string>






struct XrdClientSockConnectParms {
   XrdClientUrlInfo TcpHost;
   int TcpWindowSize;
};

class XrdClientSock {

private:

   XrdClientSockConnectParms fHost;

   bool fConnected;

   int fSocket;

public:
   XrdClientSock(XrdClientUrlInfo Host);
   ~XrdClientSock();

   void           Create(string, int, int);

   int            RecvRaw(void* buffer, int length);
   int            SendRaw(const void* buffer, int length);

   void           TryConnect();

   bool           IsConnected() { return fConnected; };
};

#endif
