#ifndef __CMS_CACHE__H
#define __CMS_CACHE__H
/******************************************************************************/
/*                                                                            */
/*                        X r d C m s C a c h e . h h                         */
/*                                                                            */
/* (c) 2007 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//         $Id$

#include <string.h>
  
#include "Xrd/XrdJob.hh"
#include "Xrd/XrdScheduler.hh"
#include "XrdCms/XrdCmsKey.hh"
#include "XrdCms/XrdCmsNash.hh"
#include "XrdCms/XrdCmsPList.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdCms/XrdCmsSelect.hh"
#include "XrdCms/XrdCmsTypes.hh"
  
class XrdCmsCache
{
public:
friend class XrdCmsCacheJob;

XrdCmsPList_Anchor Paths;

// AddFile() returns true if this is the first addition, false otherwise. See
//           method for detailed information on processing.
//
int         AddFile(XrdCmsSelect &Sel, SMask_t mask);

// DelFile() returns true if this is the last deletion, false otherwise
//
int         DelFile(XrdCmsSelect &Sel, SMask_t mask);

// GetFile() returns true if we actually found the file
//
int         GetFile(XrdCmsSelect &Sel, SMask_t mask);

// UnkFile() updates the unqueried vector and returns 1 upon success, 0 o/w.
//
int         UnkFile(XrdCmsSelect &Sel, SMask_t mask);

// WT4File() adds a request to the callback queue and returns a 0 if added
//           of a wait time to be returned to the client.
//
int         WT4File(XrdCmsSelect &Sel, SMask_t mask);

void        Bounce(SMask_t mask);

void        Drop(SMask_t mask);

int         Init(int fxHold, int fxDelay);

void       *TickTock();

            XrdCmsCache() : okVec(0), Tick(8*60*60), Tock(0), DLTime(5)
                          {memset(Bounced, 0, sizeof(Bounced));}
           ~XrdCmsCache() {}   // Never gets deleted

private:

void          Add2Q(XrdCmsRRQInfo *Info, XrdCmsKeyItem *cp, int isrw);
void          Dispatch(XrdCmsKeyItem *cinfo, short roQ, short rwQ);
void          Recycle(XrdCmsKeyItem *theList);

XrdSysMutex   myMutex;
XrdCmsNash    CTable;
SMask_t       Bounced[XrdCmsKeyItem::TickRate];
SMask_t       okVec;
unsigned int  Tick;
unsigned int  Tock;
         int  DLTime;
};

namespace XrdCms
{
extern    XrdCmsCache Cache;
}
#endif
