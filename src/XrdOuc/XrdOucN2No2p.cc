/******************************************************************************/
/*                                                                            */
/*                       X r d O u c N 2 N o 2 p . c c                        */
/*                                                                            */
/* (c) 2017 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
/******************************************************************************/
/*                         i n c l u d e   f i l e s                          */
/******************************************************************************/

#include <algorithm>
#include <errno.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <unistd.h>

#include "XrdVersion.hh"

#include "XrdOuc/XrdOucName2Name.hh"
#include "XrdOuc/XrdOucTokenizer.hh"
#include "XrdSys/XrdSysError.hh"

/******************************************************************************/
/*                               S t a t i c s                                */
/******************************************************************************/
  
XrdVERSIONINFO(XrdOucgetName2Name, "XrdN2No2p");

namespace
{
char h2c[16] = {'0','1','2','3','4','5','6','7',
                '8','9','A','B','C','D','E','F'};
}

/******************************************************************************/
/*                    E x t e r n a l   F u n c t i o n s                     */
/******************************************************************************/
  
extern unsigned long XrdOucHashVal2(const char *KeyVal, int KeyLen);

/******************************************************************************/
/*                      C l a s s   D e f i n i t i o n                       */
/******************************************************************************/
  
class XrdOucN2No2p : public XrdOucName2Name
{
public:

virtual int lfn2pfn(const char* lfn, char* buff, int blen);

virtual int lfn2rfn(const char* lfn, char* buff, int blen) {return -ENOTSUP;}

virtual int pfn2lfn(const char* pfn, char* buff, int blen);

            XrdOucN2No2p(XrdSysError *erp, const char *lroot,
                                           const char* pfx, int fnmax, char sc)
                     : eDest(erp), sChar(sc),
                       oidPfx(strdup(pfx)), oidPsz(strlen(pfx)), oidMax(fnmax)
                     {if (!lroot) {lRoot = 0; lRLen = 0;}
                         else     {lRoot = strdup(lroot);
                                   lRLen = strlen(lroot);
                                   if (lRoot[lRLen-1] == '/')
                                      {lRoot[lRLen-1] = 0; lRLen--;}
                                  }
                     }

virtual    ~XrdOucN2No2p() {if (oidPfx) free(oidPfx);
                            if (lRoot)  free(lRoot);
                           }

private:
XrdSysError *eDest;
char        *lRoot;
int          lRLen;
char         sChar;
char        *oidPfx;
int          oidPsz;
int          oidMax;
};

/******************************************************************************/
/*                               l f n 2 p f n                                */
/******************************************************************************/
  
int XrdOucN2No2p::lfn2pfn(const char* lfn, char* buff, int blen)
{
// If have a local root then prefix result with it (make sure it fits)
//
   if (lRoot)
      {if (lRLen >= blen-1) return ENAMETOOLONG;
       strcpy(buff, lRoot);
       buff += lRLen; blen -= lRLen;
      }

// Now just to the transformation so that we can ref the oid as a file
//
   return pfn2lfn(lfn, buff, blen);
}
  
/******************************************************************************/
/*                               p f n 2 l f n                                */
/******************************************************************************/
  
