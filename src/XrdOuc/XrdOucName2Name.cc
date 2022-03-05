/******************************************************************************/
/*                                                                            */
/*                     X r d O u c N a m 2 N a m e . c c                      */
/*                                                                            */
/* (c) 2006 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
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
  
// This file implements an instance of the XrdOucName2Name and the
// XrdOucName2NameVec abstract classes.

#include <cerrno>

#include "XrdSys/XrdSysError.hh"
#include "XrdOuc/XrdOucName2Name.hh"
#include "XrdSys/XrdSysPlatform.hh"
  
/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

XrdOucName2NameVec *XrdOucN2NVec_P = 0;
  
/******************************************************************************/
/*           XrdOucName2Name & XrdOucName2NameVec Class Definition            */
/******************************************************************************/
  
class XrdOucN2N : public XrdOucName2Name, public XrdOucName2NameVec
{
public:

virtual int lfn2pfn(const char *lfn, char *buff, int blen);

virtual int lfn2rfn(const char *lfn, char *buff, int blen);

virtual int pfn2lfn(const char *lfn, char *buff, int blen);

virtual std::vector<std::string *> *n2nVec(const char *lfn);

            XrdOucN2N(XrdSysError *erp, const char *lpfx, const char *rpfx);

private:
int concat_fn(const char *prefix, int  pfxlen,
              const char *path,  char *buffer, int blen);

XrdSysError *eDest;
char        *LocalRoot;
int          LocalRootLen;
char        *RemotRoot;
int          RemotRootLen;
};
 
/******************************************************************************/
/*                        I m p l e m e n t a t i o n                         */
/******************************************************************************/
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdOucN2N::XrdOucN2N(XrdSysError *erp, const char *lpfx, const char *rpfx)
{
   eDest = erp;

// Local root must not have any trailing slahes
//
   if (!lpfx) {LocalRoot = 0; LocalRootLen = 0;}
      else if (!(LocalRootLen = strlen(lpfx))) LocalRoot = 0;
              else {LocalRoot = strdup(lpfx);
                    while(LocalRootLen && LocalRoot[LocalRootLen-1] == '/')
                         {LocalRootLen--; LocalRoot[LocalRootLen] = '\0';}
                   }

// Remote root must not have any trailing slases unless it a URL
//
   if (!rpfx) {RemotRoot = 0; RemotRootLen = 0;}
      else if (!(RemotRootLen = strlen(rpfx))) RemotRoot = 0;
              else {RemotRoot = strdup(rpfx);
                    if (*RemotRoot == '/')
                    while(RemotRootLen && RemotRoot[RemotRootLen-1] == '/')
                          {RemotRootLen--; RemotRoot[RemotRootLen] = '\0';}
                   }
}

/******************************************************************************/
/*                               l f n 2 p f n                                */
/******************************************************************************/
  
int XrdOucN2N::lfn2pfn(const char *lfn, char  *buff, int blen)
{
    if (concat_fn(LocalRoot, LocalRootLen, lfn, buff, blen))
       return eDest->Emsg("glp",-ENAMETOOLONG,"generate local path",lfn);
    return 0;
}

/******************************************************************************/
/*                               l f n 2 r f n                                */
/******************************************************************************/
  
int XrdOucN2N::lfn2rfn(const char *lfn, char  *buff, int blen)
{
   if (concat_fn(RemotRoot, RemotRootLen, lfn, buff, blen))
      return eDest->Emsg("grp",-ENAMETOOLONG,"generate remote path",lfn);
   return 0;
}

/******************************************************************************/
/*                             c o n c a t _ f n                              */
/******************************************************************************/
  
int XrdOucN2N::concat_fn(const char *prefix, // String to prefix path
                         const int   pfxlen, // Length of prefix string
                         const char *path,   // String to suffix prefix
                               char *buffer, // Resulting buffer
                               int   blen)   // The buffer length
{
   int addslash = (*path != '/');
   int pathlen  = strlen(path);

   if ((pfxlen + addslash + pathlen) >= blen) return -1;

   if (pfxlen) {strcpy(buffer, prefix); buffer += pfxlen;}
   if (addslash) {*buffer = '/'; buffer++;}
   strcpy(buffer, path);
   return 0;
}

/******************************************************************************/
/*                               p f n 2 l f n                                */
/******************************************************************************/
  
int XrdOucN2N::pfn2lfn(const char *pfn, char  *buff, int blen)
{
    char *tp;

    if (!LocalRoot
    ||  strncmp(pfn, LocalRoot, LocalRootLen) 
    ||  pfn[LocalRootLen] != '/')
            tp = (char *)pfn;
       else tp = (char *)(pfn+LocalRootLen);

    if (strlcpy(buff, tp, blen) >= (unsigned int)blen) return ENAMETOOLONG;
    return 0;
}

/******************************************************************************/
/*                                n 2 n V e c                                 */
/******************************************************************************/
  
std::vector<std::string *> *XrdOucN2N::n2nVec(const char *lfn)
{
   char         pfnBuff[2048];
   std::string *s;

// Perform translation
//
   if (lfn2pfn(lfn, pfnBuff, sizeof(pfnBuff))) return 0;

// Return a vector of one
//
   s = new std::string(pfnBuff);
   return new std::vector<std::string *>(1, s);
}
  
/******************************************************************************/
/*                    X r d O u c g e t N a m e 2 N a m e                     */
/******************************************************************************/
  
XrdOucName2Name *XrdOucgetName2Name(XrdOucgetName2NameArgs)
{
   (void)confg; (void)parms;

   XrdOucN2N *n2nP = new XrdOucN2N(eDest, lroot, rroot);
   XrdOucN2NVec_P  = (XrdOucName2NameVec *)n2nP;
   return (XrdOucName2Name *)n2nP;
}
