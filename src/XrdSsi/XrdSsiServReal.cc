/******************************************************************************/
/*                                                                            */
/*                     X r d S s i S e r v R e a l . c c                      */
/*                                                                            */
/* (c) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <errno.h>
#include <stdio.h>
#include <string.h>
  
#include "XrdSsi/XrdSsiServReal.hh"
#include "XrdSsi/XrdSsiSessReal.hh"

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/
  
XrdSsiServReal::~XrdSsiServReal()
{
   XrdSsiSessReal *sP;

// Free pointer to the manager node
//
   if (manNode) {free(manNode); manNode = 0;}

// Delete all free session objects
//
   while((sP = freeSes))
        {freeSes = sP->nextSess;
         delete sP;
        }
}

/******************************************************************************/
/* Private:                        A l l o c                                  */
/******************************************************************************/

XrdSsiSessReal *XrdSsiServReal::Alloc(const char *sName)
{
   XrdSsiSessReal *sP;

// Check if we can grab this from out queue
//
   myMutex.Lock();
   actvSes++;
   if ((sP = freeSes))
      {freeCnt--;
       freeSes = sP->nextSess;
       myMutex.UnLock();
       sP->InitSession(this, sName);
      } else {
       myMutex.UnLock();
       if (!(sP = new XrdSsiSessReal(this, sName)))
          {myMutex.Lock(); actvSes--; myMutex.UnLock();}
      }

// Return the pointer
//
   return sP;
}

/******************************************************************************/
/* Private:                       G e n U R L                                 */
/******************************************************************************/
  
bool XrdSsiServReal::GenURL(const char *sName, const char *avoid,
                                  char *buff,        int   blen)
{
   int n;

// Generate appropriate url
//
   if (avoid) n = snprintf(buff, blen, "xroot://%s/%s?tried=%s",
                                       manNode, sName, avoid);
      else    n = snprintf(buff, blen, "xroot://%s/%s", manNode, sName);

// Return overflow or not
//
   return n < blen;
}

/******************************************************************************/
/*                             P r o v i s i o n                              */
/******************************************************************************/
  
bool XrdSsiServReal::Provision(XrdSsiService::Resource *resP,
                               unsigned short           timeOut
                              )
{
   XrdSsiSessReal *sObj;
   char            epURL[2048];

// Validate the resource name
//
   if (!resP->rName || *(resP->rName) != '/')
      {resP->eInfo.Set("Resource name missing or invalid.", EINVAL);
       return false;
      }

// Construct url
//
   if (!GenURL(resP->rName, resP->hAvoid, epURL, sizeof(epURL)))
      {resP->eInfo.Set("Resource url is too long.", ENAMETOOLONG);
       return false;
      }

// Obtain a new session object
//
   if (!(sObj = Alloc(resP->rName)))
      {resP->eInfo.Set("Insufficient memory.", ENOMEM);
       return false;
      }

// Now just effect an open to this resource
//
   if (sObj->Open(resP, epURL, timeOut)) return true;
   Recycle(sObj);
   return false;
}

/******************************************************************************/
/*                               R e c y c l e                                */
/******************************************************************************/
  
void XrdSsiServReal::Recycle(XrdSsiSessReal *sObj)
{

// Add to queue unless we have too many of these
//
   myMutex.Lock();
   actvSes--;
   if (freeCnt >= freeMax) {myMutex.UnLock(); delete sObj;}
      else {sObj->nextSess = freeSes;
            freeSes = sObj;
            freeCnt++;
            myMutex.UnLock();
           }
}

/******************************************************************************/
/* Private:                       R e t E r r                                 */
/******************************************************************************/
  
XrdSsiSession *XrdSsiServReal::RetErr(XrdSsiErrInfo &eInfo,
                                     const char    *eTxt, int eNum, bool async)
{
// Set the error information
//
   eInfo.Set(eTxt, eNum);

// Now return dependent on the processing mode
//
   return (async ? (XrdSsiSession *)-1 : 0);
}

/******************************************************************************/
/*                                  S t o p                                   */
/******************************************************************************/
  
bool XrdSsiServReal::Stop()
{
// Make sure we are clean
//
   myMutex.Lock();
   if (actvSes) {myMutex.UnLock(); return false;}
   myMutex.UnLock();
   delete this;
   return true;
}
