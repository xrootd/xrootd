/******************************************************************************/
/*                                                                            */
/*                        X r d C m s C a c h e . c c                         */
/*                                                                            */
/* (c) 2007 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//         $Id$

// Original Version: 1.13 2007/07/12 21:57:38 abh

const char *XrdCmsCacheCVSID = "$Id$";
  
#include <sys/types.h>

#include "XrdCms/XrdCmsCache.hh"
#include "XrdCms/XrdCmsRRQ.hh"
#include "XrdCms/XrdCmsSelect.hh"

using namespace XrdCms;

/******************************************************************************/
/*                      L o c a l   S t r u c t u r e s                       */
/******************************************************************************/
  
struct XrdCmsBNCArgs
       {SMask_t          smask;
        const char      *ppfx;
        int              plen;
       };
  
struct XrdCmsEXTArgs
       {XrdOucHash<char> *hp;
        char             *ppfx;
        int               plen;
       };

/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/
  
XrdCmsCache XrdCms::Cache;

/******************************************************************************/
/*                    E x t e r n a l   F u n c t i o n s                     */
/******************************************************************************/
/******************************************************************************/
/*                       X r d C m s B o u n c e A l l                        */
/******************************************************************************/
  
int XrdCmsBounceAll(const char *key, XrdCmsCInfo *cinfo, void *maskp)
{
    SMask_t xmask, smask = *(SMask_t *)maskp;

// Clear the vector for this server and indicate it bounced
//
   xmask = ~smask;
   cinfo->hfvec &= xmask;
   cinfo->pfvec &= xmask;
   cinfo->sbvec |= smask;

// Return a zero to keep this hash table
//
   return 0;
}

/******************************************************************************/
/*                      X r d C m s B o u n c e S o m e                       */
/******************************************************************************/

int XrdCmsBounceSome(const char *key, XrdCmsCInfo *cip, void *xargp)
{
    struct XrdCmsBNCArgs *xargs = (struct XrdCmsBNCArgs *)xargp;

// Check for match
//
   if (!strncmp(key, xargs->ppfx, xargs->plen))
      return XrdCmsBounceAll(key, cip, (void *)&xargs->smask);

// Keep the entry otherwise
//
   return 0;
}

/******************************************************************************/
/*                        X r d C m s C l e a r V e c                         */
/******************************************************************************/
  
int XrdCmsClearVec(const char *key, XrdCmsCInfo *cinfo, void *sid)
{
    const SMask_t smask_1 = 1;  // Avoid compiler promotion errors
    SMask_t smask = smask_1<<*(int *)sid;

// Clear the vector for this server
//
   smask = ~smask;
   cinfo->hfvec &= smask;
   cinfo->pfvec &= smask;
   cinfo->sbvec &= smask;

// Return indicating whether we should delete this or not
//
   return (cinfo->hfvec ? 0 : -1);
}

/******************************************************************************/
/*                       X r d C m s E x t r a c t F N                        */
/******************************************************************************/
  
int XrdCmsExtractFN(const char *key, XrdCmsCInfo *cip, void *xargp)
{
    struct XrdCmsEXTArgs *xargs = (struct XrdCmsEXTArgs *)xargp;

// Check for match
//
   if (xargs->plen <= (int)strlen(key)
   &&  !strncmp(key, xargs->ppfx, xargs->plen)) xargs->hp->Add(key, 0);

// All done
//
   return 0;
}

/******************************************************************************/
/*                       X r d C m s S c r u b S c a n                        */
/******************************************************************************/
  
int XrdCmsScrubScan(const char *key, XrdCmsCInfo *cip, void *xargp)
{
   return 0;
}

/******************************************************************************/
/*                               A d d F i l e                                */
/******************************************************************************/
  
int XrdCmsCache::AddFile(const char    *path,
                         SMask_t        mask,
                         int            Opts,   // From XrdCmsSelect::Opts
                         int            dltime,
                         XrdCmsRRQInfo *Info)
{
   XrdCmsCInfo *cinfo;
   int isrw = (Opts & XrdCmsSelect::Write), isnew = 0;

// Lock the hash table
//
   PTMutex.Lock();

// Add/Modify the entry
//
   if ((cinfo = PTable.Find(path)))
      {if (dltime > 0) 
          {cinfo->deadline = dltime + time(0);
           cinfo->hfvec = 0; cinfo->pfvec = 0; cinfo->sbvec = 0;
           if (Info) Add2Q(Info, cinfo, isrw);
          } else {
           isnew = (cinfo->hfvec == 0);
           cinfo->hfvec |=  mask; cinfo->sbvec &= ~mask;
           if (isrw) {cinfo->deadline = 0;
                      if (cinfo->roPend || cinfo->rwPend)
                         Dispatch(cinfo, cinfo->roPend, cinfo->rwPend);
                     }
              else   {if (!cinfo->rwPend) cinfo->deadline = 0;
                      if (cinfo->roPend) Dispatch(cinfo, cinfo->roPend, 0);
                     }
          }
      } else if (dltime)
                {cinfo = new XrdCmsCInfo();
                 cinfo->hfvec = mask; cinfo->pfvec=cinfo->sbvec = 0; isnew = 1;
                 if (dltime > 0) cinfo->deadline = dltime + time(0);
                 PTable.Add(path, cinfo, LifeTime);
                 if (Info) Add2Q(Info, cinfo, isrw);
                }

// All done
//
   PTMutex.UnLock();
   return isnew;
}
  
/******************************************************************************/
/*                              D e l C a c h e                               */
/******************************************************************************/

