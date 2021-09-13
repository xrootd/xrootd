#ifndef __SSI_FILESESS_H__
#define __SSI_FILESESS_H__
/******************************************************************************/
/*                                                                            */
/*                     X r d S s i F i l e S e s s . h h                      */
/*                                                                            */
/* (c) 2016 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cstring>
#include <sys/types.h>

#include "XrdSfs/XrdSfsInterface.hh"
#include "XrdSfs/XrdSfsXio.hh"
#include "XrdSsi/XrdSsiBVec.hh"
#include "XrdSsi/XrdSsiFileReq.hh"
#include "XrdSsi/XrdSsiFileResource.hh"
#include "XrdSsi/XrdSsiRRTable.hh"
#include "XrdSys/XrdSysPthread.hh"
  
class  XrdOucEnv;
struct XrdSsiRespInfo;

class XrdSsiFileSess
{
public:

static  XrdSsiFileSess  *Alloc(XrdOucErrInfo &einfo, const char *user);

        bool             AttnInfo(      XrdOucErrInfo  &eInfo,
                                  const XrdSsiRespInfo *respP,
                                        unsigned int    reqID);

        XrdOucErrInfo   *errInfo() {return eInfo;}
                        
        int              close(bool viaDel=false);
                        
        int              fctl(const int            cmd,
                                    int            alen,
                              const char          *args,
                              const XrdSecEntity  *client);
                        
        const char      *FName() {return gigID;}

        int              open(const char          *fileName,
                              XrdOucEnv           &theEnv,
                              XrdSfsFileOpenMode   openMode);

        XrdSfsXferSize   read(XrdSfsFileOffset   fileOffset,
                              char              *buffer,
                              XrdSfsXferSize     buffer_size);

        void             Recycle();

XrdSsiFileResource      &Resource() {return fileResource;}

        int              SendData(XrdSfsDio         *sfDio,
                                  XrdSfsFileOffset   offset,
                                  XrdSfsXferSize     size);

static  void             SetAuthDNS() {authDNS = true;}
                        
        void             setXio(XrdSfsXio *xP) {xioP = xP;}
                        
        int              truncate(XrdSfsFileOffset fileOffset);
                        
        XrdSfsXferSize   write(XrdSfsFileOffset   fileOffset,
                               const char        *buffer,
                               XrdSfsXferSize     buffer_size);

private:                
                        
// Constructor (via Alloc()) and destructor (via Recycle())
//                      
                         XrdSsiFileSess(XrdOucErrInfo &einfo, const char *user)
                                       {Init(einfo, user, false);}
                        ~XrdSsiFileSess() {} // Recycle() calls Reset()

void                     Init(XrdOucErrInfo &einfo, const char *user, bool forReuse);
bool                     NewRequest(unsigned int reqid, XrdOucBuffer *oP,
                                    XrdSfsXioHandle bR, int rSz);
void                     Reset();
XrdSfsXferSize           writeAdd(const char *buff, XrdSfsXferSize blen,
                                  unsigned int rid);

static XrdSysMutex       arMutex;  // Alloc and Recycle protector
static XrdSsiFileSess   *freeList;
static int               freeNum;
static int               freeNew;
static int               freeMax;
static int               freeAbs;
static bool              authDNS;

XrdSsiFileResource       fileResource;
char                    *tident;
XrdOucErrInfo           *eInfo;
char                    *gigID;
char                    *fsUser;
XrdSysMutex              myMutex;
XrdSfsXio               *xioP;
XrdOucBuffer            *oucBuff;
XrdSsiFileSess          *nextFree;
int                      reqSize;
int                      reqLeft;
bool                     isOpen;
bool                     inProg;

XrdSsiBVec               eofVec;
XrdSsiRRTable<XrdSsiFileReq> rTab;
};
#endif
