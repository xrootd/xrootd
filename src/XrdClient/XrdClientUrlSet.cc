//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientUrlSet                                                      // 
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
// Adapted from TXNetFile (root.cern.ch) originally done by             //
//  Alvise Dorigo, Fabrizio Furano                                      //
//          INFN Padova, 2003                                           //
//                                                                      //
// A container for multiple urls to be resolved through DNS aliases     //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#include "XrdClient/XrdClientUrlSet.hh"

#include <math.h>
#include <stdio.h>
#include <iostream>
#include <ctype.h>               // needed by isdigit()
#include <netdb.h>               // needed by getservbyname()
#include <netinet/in.h>          // needed by ntohs()
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "XrdNet/XrdNetDNS.hh"

#include <stdlib.h>
#include <resolv.h>
#include <arpa/nameser.h>
#include <sys/time.h>
#include <unistd.h>

#include "XrdClient/XrdClientDebug.hh"

#ifdef __sun
#include <sunmath.h>
#endif


using namespace std;

//_____________________________________________________________________________
double XrdClientUrlSet::GetRandom(int i)
{
//  Machine independent random number generator.
//  Produces uniformly-distributed floating points between 0 and 1.
//  Identical sequence on all machines of >= 32 bits.
//  Periodicity = 10**8
//  Universal version (Fred James 1985).
//  generates a number in ]0,1]

   const double kCONS = 4.6566128730774E-10;
   const int kMASK24  = 2147483392;

   fSeed *= 69069;      
   unsigned int jy = (fSeed&kMASK24); // Set lower 8 bits to zero to assure exact float
   if (jy) return kCONS*jy;
   return GetRandom();
}

//_____________________________________________________________________________
XrdClientUrlSet::XrdClientUrlSet(XrdClientUrlInfo tmpurl) : fIsValid(TRUE)
{
   // A container for multiple urls.
   // It creates an array of multiple urls parsing the argument Urls and
   //  resolving the DNS aliases
   //
   // Urls MUST be in the form:
   //
   //    root://[username1@]server1:port1[,[username2@]server2:port2, ... ,
   //           [usernameN@]serverN:portN]/pathfile
   //
   // Using the method GetNextUrl() the user can obtain the next TUrl object pointer in the array
   // (the array is cyclic).
   // Using the method GetARandomUrl() the user can obtain a random TUrl from the array


   UrlArray urlArray;
   XrdClientString listOfMachines;

   if (!tmpurl.IsValid()) {
      fIsValid = FALSE;
      return;
   }
  
   // Init of the random number generator
   fSeed = getpid();

   //
   // We assume the protol is "root://", because this 
   // must be the protocol for
   //


   if ( tmpurl.Proto != "root" ) {
      Error("TXUrl", "This is not a root protocol." );
      fIsValid = FALSE;
      return;
   }

   if ( tmpurl.HostWPort.GetSize() == 0 ) {
      Error("TXUrl", "Malformed pathfile (HostWPort is invalid)" );
      fIsValid = FALSE;
      return;
   }
   listOfMachines = tmpurl.HostWPort;

   // remove trailing "," that would introduce a null host
   while ( (listOfMachines.EndsWith((char *)",")) || (listOfMachines.EndsWith((char *)" ")) )
      listOfMachines.EraseFromEnd(1);

   // remove leading "," that would introduce a null host
   while ( listOfMachines.BeginsWith((char *)",") )
      listOfMachines.EraseFromStart(1);

   Info( XrdClientDebug::kUSERDEBUG, "XrdClientUrlSet", "List of servers to connect to is [" <<
	listOfMachines << "]" );

   //
   // Set fPathName
   //
   fPathName = tmpurl.File;

   // If at this point we have a strange pathfile, then it's bad
   if ( (fPathName.GetSize() <= 1) || (fPathName == "/") ) {
      Error("TXUrl", "Malformed pathfile " << fPathName);
      fIsValid = FALSE;
      return;
   }

   Info(XrdClientDebug::kHIDEBUG, "XrdClientUrlSet", "Remote file to open is '" <<
	fPathName << "'");
 
   if (fIsValid) {
      ConvertDNSAliases(fUrlArray, listOfMachines, fPathName);

      if (fUrlArray.GetSize() <= 0)
	 fIsValid = FALSE;

      ShowUrls();
   }

}

//_____________________________________________________________________________
XrdClientUrlSet::~XrdClientUrlSet()
{
   fTmpUrlArray.Clear();

   for( int i=0; i < fUrlArray.GetSize(); i++)
      delete fUrlArray[i];

   fUrlArray.Clear();
}

//_____________________________________________________________________________
XrdClientUrlInfo *XrdClientUrlSet::GetNextUrl()
{
   // Returns the next url object pointer in the array.
   // After the last object is returned, the array is rewind-ed.
   // Now implemented as a pick from the tmpUrlArray queue

   XrdClientUrlInfo *retval;

   if ( !fTmpUrlArray.GetSize() ) Rewind();

   retval = fTmpUrlArray.Pop_back();

   return retval;
}

//_____________________________________________________________________________
void XrdClientUrlSet::Rewind()
{
   // Rebuilds tmpUrlArray, i..e the urls that have to be picked
   fTmpUrlArray.Clear();

   for(int i=0; i <= fUrlArray.GetSize()-1; i++)
      fTmpUrlArray.Push_back( fUrlArray[i] );
}

