//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientUrlInfo                                                     // 
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
// Adapted from TXNetFile (root.cern.ch) originally done by             //
//  Alvise Dorigo, Fabrizio Furano                                      //
//          INFN Padova, 2003                                           //
//                                                                      //
// Class handling information about an url                              //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

//       $Id$

#ifndef _XRC_URLINFO_H
#define _XRC_URLINFO_H

#include <string>
using namespace std;




// The information an url may contain
// Plus utilities for parsing and rebuilding an url
class XrdClientUrlInfo {
 public:
   string Proto;
   string Passwd;
   string User;
   string Host;
   int Port;
   string HostAddr;
   string HostWPort;
   string File;

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