void XrdCmsCache::DelCache(const char *path)
{

// Lock the hash table
//
   PTMutex.Lock();

// Delete the cache line
//
   PTable.Del(path);

// All done
//
   PTMutex.UnLock();
}
  
/******************************************************************************/
/*                               D e l F i l e                                */
/******************************************************************************/
  
int XrdCmsCache::DelFile(const char    *path,
                         SMask_t        mask,
                         int            dltime)
{
   XrdCmsCInfo *cinfo;
   int gone4good;

// Lock the hash table
//
   PTMutex.Lock();

// Look up the entry and remove server
//
   if ((cinfo = PTable.Find(path)))
      {cinfo->hfvec &= ~mask;
       cinfo->pfvec &= ~mask;
       cinfo->sbvec &= ~mask;
       gone4good = (cinfo->hfvec == 0);
       if (dltime > 0) cinfo->deadline = dltime + time(0);
          else if (gone4good) PTable.Del(path);
      } else gone4good = 0;

// All done
//
   PTMutex.UnLock();
   return gone4good;
}
  
/******************************************************************************/
/*                               G e t F i l e                                */
/******************************************************************************/
  
int  XrdCmsCache::GetFile(const char    *path,
                          XrdCmsCInfo   &cinfo,
                          int            isrw,
                          XrdCmsRRQInfo *Info)
{
   XrdCmsCInfo *info;

// Lock the hash table
//
   PTMutex.Lock();

// Look up the entry and remove server
//
   if ((info = PTable.Find(path)))
      {cinfo.hfvec = info->hfvec;
       cinfo.pfvec = info->pfvec;
       cinfo.sbvec = info->sbvec;
       if (info->deadline && info->deadline <= time(0))
          info->deadline = 0;
          else if (Info && info->deadline && !info->sbvec) Add2Q(Info,info,isrw);
       cinfo.deadline = info->deadline;
      }

// All done
//
   PTMutex.UnLock();
   return (info != 0);
}

/******************************************************************************/
/*                                 A p p l y                                  */
/******************************************************************************/
  
void XrdCmsCache::Apply(int (*func)(const char *,XrdCmsCInfo *,void *), void *Arg)
{
     PTMutex.Lock();
     PTable.Apply(func, Arg);
     PTMutex.UnLock();
}
 
/******************************************************************************/
/*                                B o u n c e                                 */
/******************************************************************************/

void XrdCmsCache::Bounce(SMask_t smask, const char *path)
{

// Remove server from cache entries and indicate that it bounced
//
   if (!path)
      {PTMutex.Lock();
       PTable.Apply(XrdCmsBounceAll, (void *)&smask);
       PTMutex.UnLock();
      } else {
       struct XrdCmsBNCArgs xargs = {smask, path, strlen(path)};
       PTMutex.Lock();
       PTable.Apply(XrdCmsBounceSome, (void *)&xargs);
       PTMutex.UnLock();
      }
}
  
/******************************************************************************/
/*                               E x t r a c t                                */
/******************************************************************************/

void XrdCmsCache::Extract(const char *pathpfx, XrdOucHash<char> *hashp)
{
   struct XrdCmsEXTArgs xargs = {hashp, (char *)pathpfx, strlen(pathpfx)};

// Search the cache for all matching elements and insert them into the new hash
//
   PTMutex.Lock();
   PTable.Apply(XrdCmsExtractFN, (void *)&xargs);
   PTMutex.UnLock();
}
  
/******************************************************************************/
/*                                 R e s e t                                  */
/******************************************************************************/
  
void XrdCmsCache::Reset(int nodeid)
{
     PTMutex.Lock();
     PTable.Apply(XrdCmsClearVec, (void *)&nodeid);
     PTMutex.UnLock();
}

/******************************************************************************/
/*                                 S c r u b                                  */
/******************************************************************************/
  
void XrdCmsCache::Scrub()
{
     PTMutex.Lock();
     PTable.Apply(XrdCmsScrubScan, (void *)0);
     PTMutex.UnLock();
}

/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                                 A d d 2 Q                                  */
/******************************************************************************/
  
void XrdCmsCache::Add2Q(XrdCmsRRQInfo *Info, XrdCmsCInfo *cp, int isrw)
{
   short Slot = (isrw ? cp->rwPend : cp->roPend);

// Add the request to the appropriate pending queue
//
   Info->Key = cp;
   Info->isRW= isrw;
   if (!(Slot = RRQ.Add(Slot, Info))) Info->Key = 0;
      else if (isrw) cp->rwPend = Slot;
               else  cp->roPend = Slot;
}

/******************************************************************************/
/*                              D i s p a t c h                               */
/******************************************************************************/
  
void XrdCmsCache::Dispatch(XrdCmsCInfo *cinfo, short roQ, short rwQ)
{

// Dispach the waiting elements
//
   if (roQ) {RRQ.Ready(roQ, cinfo, cinfo->hfvec, cinfo->pfvec);
             cinfo->roPend = 0;
            }
   if (rwQ) {RRQ.Ready(rwQ, cinfo, cinfo->hfvec, cinfo->pfvec);
             cinfo->rwPend = 0;
            }
}

/******************************************************************************/
/*                X r d C m s C I n f o   D e s t r u c t o r                 */
/******************************************************************************/
  
XrdCmsCInfo::~XrdCmsCInfo()
{
   if (roPend) RRQ.Del(roPend, this);
   if (rwPend) RRQ.Del(rwPend, this);
}
