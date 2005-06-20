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

//           $Id$

#ifndef XRC_SOCK_H
#define XRC_SOCK_H

#include <XrdClient/XrdClientUrlInfo.hh>

struct XrdClientSockConnectParms {
   XrdClientUrlInfo TcpHost;
   int TcpWindowSize;
};

class XrdClientSock {

friend class XrdClientPhyConnection;

private:

   XrdClientSockConnectParms fHost;
   bool                      fConnected;
   int fSocket;

   int    SaveSocket() { int fd = fSocket; fSocket = -1; return fd; }

public:
   XrdClientSock(XrdClientUrlInfo host, int windowsize = 0);
   virtual ~XrdClientSock();

   int    RecvRaw(void* buffer, int length);
   int    SendRaw(const void* buffer, int length);

   void   TryConnect();

   void   Disconnect();

   bool   IsConnected() {return fConnected;}
};

#endif
