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

//       $Id$

#include "XrdClient/XrdClientSock.hh"
#include "XrdClient/XrdClientFactory.hh"
#include "XrdClient/XrdClientUrlInfo.hh"

//_____________________________________________________________________________
XrdClientSock::XrdClientSock(XrdClientUrlInfo u, int ws)
{
   // Constructor

   fSockImp = XrdClientGetFactory()->CreateSockImp(u,ws);
}

//_____________________________________________________________________________
void XrdClientSock::Disconnect()
{
   // Close the connection

   if (fSockImp)
      fSockImp->Disconnect();
}

//_____________________________________________________________________________
int XrdClientSock::RecvRaw(void* buffer, int length)
{
   // Read bytes following carefully the timeout rules

   if (fSockImp)
      return fSockImp->RecvRaw(buffer,length);
   return -1;
}

//_____________________________________________________________________________
int XrdClientSock::SendRaw(const void* buffer, int length)
{
   // Write bytes following carefully the timeout rules
   // (writes will not hang)

   if (fSockImp)
      return fSockImp->SendRaw(buffer,length);
   return -1;
}

//_____________________________________________________________________________
void XrdClientSock::TryConnect()
{
   // Try connection

   if (fSockImp)
      fSockImp->TryConnect();
}

//_____________________________________________________________________________
bool XrdClientSock::IsConnected()
{
   // Test connection

   if (fSockImp)
      return fSockImp->IsConnected();
   return 0;
}
