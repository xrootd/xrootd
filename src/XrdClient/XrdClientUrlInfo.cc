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

#include "XrdClientUrlInfo.hh"
#include "XrdNet/XrdNetDNS.hh"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>

//_____________________________________________________________________________
XrdClientUrlInfo::XrdClientUrlInfo() {
   Clear();
}


//_____________________________________________________________________________
XrdClientUrlInfo::XrdClientUrlInfo(string url) {
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
void XrdClientUrlInfo::TakeUrl(string url) {
   // Parse url character string and split in its different subcomponents.
   // Use IsValid() to check if URL is legal.
   //
   // url: [proto://][user[:passwd]@]host[:port]/file
   //

   unsigned int p;
   string u(url);

   Clear();

   if (!u.size()) return;

   // Get protocol
   if ( (p = u.find("://")) != string::npos) {
      Proto = u.substr(0, p);
      u = u.substr(p+3, string::npos);
   }

   if (!u.size()) {
      Clear();
      return;
   }

   // Get user/pwd
   if ( (p = u.find("@")) != string::npos) {
      string tmp;
      unsigned int p2;

      tmp = u.substr(0, p);
      if ( (p2 = u.find(":")) != string::npos ) {
	 User = tmp.substr(0, p2);
	 Passwd = tmp.substr(p2+1, string::npos);
      }
      else
	 User = tmp;

      
      u = u.substr(p+1, string::npos);
   }

   if (!u.size()) {
      Clear();
      Port = -1;
      return;
   }

   // Get host:port
   p = u.find("/");
   HostWPort = u.substr(0, p);

   {
      unsigned int p2;

      if ( (p2 = HostWPort.find(":")) != string::npos ) {	 
	 Host = HostWPort.substr(0, p2);
	 Port = atoi(HostWPort.substr(p2+1, string::npos).c_str());
      }
      else {
	 Host = HostWPort;
	 Port = 0;
      }

      
      if (p != string::npos)
	 u = u.substr(p+1, string::npos);
      else return;
   }

   // Get pathfile
   File = u;

}

//_____________________________________________________________________________
string XrdClientUrlInfo::GetUrl() {
   string s;

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
