//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientXrdSock                                                     //
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
// Adapted from TXNetFile (root.cern.ch) originally done by             //
//  Alvise Dorigo, Fabrizio Furano                                      //
//          INFN Padova, 2003                                           //
//                                                                      //
// Client Socket with timeout features                                  //
//                                                                      //
//////////////////////////////////////////////////////////////////////////


#ifndef XRC_XRDSOCK_H
#define XRC_XRDSOCK_H

#include <XrdClient/XrdClientSockImp.hh>

#include <XrdClient/XrdClientUrlInfo.hh>
#include <XrdClient/XrdClientConst.hh>

struct XrdClientSockConnectParms {
   XrdClientUrlInfo TcpHost;
   int TcpWindowSize;
};

class XrdClientXrdSock : public XrdClientSockImp {

private:

   XrdClientSockConnectParms fHost;
   bool                      fConnected;
   int fSocket;

public:
   XrdClientXrdSock(XrdClientUrlInfo Host, int windowsize = 0);
   virtual ~XrdClientXrdSock();

   int            RecvRaw(void* buffer, int length);
   int            SendRaw(const void* buffer, int length);

   void           TryConnect();

   void           Disconnect();

   bool           IsConnected() { return fConnected; };
};

#endif
