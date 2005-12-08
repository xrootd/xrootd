//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientUrlInfo                                                     // 
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
// Adapted from TXNetFile (root.cern.ch) originally done by             //
// Alvise Dorigo, Fabrizio Furano, INFN Padova, 2003                    //
// Revised by G. Ganis, CERN, June 2005                                 //
//                                                                      //
// Class handling information about an url                              //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

//       $Id$

#include "XrdClient/XrdClientDebug.hh"
#include "XrdClient/XrdClientUrlInfo.hh"
#include "XrdNet/XrdNetDNS.hh"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>

//_____________________________________________________________________________
XrdClientUrlInfo::XrdClientUrlInfo()
{
   // Default constructor

   Clear();
}

//_____________________________________________________________________________
XrdClientUrlInfo::XrdClientUrlInfo(const char *url)
{
   // Constructor from a string specifying an url or multiple urls in the
   // form:
   //         [proto://][user1@]host1:port1[,[user2@]host2:port2, ... ,
   //                   [userN@]hostN:portN]]/pathfile

   Clear();
   TakeUrl(XrdClientString(url));
}

//_____________________________________________________________________________
XrdClientUrlInfo::XrdClientUrlInfo(const XrdClientString url)
{
   // Constructor from a string specifying an url or multiple urls in the
   // form:
   //         [proto://][user1@]host1:port1[,[user2@]host2:port2, ... ,
   //                   [userN@]hostN:portN]]/pathfile

   Clear();
   TakeUrl(url);
}

//______________________________________________________________________________
XrdClientUrlInfo::XrdClientUrlInfo(const XrdClientUrlInfo &url)
{
   // Copy constructor

   Proto    = url.Proto;
   User     = url.User;
   Passwd   = url.Passwd;
   Host     = url.Host;
   HostWPort= url.HostWPort;
   HostAddr = url.HostAddr;
   Port     = url.Port;
   File     = url.File;

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

//______________________________________________________________________________
XrdClientUrlInfo& XrdClientUrlInfo::operator=(const XrdClientString url)
{
   // Assign url to local url.

   this->TakeUrl(url);

   return (*this);
}

//______________________________________________________________________________
XrdClientUrlInfo& XrdClientUrlInfo::operator=(const XrdClientUrlInfo url)
{
   // Assign url to local url.

   Proto    = url.Proto;
   User     = url.User;
   Passwd   = url.Passwd;
   Host     = url.Host;
   HostWPort= url.HostWPort;
   HostAddr = url.HostAddr;
   Port     = url.Port;
   File     = url.File;

   return (*this);
}

//_____________________________________________________________________________
void XrdClientUrlInfo::TakeUrl(XrdClientString u)
{
   // Parse url character string and split in its different subcomponents.
   // Use IsValid() to check if URL is legal.
   // Url format:
   //             [proto://][user[:passwd]@]host:port/pathfile
   //
   int p1 = 0, p2 = STR_NPOS, p3 = STR_NPOS, left = u.GetSize();

   Clear();

   Info(XrdClientDebug::kHIDEBUG,"TakeUrl", "parsing url: " << u);

   if (u.GetSize() <= 0) return;

   // Get protocol
   if ((p2 = u.Find((char *)"://")) != STR_NPOS) {
      Proto = u.Substr(p1, p2);
      Info(XrdClientDebug::kHIDEBUG,"TakeUrl", "   Proto:   " << Proto);
      // Update start of search range and remaining length
      p1 = p2 + 3;
      left -= p1;
   }
   if (left <= 0) {
      Clear();
      return;
   }

   // Store the whole "[user[:passwd]@]host:port" thing in HostWPort
   if ((p2 = u.Find((char *)"/",p1)) != STR_NPOS) {
      if (p2 > p1) {
         HostWPort = u.Substr(p1, p2);
         // Update start of search range and remaining length
         p1 = p2+1;
         left -= p1;
      }
   } else {
      HostWPort = u.Substr(p1);
      left = 0;
   }
   Info(XrdClientDebug::kHIDEBUG,"TakeUrl", "   HostWPort:   " << HostWPort);

   // Get pathfile
   if (left > 0)
     File = u.Substr(p1);
   Info(XrdClientDebug::kHIDEBUG,"TakeUrl", "   File:   " << File);

   //
   // Resolve username, passwd, host and port.
   p1 = 0;
   left = HostWPort.GetSize();
   // Get user/pwd
   if ((p2 = HostWPort.Find((char *)"@",p1)) != STR_NPOS) {
      p3 = HostWPort.Find((char *)":",p1);
      if (p3 != STR_NPOS && p3 < p2) {
         User = HostWPort.Substr(p1,p3);
         Passwd = HostWPort.Substr(p3+1,p2);
         Info(XrdClientDebug::kHIDEBUG,"TakeUrl", "   Passwd: " << Passwd);
      } else {
         User = HostWPort.Substr(p1,p2);
      }
      Info(XrdClientDebug::kHIDEBUG,"TakeUrl", "   User:   " << User);
      // Update start of search range and remaining length
      p1 = p2 + 1;
      left -= p1;
   }

   // Get host:port
   if ((p2 = HostWPort.Find((char *)":",p1)) != STR_NPOS ) {	 
      Host = HostWPort.Substr(p1, p2);
      Port = strtol((HostWPort.c_str())+p2+1, (char **)0, 10);
   } else {
      Host = HostWPort.Substr(p1);
      Port = 0;
   }
   Info(XrdClientDebug::kHIDEBUG,"TakeUrl", "   Host:   " << Host);
   Info(XrdClientDebug::kHIDEBUG,"TakeUrl", "   Port:   " << Port);
}

//_____________________________________________________________________________
XrdClientString XrdClientUrlInfo::GetUrl()
{
   // Get full url
   // The fields might have been modified, so the full url
   // must be reconstructed

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

//_____________________________________________________________________________
void XrdClientUrlInfo::SetAddrFromHost() 
{
   struct sockaddr_in ip[2];
   char buf[255], **errmsg = 0;

   int numaddr = XrdNetDNS::getHostAddr((char *)Host.c_str(),
                           (struct sockaddr *)ip, 1, errmsg);
   if (numaddr > 0)
      HostAddr = inet_ntop(ip[0].sin_family, &ip[0].sin_addr, buf, sizeof(buf));
}