//_____________________________________________________________________________
XrdClientUrlInfo *XrdClientUrlSet::GetARandomUrl()
{
   XrdClientUrlInfo *retval;
   int rnd;

   if (!fTmpUrlArray.GetSize()) Rewind();

   for (int i=0; i < 10; i++)
#ifdef __sun
      rnd = irint(GetRandom() * fTmpUrlArray.GetSize()) % fTmpUrlArray.GetSize();
#else
      rnd = lrint(GetRandom() * fTmpUrlArray.GetSize()) % fTmpUrlArray.GetSize();
#endif

   // Returns a random url from the ones that have to be picked
   // When all the urls have been picked, we restart from the full url set

   retval = fTmpUrlArray[rnd];
   fTmpUrlArray.Erase(rnd);

   return retval;
}

//_____________________________________________________________________________
void XrdClientUrlSet::ShowUrls()
{
   // Prints the list of urls

   Info(XrdClientDebug::kUSERDEBUG, "ShowUrls",
	"The converted URLs count is " << fUrlArray.GetSize() );

   for(int i=0; i < fUrlArray.GetSize(); i++)
      Info(XrdClientDebug::kUSERDEBUG, "ShowUrls",
	   "URL n." << i+1 << ": "<< fUrlArray[i]->GetUrl() << "."); 

}

//_____________________________________________________________________________
void XrdClientUrlSet::CheckPort(XrdClientString &machine)
{
   // Checks the validity of port in the given host[:port]
   // Eventually completes the port if specified in the services file
   int p = machine.Find((char *)":");

   if(p == STR_NPOS) {
      // Port not specified

      Info(XrdClientDebug::kHIDEBUG, "CheckPort", 
	   "TCP port not specified for host " << machine <<
	   ". Trying to get it from /etc/services...");

      struct servent *svc = getservbyname("rootd", "tcp");

      if(svc <= 0) {
	 Info(XrdClientDebug::kHIDEBUG, "CheckPort",
	      "Service rootd not specified in /etc/services;" <<
	      "using default IANA tcp port 1094");

	 machine += ":1094";

      } else {

	 Info(XrdClientDebug::kHIDEBUG, "CheckPort",
	      "Found tcp port " <<  ntohs(svc->s_port) << ".");
	 
	 machine += ":";
	 machine += ntohs(svc->s_port);
      }

   } else {
      // The port seems there

      XrdClientString tmp = machine.Substr(p+1, STR_NPOS);

      if(tmp == "")
	 Error("checkPort","The specified tcp port is empty for " <<  machine)
      else {

	 for(int i = 0; i <= tmp.GetSize()-1; i++)
	    if(!isdigit(tmp[i])) {
	       Error("checkPort","The specified tcp port is not numeric for " <<
		     machine);

	    }
      }
   }
}

//_____________________________________________________________________________
void XrdClientUrlSet::ConvertSingleDNSAlias(UrlArray& urls, XrdClientString hostname, 
                                                    XrdClientString fname)
{
   // Converts a single host[:port] into an array of urls
   // The new urls are appended to the given UrlArray


   bool specifiedPort;
   XrdClientString tmpaddr;
  
   specifiedPort = ( hostname.Find((char *)":") != STR_NPOS );
  
   XrdClientUrlInfo tmp(hostname);
   specifiedPort = tmp.Port > 0;

   if(specifiedPort) {

      Info(XrdClientDebug::kHIDEBUG, "ConvertSingleDNSAlias","Resolving " <<
	   tmp.Host << ":" << tmp.Port);

   } else
      Info(XrdClientDebug::kHIDEBUG, "ConvertSingleDNSAlias","Resolving " <<
	   tmp.Host);


   struct sockaddr_in ip[10];
   char *hosterrmsg;
   XrdClientUrlInfo *newurl;

   // From the hostname, in x.y.z.w form or the ascii hostname, get the list of addresses
   int numaddr = XrdNetDNS::getHostAddr((char *)tmp.Host.c_str(),
					(struct sockaddr *)ip, 10, &hosterrmsg);
   for (int i = 0; i < numaddr; i++ ) {
      char buf[255];
      char *names[100];
      int hn;

      // Create a copy of the main urlinfo with the new data
      newurl = new XrdClientUrlInfo();
      newurl->TakeUrl(tmp.GetUrl());
	    
      inet_ntop(ip[i].sin_family, &ip[i].sin_addr, buf, sizeof(buf));

      newurl->HostAddr = buf;
      hn = XrdNetDNS::getHostName((struct sockaddr&)ip[i], names, 1, &hosterrmsg);

      if (hn)
	 newurl->Host = names[0];
      else newurl->Host = newurl->HostAddr;
      
      Info(XrdClientDebug::kHIDEBUG, "ConvertSingleDNSAlias",
	   "Found host " << newurl->Host << " with addr " << newurl->HostAddr);

      urls.Push_back( newurl );

   }

   
}

//_____________________________________________________________________________
void XrdClientUrlSet::ConvertDNSAliases(UrlArray& urls, XrdClientString list, XrdClientString fname)
{
   // Given a list of comma-separated host[:port]
   // every entry is resolved via DNS into its aliases

   int colonPos;
   XrdClientString lst(list);

   lst += ",";

   while(lst.GetSize() > 0) {
      colonPos = lst.Find((char *)",");
      if ((colonPos != STR_NPOS) && (colonPos <= lst.GetSize())) {
	 XrdClientString tmp(lst);

	 tmp.EraseToEnd(colonPos);
	 lst.EraseFromStart(colonPos+1);

	 CheckPort(tmp);
	 ConvertSingleDNSAlias(urls, tmp, fname);
      }
	else break;

   }
      
}











