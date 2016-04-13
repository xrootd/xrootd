#ifndef __CMS_MANAGER__H
#define __CMS_MANAGER__H
/******************************************************************************/
/*                                                                            */
/*                      X r d C m s M a n a g e r . h h                       */
/*                                                                            */
/* (c) 2007 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <stdlib.h>
#include <string.h>
#include <strings.h>
  
#include "XProtocol/YProtocol.hh"

#include "XrdCms/XrdCmsManList.hh"
#include "XrdCms/XrdCmsTypes.hh"
#include "XrdSys/XrdSysPthread.hh"

class XrdLink;
class XrdCmsManList;
class XrdCmsManTree;
class XrdCmsNode;
class XrdOucTList;
  
/******************************************************************************/
/*                   C l a s s   X r d C m s M a n a g e r                    */
/******************************************************************************/
  
// This a single-instance global class
//
class XrdCmsManager
{
public:
friend class XrdCmsDelNode;

XrdCmsManList *myMans;
XrdCmsManTree *ManTree;

static const int MTMax = 16;   // Maximum number of Managers

XrdCmsNode *Add(XrdLink *lp, int Lvl, bool &xit);

void        Delete(XrdCmsNode *nodeP);

void        Finished(const char *manP, int mPort);

static void Inform(const char *What, const char *Data, int Dlen);
static void Inform(const char *What, struct iovec *vP, int vN, int vT=0);
static void Inform(XrdCms::CmsReqCode rCode, int rMod, const char *Arg=0, int Alen=0);
static void Inform(XrdCms::CmsRRHdr &Hdr, const char *Arg=0, int Alen=0);

static bool Present() {return MTHi >= 0;};

void        Remove(XrdCmsNode *nP, const char *reason=0);

void        Rerun(char *newMans);

static void Reset();

static bool Start(const XrdOucTList *mL);

       bool Verify(XrdLink *lP, const char *sid, const char *sname);

            XrdCmsManager(XrdOucTList *mlP, int snum);
           ~XrdCmsManager() {} // This object should never be deleted

private:
int         Run(XrdOucTList *manP);

// The following are common across all manager instances
//
static XrdSysMutex   MTMutex;
static XrdCmsNode   *MastTab[MTMax];
static char          MastSID[MTMax];
static int           MTHi;

// The following are specific to a manager instance
//
XrdOucTList         *newManList;
XrdOucTList         *curManList;
char                *theSite;
char                *theHost;
char                *theSID;
int                  curManCnt;
short                siteID;
bool                 wasRedir;
};
#endif