int XrdOucN2No2p::pfn2lfn(const char* pfn, char* buff, int blen) 
{
   const char *sP;
   char *bP;
   std::string pstr;
   int pfnLen = strlen(pfn);

// If the pfn starts with a slash then do nothing
//
   if (*pfn == '/')
      {if (pfnLen >= blen) return ENAMETOOLONG;
       strcpy(buff, pfn);
       return 0;
      }

// If there are any slashes in the object id we need to remove them
//
   if ((sP = index(pfn, '/')))
      {pstr = pfn;
       std::replace(pstr.begin(), pstr.end(), '/', sChar);
       pfn = pstr.c_str();
      }

// Create the object distribution subpath. The format is based on the
// actual length of the object id and what we can use in this file system.
// We make special allowances for short object ID's that can screw this up.
//
   if (pfnLen <= oidMax)
      {unsigned long hVal = XrdOucHashVal2(pfn, pfnLen);
       unsigned long sVal = ((int)sizeof(unsigned long) > 4 ? 32 : 16);
       char subP[8];
       if (pfnLen <= (int)sizeof(unsigned long)) hVal = hVal ^ (hVal >> sVal);
       subP[1] = h2c[(hVal & 0x0f)]; hVal >>= 4; subP[0] = h2c[(hVal & 0x0f)];
       subP[2] = '/';  hVal >>= 4;
       subP[4] = h2c[(hVal & 0x0f)]; hVal >>= 4; subP[3] = h2c[(hVal & 0x0f)];
       subP[5] = '/'; subP[6] = 0;
       int n = snprintf(buff, blen, "%s%s%s", oidPfx, subP, pfn);
       return (n < blen ? 0 : ENAMETOOLONG);
      }

// The object id is longer than what is allowed for a file name. So, we
// convert the name to a number of directories using object id fragments.
// Check if we even have a chance here (note we may be one byte too many).
//
   if ((oidPsz + pfnLen + (pfnLen/oidMax)) >= blen) return ENAMETOOLONG;

// Prepare to segement the oid
//
   strcpy(buff, oidPfx); bP = buff + oidPsz; blen -= oidPsz;

// Copy over segments separated by a slash
//
   while(blen > oidMax && pfnLen > oidMax)
        {strncpy(bP, pfn, oidMax);
         bP  += oidMax; blen   -= oidMax;
         pfn += oidMax; pfnLen -= oidMax;
         if (blen > 0) {*bP++ = '/'; blen--;}
        }

// Copy the final segment if we have room
//
   if (blen <= pfnLen) return ENAMETOOLONG;
   strcpy(bP, pfn);
   return 0;
}
 
/******************************************************************************/
/*                    X r d O u c g e t N a m e 2 N a m e                     */
/******************************************************************************/
  
XrdOucName2Name *XrdOucgetName2Name(XrdOucgetName2NameArgs)
{
   struct bHelper {char *p; bHelper(const char *bP) : p(bP ? strdup(bP) : 0) {}
                           ~bHelper() {if (p) free(p);}
                  };
   bHelper prms(parms);
   const char *oPfx;
   char *val, *eP;
   std::string ostr;
   int fnMax = 0, n;
   char sChar = '\\';

// Process options
//
   XrdOucTokenizer toks(prms.p);
   toks.GetLine();
   while((val = toks.GetToken()) && *val)
        {     if (!strcmp(val, "-slash"))
                 {if (!(val = toks.GetToken()) || !(*val))
                     {eDest->Emsg("N2No2p", "-slash argument not specified.");
                      return 0;
                     }
                  if (strlen(val) == 1) {sChar = *val; continue;}
                  n = strtol(val, &eP, 16);
                  if (n & 0xff || *eP)
                     {eDest->Emsg("N2No2p", "Invalid -slash argument -",val);
                      return 0;
                     }
                  sChar = static_cast<char>(n);
                 }
         else if (!strcmp(val, "-maxfnlen"))
                 {if (!(val = toks.GetToken()) || !(*val))
                     {eDest->Emsg("N2No2p", "-maxfnlen argument not specified.");
                      return 0;
                     }
                  fnMax = strtol(val, &eP, 16);
                  if (fnMax <= 0 || *eP)
                     {eDest->Emsg("N2No2p", "Invalid -maxfnlen argument -",val);
                      return 0;
                     }
                 }
         else break;
        }

// Obtain the objectid prefix we are to use (default is '/')
//
   if (!val || !(*val)) oPfx = "/";
      else {if (*val != '/')
               {eDest->Emsg("N2No2p", "Invalid object ID path prefix -", val);
                return 0;
               }
            oPfx = val;
            n = strlen(val);
            if (val[n-1] != '/') {ostr = val; ostr += '/'; oPfx = ostr.c_str();}
           }

// Now determine what the maximum filename length if not specified
//
   if (!fnMax)
   if ((fnMax = pathconf("/", _PC_NAME_MAX)) < 0)
      {eDest->Emsg("N2No2p", errno, "determine -fnmaxlen for '/'; using 255.");
       fnMax = 255;
      }

// Return a new n2n object
//
   return new XrdOucN2No2p(eDest, lroot, oPfx, fnMax, sChar);
}
