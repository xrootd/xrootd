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

#include "XrdClient/XrdClientUrlInfo.hh"
#include "XrdNet/XrdNetDNS.hh"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>

//_____________________________________________________________________________
XrdClientUrlInfo::XrdClientUrlInfo() {
   Clear();
}


//_____________________________________________________________________________
XrdClientUrlInfo::XrdClientUrlInfo(XrdClientString url) {
   TakeUrl(url);
}

//_____________________________________________________________________________
void XrdClientUrlInfo::Clear() {
   // Set defaults

   Proto    = "";
   User     = "";
   Passwd   = "";
   Host     = "";
   HostWPort= "";
   HostAddr = "";
   Port     = -1;
   File     = "/";
}

//_____________________________________________________________________________
void XrdClientUrlInfo::TakeUrl(XrdClientString url) {
   // Parse url character string and split in its different subcomponents.
   // Use IsValid() to check if URL is legal.
   //
   // url: [proto://][user[:passwd]@]host[:port]/file
   //

   int p;
   XrdClientString u(url);

   Clear();

   if (!u.GetSize()) return;

   // Get protocol
   if ( (p = u.Find((char *)"://")) != STR_NPOS) {
      Proto = u.Substr(0, p);
      u = u.Substr(p+3);
   }

   if (!u.GetSize()) {
      Clear();
      return;
   }

   // Get user/pwd
   if ( (p = u.Find((char *)"@")) != STR_NPOS) {
      XrdClientString tmp;
      int p2;

      tmp = u.Substr(0, p);
      if ( (p2 = u.Find((char *)":")) != STR_NPOS ) {
	 User = tmp.Substr(0, p2);
	 Passwd = tmp.Substr(p2+1, STR_NPOS);
      }
      else
	 User = tmp;

      
      u = u.Substr(p+1);
   }

   if (!u.GetSize()) {
      Clear();
      Port = -1;
      return;
   }

   // Get host:port
   p = u.Find((char *)"/");
   HostWPort = u.Substr(0, p);

   {
      int p2;

      if ( (p2 = HostWPort.Find((char *)":")) != STR_NPOS ) {	 
	 Host = HostWPort.Substr(0, p2);
	 Port = atoi(HostWPort.Substr(p2+1, STR_NPOS).c_str());
      }
      else {
	 Host = HostWPort;
	 Port = 0;
      }

      
      if (p != STR_NPOS)
	 u = u.Substr(p+1, STR_NPOS);
      else return;
   }

   // Get pathfile
   File = u;

}

//_____________________________________________________________________________
XrdClientString XrdClientUrlInfo::GetUrl() {
   XrdClientString s;

   if (Proto != "")
      s = Proto + "://";

   if (User != "") {
      s += User;

      if (Passwd != "") {
	 s += ":";
	 s += Passwd;
      }

      s += "@";
   }

   s += Host;

   if ( (Host != "") && (Port > 0) ) {
      char buf[256];
      sprintf(buf, "%d", Port);
      s += ":";
      s += buf;
   }

   if (File != "") {
      s += "/";
      s += File;
   }

   return s;
}

void XrdClientUrlInfo::SetAddrFromHost() 
{
   struct sockaddr_in ip[2];
   char buf[255], **errmsg = 0;

   int numaddr = XrdNetDNS::getHostAddr((char *)Host.c_str(), (struct sockaddr *)ip, 1, errmsg);

   if (numaddr > 0)
      HostAddr = inet_ntop(ip[0].sin_family, &ip[0].sin_addr, buf, sizeof(buf));

}
