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

#include <vector>
#include "XrdClientUrlInfo.hh"
#include "XrdClientConst.hh"

using namespace std;



typedef vector<XrdClientUrlInfo*> UrlArray;


// Manages a set of XrdClientUrlInfo objects
// Plus
//  funcs to resolve multiple addresses from the dns
//  funcs to pick urls sequantially and randomly
class XrdClientUrlSet {
 private:
   UrlArray fUrlArray, fTmpUrlArray;
   string fPathName;
   
   bool fIsValid;

   unsigned int fSeed;
   double GetRandom(int seed = 0);

   void CheckPort(string &machine);

   // Takes a sequence of hostname and resolves it into a vector of UrlInfo
   void ConvertDNSAliases(UrlArray& urls, string list, string fname);
   void ConvertSingleDNSAlias(UrlArray& urls, string hostname, string fname);

 public:
   XrdClientUrlSet(XrdClientUrlInfo);
   ~XrdClientUrlSet();

   // Returns the final resolved list of servers
   string GetServers() {
      string s;

      for ( unsigned int i = 0; i < fUrlArray.size(); i++ ) {
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

   // Returns the number of urls
   int Size() { return fUrlArray.size(); }

   // Returns the pathfile extracted from the CTOR's argument
   string GetFile() { return fPathName; }

   bool IsValid() { return fIsValid; }    // Return kFALSE if the CTOR's argument is malformed

};

#endif
