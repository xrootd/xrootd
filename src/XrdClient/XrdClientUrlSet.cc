//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientUrlSet                                                            // 
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
// Adapted from TXNetFile (root.cern.ch) originally done by             //
//  Alvise Dorigo, Fabrizio Furano                                      //
//          INFN Padova, 2003                                           //
//                                                                      //
// A container for multiple urls to be resolved through DNS aliases     //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#include "XrdClientUrlSet.hh"

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

#include "XrdClientDebug.hh"

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

   unsigned int p;
   UrlArray urlArray;
   string listOfMachines;

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

   listOfMachines = tmpurl.HostWPort;

   // remove trailing "," that would introduce a null host
   while ( (p = listOfMachines.rfind(",")) == listOfMachines.size()-1 )
      listOfMachines.erase(p, string::npos);

   // remove leading "," that would introduce a null host
   while ( (p == listOfMachines.find(",")) == 0)
      listOfMachines.erase(0,1);

   Info( XrdClientDebug::kUSERDEBUG, "XrdClientUrlSet", "List of servers to connect to is [" <<
	listOfMachines << "]" );

   //
   // Set fPathName
   //
   fPathName = tmpurl.File;

   // If at this point we have a strange pathfile, then it's bad
   if ( (fPathName.size() <= 1) || (fPathName == "/") ) {
      Error("TXUrl", "Malformed pathfile " << fPathName);
      fIsValid = FALSE;
      return;
   }

   Info(XrdClientDebug::kHIDEBUG, "XrdClientUrlSet", "Remote file to open is '" <<
	fPathName << "'");
 
   if (fIsValid) {
      ConvertDNSAliases(fUrlArray, listOfMachines, fPathName);

      if (fUrlArray.size() <= 0)
	 fIsValid = FALSE;

      ShowUrls();
   }

}

//_____________________________________________________________________________
XrdClientUrlSet::~XrdClientUrlSet()
{
   fTmpUrlArray.clear();

   for( unsigned int i=0; i < fUrlArray.size(); i++)
      SafeDelete( fUrlArray[i] );

   fUrlArray.clear();
}

//_____________________________________________________________________________
XrdClientUrlInfo *XrdClientUrlSet::GetNextUrl()
{
   // Returns the next url object pointer in the array.
   // After the last object is returned, the array is rewind-ed.
   // Now implemented as a pick from the tmpUrlArray queue

   XrdClientUrlInfo *retval;

   if ( !fTmpUrlArray.size() ) Rewind();

   retval = fTmpUrlArray.back();

   fTmpUrlArray.pop_back();

   return retval;
}

//_____________________________________________________________________________
void XrdClientUrlSet::Rewind()
{
   // Rebuilds tmpUrlArray, i..e the urls that have to be picked
   fTmpUrlArray.clear();

   for(unsigned int i=0; i <= fUrlArray.size()-1; i++)
      fTmpUrlArray.push_back( fUrlArray[i] );
}

//_____________________________________________________________________________
XrdClientUrlInfo *XrdClientUrlSet::GetARandomUrl()
{
   XrdClientUrlInfo *retval;
   int rnd;

   if (!fTmpUrlArray.size()) Rewind();

   for (int i=0; i < 10; i++)
#ifdef __sun
      rnd = irint(GetRandom() * fTmpUrlArray.size()) % fTmpUrlArray.size();
#else
      rnd = lrint(GetRandom() * fTmpUrlArray.size()) % fTmpUrlArray.size();
#endif

   // Returns a random url from the ones that have to be picked
   // When all the urls have been picked, we restart from the full url set

   UrlArray::iterator it = fTmpUrlArray.begin() + rnd;

   retval = *it;
   fTmpUrlArray.erase(it);

   return retval;
}

//_____________________________________________________________________________
void XrdClientUrlSet::ShowUrls()
{
   // Prints the list of urls

   Info(XrdClientDebug::kUSERDEBUG, "ShowUrls",
	"The converted URLs count is " << fUrlArray.size() );

   for(unsigned int i=0; i < fUrlArray.size(); i++)
      Info(XrdClientDebug::kUSERDEBUG, "ShowUrls",
	   "URL n." << i+1 << ": "<< fUrlArray[i]->GetUrl() << "."); 

}

//_____________________________________________________________________________
void XrdClientUrlSet::CheckPort(string &machine)
{
   // Checks the validity of port in the given host[:port]
   // Eventually completes the port if specified in the services file
   unsigned int p = machine.find(':');

   if(p == string::npos) {
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
	      "Found tcp port " <<  svc->s_port << ".");
	 
	 machine += ":";
	 machine += svc->s_port;
      }

   } else {
      // The port seems there

      string tmp = machine.substr(p+1, string::npos);

      if(tmp == "")
	 Error("checkPort","The specified tcp port is empty for " <<  machine)
      else {

	 for(unsigned int i = 0; i <= tmp.size()-1; i++)
	    if(!isdigit(tmp[i])) {
	       Error("checkPort","The specified tcp port is not numeric for " <<
		     machine);

	    }
      }
   }
}

//_____________________________________________________________________________
void XrdClientUrlSet::ConvertSingleDNSAlias(UrlArray& urls, string hostname, 
                                                    string fname)
{
   // Converts a single host[:port] into an array of TUrl
   // The new Turls are appended to the given UrlArray


   bool specifiedPort;
   string tmpaddr;
  
   specifiedPort = ( hostname.find(':') != string::npos );
  
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

      urls.push_back( newurl );

   }

   
}

//_____________________________________________________________________________
void XrdClientUrlSet::ConvertDNSAliases(UrlArray& urls, string list, string fname)
{
   // Given a list of comma-separated host[:port]
   // every entry is resolved via DNS into its aliases

   unsigned int colonPos;
   string lst(list);

   lst += ",";

   while(lst.size() > 0) {
      colonPos = lst.find(',');
      if ((colonPos != string::npos) && (colonPos <= lst.size())) {
	 string tmp(lst);

	 tmp.erase(colonPos, string::npos);
	 lst.erase(0,colonPos+1);

	 CheckPort(tmp);
	 ConvertSingleDNSAlias(urls, tmp, fname);
      }
	else break;

   }
      
}











