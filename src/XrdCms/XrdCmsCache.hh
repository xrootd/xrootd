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
  
#include "Xrd/XrdJob.hh"
#include "Xrd/XrdScheduler.hh"
#include "XrdCms/XrdCmsPList.hh"
#include "XrdCms/XrdCmsTypes.hh"
#include "XrdOuc/XrdOucHash.hh"
#include "XrdSys/XrdSysPthread.hh"
 
/******************************************************************************/
/*                    S t r u c t   X r d C m s C I n f o                     */
/******************************************************************************/
  
struct XrdCmsCInfo
       {SMask_t hfvec;    // Servers who are staging or have the file
        SMask_t pfvec;    // Servers who are staging         the file
        SMask_t sbvec;    // Servers that are suspect (eventually TOE clock)
        int     deadline;
        short   roPend;   // Redirectors waiting for R/O response
        short   rwPend;   // Redirectors waiting for R/W response

        XrdCmsCInfo() {roPend = rwPend = 0;}
       ~XrdCmsCInfo();
       };

/******************************************************************************/
/*                     C l a s s   X r d C m s C a c h e                      */
/******************************************************************************/

class XrdCmsRRQInfo;
  
class XrdCmsCache
{
public:
friend class XrdCmsCache_Scrubber;

XrdCmsPList_Anchor Paths;

// AddFile() returns true if this is the first addition, false otherwise. Opts
//           are those defined for XrdCmsSelect::Opts. However, only Write and
//           Pending are currently recognized.
//
int        AddFile(const char *path, SMask_t mask, int Opts=0,
                   int dltime=0, XrdCmsRRQInfo *Info=0);

// DelCache() deletes a specific cache line
//
void       DelCache(const char *path);

// DelFile() returns true if this is the last deletion, false otherwise
//
int        DelFile(const char *path, SMask_t mask, int dltime=0);

// GetFile() returns true if we actually found the file
//
int        GetFile(const char *path, XrdCmsCInfo &cinfo,
                   int isrw=0, XrdCmsRRQInfo *Info=0);

void       Apply(int (*func)(const char *, XrdCmsCInfo *, void *), void *Arg);

void       Bounce(SMask_t mask, const char *path=0);

void       Extract(const char *pathpfx, XrdOucHash<char> *hashp);

void       Reset(int servid);

void       Scrub();

void       setLifetime(int lsec) {LifeTime = lsec;}

           XrdCmsCache() {LifeTime = 8*60*60;}
          ~XrdCmsCache() {}   // Never gets deleted

private:

void                    Add2Q(XrdCmsRRQInfo *Info, XrdCmsCInfo *cp, int isrw);
void                    Dispatch(XrdCmsCInfo *cinfo, short roQ, short rwQ);
XrdSysMutex             PTMutex;
XrdOucHash<XrdCmsCInfo> PTable;
int                     LifeTime;
};
 
/******************************************************************************/
/*            C l a s s   X r d C m s C a c h e _ S c r u b b e r             */
/******************************************************************************/
  
class XrdCmsCache_Scrubber : public XrdJob
{
public:

void  DoIt() {CacheP->Scrub();
              SchedP->Schedule((XrdJob *)this, CacheP->LifeTime+time(0));
             }
      XrdCmsCache_Scrubber(XrdCmsCache *cp, XrdScheduler *sp)
                        : XrdJob("File cache scrubber")
                {CacheP = cp; SchedP = sp;}
     ~XrdCmsCache_Scrubber() {}

private:

XrdCmsCache     *CacheP;
XrdScheduler    *SchedP;
};

namespace XrdCms
{
extern    XrdCmsCache Cache;
}
#endif
