//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientUrlSet                                                      // 
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
// Adapted from TXNetFile (root.cern.ch) originally done by             //
// Alvise Dorigo, Fabrizio Furano, INFN Padova, 2003                    //
// Revised by G. Ganis, CERN, June 2005                                 //
//                                                                      //
// A container for multiple urls to be resolved through DNS aliases     //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

//       $Id$

#ifndef _XRC_URLSET_H
#define _XRC_URLSET_H

#include "XrdClient/XrdClientConst.hh"
#include "XrdClient/XrdClientVector.hh"

using namespace std;

class XrdClientString;
class XrdClientUrlInfo;

typedef XrdClientVector<XrdClientUrlInfo*> UrlArray;

//
// Manages a set of XrdClientUrlInfo objects and provides a set
// of utilities to resolve multiple addresses from the dns
// and to pick urls sequentially and randomly an url
//

class XrdClientUrlSet {
private:
   UrlArray        fUrlArray;
   UrlArray        fTmpUrlArray;
   XrdClientString    fPathName;
   
   bool            fIsValid;
   unsigned int    fSeed;

   void            CheckPort(int &port);
   void            ConvertDNSAlias(UrlArray& urls, XrdClientString proto,
                                   XrdClientString host, XrdClientString file);
   double          GetRandom(int seed = 0);

public:
   XrdClientUrlSet(XrdClientString urls);
   ~XrdClientUrlSet();

   // Returns the final resolved list of servers
   XrdClientString   GetServers();

   // Gets the subsequent Url, the one after the last given
   XrdClientUrlInfo *GetNextUrl();

   // From the remaining urls we pick a random one. Without reinsert.
   //  i.e. while there are not considered urls, never pick an already seen one
   XrdClientUrlInfo *GetARandomUrl();

   void              Rewind();
   void              ShowUrls();
   void              EraseUrl(XrdClientUrlInfo *url);

   // Returns the number of urls
   int               Size() { return fUrlArray.GetSize(); }

   // Returns the pathfile extracted from the CTOR's argument
   XrdClientString   GetFile() { return fPathName; }

   bool              IsValid() { return fIsValid; } // Spot malformations

};

#endif
