#ifndef __OLB_CACHE__H
#define __OLB_CACHE__H
/******************************************************************************/
/*                                                                            */
/*                        X r d O l b C a c h e . h h                         */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//         $Id$
  
#include "XrdOlb/XrdOlbPList.hh"
#include "XrdOlb/XrdOlbScheduler.hh"
#include "XrdOlb/XrdOlbTypes.hh"
#include "XrdOuc/XrdOucHash.hh"
#include "XrdOuc/XrdOucPthread.hh"
 
/******************************************************************************/
/*                     S t r u c t   o o l b _ C I n f o                      */
/******************************************************************************/
  
struct XrdOlbCInfo
       {SMask_t rovec;
        SMask_t rwvec;
        SMask_t sbvec;
        int deadline;
       };

/******************************************************************************/
/*                      C l a s s   o o l b _ C a c h e                       */
/******************************************************************************/
  
class XrdOlbCache
{
public:
friend class XrdOlbCache_Scrubber;

XrdOlbPList_Anchor Paths;

// AddFile() returns true if this is the first addition, false otherwise
//
int        AddFile(const char *path, SMask_t mask, int isrw=-1, int dltime=0);

// DelFile() returns true if this is the last deletion, false otherwise
//
int        DelFile(const char *path, SMask_t mask, int dltime=0);

// GetFile() returns true if we actually found the file
//
int        GetFile(const char *path, XrdOlbCInfo &cinfo);

void       Apply(int (*func)(const char *, XrdOlbCInfo *, void *), void *Arg);

void       Bounce(SMask_t mask, char *path=0);

void       Extract(const char *pathpfx, XrdOucHash<char> *hashp);

void       Reset(int servid);

void       Scrub();

void       setLifetime(int lsec) {LifeTime = lsec;}

           XrdOlbCache() {LifeTime = 8*60*60;}
          ~XrdOlbCache() {}   // Never gets deleted

private:

XrdOucMutex            PTMutex;
XrdOucHash<XrdOlbCInfo> PTable;
int                   LifeTime;
};
 
/******************************************************************************/
/*             C l a s s   o o l b _ C a c h e _ S c r u b b e r              */
/******************************************************************************/
  
class XrdOlbCache_Scrubber : public XrdOlbJob
{
public:

int   DoIt() {CacheP->Scrub(); 
              SchedP->Schedule((XrdOlbJob *)this, CacheP->LifeTime+time(0));
              return 1;
             }
      XrdOlbCache_Scrubber(XrdOlbCache *cp, XrdOlbScheduler *sp)
                        : XrdOlbJob("File cache scrubber")
                {CacheP = cp; SchedP = sp;}
     ~XrdOlbCache_Scrubber() {}

private:

XrdOlbScheduler *SchedP;
XrdOlbCache     *CacheP;
};
#endif
