//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientSockImp                                                     //
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
// Adapted from TXNetFile (root.cern.ch) originally done by             //
//  Alvise Dorigo, Fabrizio Furano                                      //
//          INFN Padova, 2003                                           //
//                                                                      //
// Client Socket with timeout features                                  //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#ifndef XRC_SOCKIMP_H
#define XRC_SOCKIMP_H

class XrdClientString;

class XrdClientSockImp {

public:
   XrdClientSockImp() {}
   virtual ~XrdClientSockImp() {}

   virtual int    RecvRaw(void* buffer, int length) = 0;
   virtual int    SendRaw(const void* buffer, int length) = 0;

   virtual void   TryConnect() = 0;

   virtual void   Disconnect() = 0;

   virtual bool   IsConnected() = 0;
};

#endif
