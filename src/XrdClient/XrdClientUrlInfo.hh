//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientUrlInfo                                                           // 
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
// Adapted from TXNetFile (root.cern.ch) originally done by             //
//  Alvise Dorigo, Fabrizio Furano                                      //
//          INFN Padova, 2003                                           //
//                                                                      //
// Class handling information about an url                              //
//                                                                      //
//////////////////////////////////////////////////////////////////////////


#ifndef _XRC_URLINFO_H
#define _XRC_URLINFO_H

#include <string>
#include <sys/types.h>

using namespace std;




// The information an url may contain
// Plus utilities for parsing and rebuilding an url
class XrdClientUrlInfo {
 public:
   string Proto, Passwd, User, Host, HostAddr, HostWPort, File;
   int Port;

   void Clear();
   void TakeUrl(string url);
   string GetUrl();

   XrdClientUrlInfo(string url);
   XrdClientUrlInfo();

   void SetAddrFromHost();

   inline bool IsValid() {
      return (Port >= 0);
   }

};


#endif
