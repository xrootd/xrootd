/******************************************************************************/
/*                                                                            */
/*                        X r d S s i U t i l s . c c                         */
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

#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include "XProtocol/XProtocol.hh"

#include "Xrd/XrdScheduler.hh"

#include "XrdCl/XrdClXRootDResponses.hh"

#include "XrdOuc/XrdOucERoute.hh"
#include "XrdOuc/XrdOucErrInfo.hh"

#include "XrdSfs/XrdSfsInterface.hh"

#include "XrdSsi/XrdSsiAtomics.hh"
#include "XrdSsi/XrdSsiErrInfo.hh"
#include "XrdSsi/XrdSsiRequest.hh"
#include "XrdSsi/XrdSsiResponder.hh"
#include "XrdSsi/XrdSsiRRAgent.hh"
#include "XrdSsi/XrdSsiUtils.hh"

#include "XrdSys/XrdSysError.hh"
  
/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

namespace XrdSsi
{
extern XrdSysError   Log;
extern XrdScheduler *schedP;
};

using namespace XrdSsi;

/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/
  
class PostError : public XrdJob, public XrdSsiResponder
{
public:

void     DoIt() {myMutex.Lock();
                 if ( isActive) SetErrResponse(eTxt, eNum);
                 if (!isActive) delete this;
                    else {isActive = false;
                          myMutex.UnLock();
                         }
                }

virtual void   Finished(      XrdSsiRequest  &rqstR,
                        const XrdSsiRespInfo &rInfo,
                              bool            cancel=false)
                       {UnBindRequest();
                        myMutex.Lock();
                        if (!isActive) delete this;
                           else {isActive = false;
                                 myMutex.UnLock();
                                }
                       }

         PostError(XrdSsiRequest *rP, char *emsg, int ec)
                  : myMutex(XrdSsiMutex::Recursive),
                    reqP(rP), eTxt(emsg), eNum(ec), isActive(true)
                    {XrdSsiRRAgent::SetMutex(rP, &myMutex);
                     BindRequest(*reqP);
                    }

virtual ~PostError() {myMutex.UnLock();
                      if (eTxt) free(eTxt);
                     }

private:
XrdSsiMutex        myMutex; // Allow possible rentry via SetErrResponse()
XrdSsiRequest     *reqP;
char              *eTxt;
int                eNum;
bool               isActive;
};

/******************************************************************************/
/*                                   b 2 x                                    */
/******************************************************************************/
  
char *XrdSsiUtils::b2x(const char *ibuff, int ilen, char *obuff, int olen,
                             char xbuff[4])
{
    static char hv[] = "0123456789abcdef";
    char *oP = obuff;

    // Gaurd against too short of an output buffer (minimum if 3 bytes)
    //
    if (olen < 3)
       {*obuff = 0;
        strcpy(xbuff, "...");
        return obuff;
       }

    // Make sure we have something to format
    //
    if (ilen < 1)
       {*obuff = 0;
        *xbuff = 0;
        return obuff;
       }

    // Do length adjustment, as needed
    //
    if (ilen*2 < olen) *xbuff = 0;
       else {ilen = (olen-1)/2;
             strcpy(xbuff, "...");
            }

    // Format the data. We know it will fit with a trailing null byte.
    //
    for (int i = 0; i < ilen; i++) {
        *oP++ = hv[(ibuff[i] >> 4) & 0x0f];
        *oP++ = hv[ ibuff[i]       & 0x0f];
        }
     *oP = '\0';
     return obuff;
}

/******************************************************************************/
/*                                  E m s g                                   */
/******************************************************************************/

int XrdSsiUtils::Emsg(const char    *pfx,    // Message prefix value
                      int            ecode,  // The error code
                      const char    *op,     // Operation being performed
                      const char    *path,   // Operation target
                      XrdOucErrInfo &eDest)  // Plase to put error
{
   char buffer[2048];

// Get correct error code and path
//
    if (ecode < 0) ecode = -ecode;
    if (!path) path = "???";

// Format the error message
//
   XrdOucERoute::Format(buffer, sizeof(buffer), ecode, op, path);

// Put the message in the log
//
   Log.Emsg(pfx, eDest.getErrUser(), buffer);

// Place the error message in the error object and return
//
   eDest.setErrInfo(ecode, buffer);
   return SFS_ERROR;
}


/******************************************************************************/
/*                                G e t E r r                                 */
/******************************************************************************/
  
int XrdSsiUtils::GetErr(XrdCl::XRootDStatus &Status, std::string &eText)
{

// If this is an xrootd error then get the xrootd generated error
//
   if (Status.code == XrdCl::errErrorResponse)
      {eText = Status.GetErrorMessage();
       return MapErr(Status.errNo);
      }

// Internal error, we will need to copy strings here
//
   eText = Status.ToStr();
   return (Status.errNo ? Status.errNo : EFAULT);
}

/******************************************************************************/
/*                                M a p E r r                                 */
/******************************************************************************/

int XrdSsiUtils::MapErr(int xEnum)
{
    switch(xEnum)
       {case kXR_NotFound:      return ENOENT;
        case kXR_NotAuthorized: return EACCES;
        case kXR_IOError:       return EIO;
        case kXR_NoMemory:      return ENOMEM;
        case kXR_NoSpace:       return ENOSPC;
        case kXR_ArgTooLong:    return ENAMETOOLONG;
        case kXR_noserver:      return EHOSTUNREACH;
        case kXR_NotFile:       return ENOTBLK;
        case kXR_isDirectory:   return EISDIR;
        case kXR_FSError:       return ENOSYS;
        default:                return ECANCELED;
       }
}

/******************************************************************************/
/*                                R e t E r r                                 */
/******************************************************************************/
  
void XrdSsiUtils::RetErr(XrdSsiRequest &reqP, const char *eTxt, int eNum)
{

// Schedule an error callback
//
   XrdSsi::schedP->Schedule(new PostError(&reqP, strdup(eTxt), eNum));
}

/******************************************************************************/
/*                                S e t E r r                                 */
/******************************************************************************/
  
void XrdSsiUtils::SetErr(XrdCl::XRootDStatus &Status, XrdSsiErrInfo &eInfo)
{

// If this is an xrootd error then get the xrootd generated error
//
   if (Status.code == XrdCl::errErrorResponse)
      {eInfo.Set(Status.GetErrorMessage().c_str(), MapErr(Status.errNo));
      } else {
       eInfo.Set(Status.ToStr().c_str(), (Status.errNo ? Status.errNo:EFAULT));
      }
}
