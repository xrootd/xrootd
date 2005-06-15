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

//       $Id$

#ifndef _XRC_URLSET_H
#define _XRC_URLSET_H

#include "XrdClient/XrdClientUrlInfo.hh"
#include "XrdClient/XrdClientConst.hh"
#include "XrdClient/XrdClientVector.hh"

using namespace std;


typedef XrdClientVector<XrdClientUrlInfo*> UrlArray;


// Manages a set of XrdClientUrlInfo objects
// Plus
//  funcs to resolve multiple addresses from the dns
//  funcs to pick urls sequantially and randomly
class XrdClientUrlSet {
 private:
   UrlArray fUrlArray, fTmpUrlArray;
   XrdClientString fPathName;
   
   bool fIsValid;

   unsigned int fSeed;
   double GetRandom(int seed = 0);

   void CheckPort(XrdClientString &machine);

   // Takes a sequence of hostname and resolves it into a vector of UrlInfo
   void ConvertDNSAliases(UrlArray& urls, XrdClientString list, XrdClientString fname);
   void ConvertSingleDNSAlias(UrlArray& urls, XrdClientString hostname, XrdClientString fname);

 public:
   XrdClientUrlSet(XrdClientUrlInfo);
   ~XrdClientUrlSet();

   // Returns the final resolved list of servers
   XrdClientString GetServers() {
      XrdClientString s;

      for ( int i = 0; i < fUrlArray.GetSize(); i++ ) {
	 s += fUrlArray[i]->Host;
	 s += "\n";
      }

      return s;
   }

   // Gets the subsequent Url, the one after the last given
   XrdClientUrlInfo *GetNextUrl();

   // From the remaining urls we pick a random one. Without reinsert.
   //  i.e. while there are not considered urls, never pick an already seen one
   XrdClientUrlInfo *GetARandomUrl();

   void Rewind();
   void ShowUrls();
   void EraseUrl(XrdClientUrlInfo *url);

   // Returns the number of urls
   int Size() { return fUrlArray.GetSize(); }

   // Returns the pathfile extracted from the CTOR's argument
   XrdClientString GetFile() { return fPathName; }

   bool IsValid() { return fIsValid; }    // Return kFALSE if the CTOR's argument is malformed

};

#endif
