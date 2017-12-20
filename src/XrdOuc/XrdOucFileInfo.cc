/******************************************************************************/
/*                                                                            */
/*                     X r d O u c F i l e I n f o . c c                      */
/*                                                                            */
/* (c) 2015 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
/*                                                                            */
/* XRootD is free software: you can redistribute it and/or modify it under    */
/* the terms of the GNU Lesser General Public License as published by the     */
/* Free Software Foundation, either version 3 of the License, or (at your     */
/* option) any later version.                                                 */
/*                                                                            */
/* XRootD is distributed in the hope that it will be useful, but WITHOUT      */
/* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or      */
/* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public       */
/* License for more details.                                                  */
/*                                                                            */
/* You should have received a copy of the GNU Lesser General Public License   */
/* along with XRootD in a file called COPYING.LESSER (LGPL license) and file  */
/* COPYING (GPL license).  If not, see <http://www.gnu.org/licenses/>.        */
/*                                                                            */
/* The copyright holder's institutional names and contributor's names may not */
/* be used to endorse or promote products derived from this software without  */
/* specific prior written permission of the institution or contributor.       */
/******************************************************************************/

#include "XrdOucFileInfo.hh"

/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/
/******************************************************************************/
/*                          X r d O u c F I H a s h                           */
/******************************************************************************/
  
class XrdOucFIHash
{
public:
char         *hName;
char         *hValue;
XrdOucFIHash *next;

const char   *XrdhName();

             XrdOucFIHash(const char *hn, const char *hv, XrdOucFIHash *np=0)
                         : hName(strdup(hn)), hValue(strdup(hv)), next(np) {}

            ~XrdOucFIHash() {if (hName)  free(hName);
                             if (hValue) free(hValue);
                            }
};

const char *XrdOucFIHash::XrdhName()
{
   if (!strcmp(hName, "adler-32") || !strcmp(hName, "adler32")
   ||  !strcmp(hName, "adler")) return "a32";
   return hName;
}

/******************************************************************************/
/*                           X r d O u c F I U r l                            */
/******************************************************************************/
  
class XrdOucFIUrl
{
public:
char        *fUrl;
int          fPrty;
char         fCC[4];
XrdOucFIUrl *next;

             XrdOucFIUrl(const char *url, const char *cc=0, int pri=0)
                       : fUrl(strdup(url)), fPrty(pri), next(0)
                    {if (cc) {strncpy(fCC, cc, sizeof(fCC)-1); fCC[2] = 0;}
                        else  strcpy(fCC, "us");
                    }

           ~XrdOucFIUrl() {if (fUrl) free(fUrl);}
};

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/
  
XrdOucFileInfo::~XrdOucFileInfo()
{
   XrdOucFIHash *hdP, *hP = fHash;
   XrdOucFIUrl  *udP, *uP = fUrl;

// Destroy the hash list
//
   while((hdP = hP)) {hP = hP->next; delete hdP;}

// Destroy the url  list
//
   while((udP = uP)) {uP = uP->next; delete udP;}

// Free the memory allocated for fTargetName
//
   if( fTargetName ) free(fTargetName);

// Free memory allocated to the lfn
//
   if(fLfn) free(fLfn);
}

/******************************************************************************/
/*                             A d d D i g e s t                              */
/******************************************************************************/
  
void XrdOucFileInfo::AddDigest(const char *hname, const char *hval)
{
   int n;

// Chain in a new digest
//
   fHashNext = fHash = new XrdOucFIHash(hname, hval, fHash);

// Now make sure the hash type is lower case
//
   n = strlen(hname);
   for (int i = 0; i < n; i++) fHash->hName[i] = tolower(fHash->hName[i]);
}

/******************************************************************************/
/*                                A d d U r l                                 */
/******************************************************************************/
  
void XrdOucFileInfo::AddUrl(const char *url,  const char *cntry,
                            int         prty, bool        fifo)
{
   XrdOucFIUrl *urlP = new XrdOucFIUrl(url, cntry, prty);
   XrdOucFIUrl *unP = fUrl, *upP = 0;

// If a country code was specified, convert it to lower case
//
   if (cntry)
      {urlP->fCC[0] = tolower(cntry[0]);
       urlP->fCC[1] = tolower(cntry[1]);
       urlP->fCC[2] = urlP->fCC[3] = 0;
      } else strcpy(urlP->fCC, "us");

// Find location to insert this url
//
   if (fifo)
      {while(unP && prty >= unP->fPrty) {upP = unP; unP = unP->next;}
      } else {
       while(unP && prty >  unP->fPrty) {upP = unP; unP = unP->next;}
      }

// Do the insert
//
   urlP->next = unP;
   if (upP) upP->next = urlP;
      else  fUrl      = urlP;
   if (fUrl != fUrlNext) fUrlNext = fUrl;
}

/******************************************************************************/
/*                                A d d F i l e N a m e                       */
/******************************************************************************/

void XrdOucFileInfo::AddFileName(const char * filename)
{
  if(fTargetName) {free(fTargetName); fTargetName = 0;}

  if(filename)
    fTargetName = strdup(filename);
}

/******************************************************************************/
/*                                A d d L f n                                 */
/******************************************************************************/
  
void XrdOucFileInfo::AddLfn(const char * lfn)
{
  if(fLfn) {free(fLfn); fLfn = 0;}

  if(lfn)
    fLfn = strdup(lfn);
}
  
/******************************************************************************/
/*                           A d d P r o t o c o l                            */
/******************************************************************************/
  
void XrdOucFileInfo::AddProtocol(const char * protname)
{
   if (protList.find(protname) == std::string::npos) protList.append(protname);
}

/******************************************************************************/
/*                             G e t D i g e s t                              */
/******************************************************************************/

const char *XrdOucFileInfo::GetDigest(const char *&hval, bool xrdname)
{
   XrdOucFIHash *hP;

// Check if we are at the end
//
   if (!fHashNext) {fHashNext = fHash; return 0;}

// Skip to next hash for subsequent call
//
   hP = fHashNext; fHashNext = fHashNext->next;

// Return the appropriate values
//
   hval = hP->hValue;
   return (xrdname ? hP-> XrdhName() : hP->hName);
}

/******************************************************************************/
/*                                g e t U r l                                 */
/******************************************************************************/
  
const char *XrdOucFileInfo::GetUrl(char *cntry, int *prty)
{
   XrdOucFIUrl *uP;

// Check if we are at the end
//
   if (!fUrlNext) {fUrlNext = fUrl; return 0;}

// Skip to next url for subsequent call
//
   uP = fUrlNext; fUrlNext = fUrlNext->next;

// Return country code if wanted
//
   if (cntry) strcpy(cntry, uP->fCC);

// Return priority if wanted
//
   if (prty) *prty = uP->fPrty;

// Return the url
//
   return uP->fUrl;
}

/******************************************************************************/
/*                           H a s P r o t o c o l                            */
/******************************************************************************/
  
bool XrdOucFileInfo::HasProtocol(const char * protname)
{
   return (protList.find(protname) != std::string::npos);
}
