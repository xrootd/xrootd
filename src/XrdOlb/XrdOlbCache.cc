/******************************************************************************/
/*                                                                            */
/*                        X r d O l b C a c h e . c c                         */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//         $Id$

const char *XrdOlbCacheCVSID = "$Id$";
  
#include "XrdOlb/XrdOlbCache.hh"

/******************************************************************************/
/*                      L o c a l   S t r u c t u r e s                       */
/******************************************************************************/
  
struct XrdOlbBNCArgs
       {SMask_t          smask;
        char            *ppfx;
        int              plen;
       };
  
struct XrdOlbEXTArgs
       {XrdOucHash<char> *hp;
        char             *ppfx;
        int               plen;
       };

/******************************************************************************/
/*                    E x t e r n a l   F u n c t i o n s                     */
/******************************************************************************/
/******************************************************************************/
/*                       X r d O l b B o u n c e A l l                        */
/******************************************************************************/
  
int XrdOlbBounceAll(const char *key, XrdOlbCInfo *cinfo, void *maskp)
{
    SMask_t xmask, smask = *(SMask_t *)maskp;

// Clear the vector for this server and indicate it bounced
//
   xmask = ~smask;
   cinfo->rovec &= xmask;
   cinfo->rwvec &= xmask;
   cinfo->sbvec |= smask;

// Return a zero to keep this hash table
//
   return 0;
}

/******************************************************************************/
/*                      X r d O l b B o u n c e S o m e                       */
/******************************************************************************/

int XrdOlbBounceSome(const char *key, XrdOlbCInfo *cip, void *xargp)
{
    struct XrdOlbBNCArgs *xargs = (struct XrdOlbBNCArgs *)xargp;

// Check for match
//
   if (!strncmp(key, xargs->ppfx, xargs->plen))
      return XrdOlbBounceAll(key, cip, (void *)&xargs->smask);

// Keep the entry otherwise
//
   return 0;
}

/******************************************************************************/
/*                        X r d O l b C l e a r V e c                         */
/******************************************************************************/
  
int XrdOlbClearVec(const char *key, XrdOlbCInfo *cinfo, void *sid)
{
    const SMask_t smask_1 = 1;  // Avoid compiler promotion errors
    SMask_t smask = smask_1<<*(int *)sid;

// Clear the vector for this server
//
   smask = ~smask;
   cinfo->rovec &= smask;
   cinfo->rwvec &= smask;
   cinfo->sbvec &= smask;

// Return indicating whether we should delete this or not
//
   return (cinfo->rovec || cinfo->rwvec ? 0 : -1);
}

/******************************************************************************/
/*                       X r d O l b E x t r a c t F N                        */
/******************************************************************************/
  
int XrdOlbExtractFN(const char *key, XrdOlbCInfo *cip, void *xargp)
{
    struct XrdOlbEXTArgs *xargs = (struct XrdOlbEXTArgs *)xargp;

// Check for match
//
   if (xargs->plen <= (int)strlen(key)
   &&  !strncmp(key, xargs->ppfx, xargs->plen)) xargs->hp->Add(key, 0);

// All done
//
   return 0;
}

/******************************************************************************/
/*                       X r d O l b S c r u b S c a n                        */
/******************************************************************************/
  
int XrdOlbScrubScan(const char *key, XrdOlbCInfo *cip, void *xargp)
{
   return 0;
}

/******************************************************************************/
/*                               A d d F i l e                                */
/******************************************************************************/
  
int XrdOlbCache::AddFile(char *path, SMask_t mask, int isrw, int dltime)
{
   XrdOlbPInfo  pinfo;
   XrdOlbCInfo *cinfo;
   int isnew = 0;

// Find if this server can handle the file in r/w mode
//
   if (isrw < 0)
      if (!Paths.Find(path, pinfo)) isrw = 0;
         else isrw = (pinfo.rwvec & mask) != 0;

// Lock the hash table
//
   PTMutex.Lock();

// Add/Modify the entry
//
   if ((cinfo = PTable.Find(path)))
      {if (dltime > 0) 
          {cinfo->deadline = dltime + time(0);
           cinfo->rovec = 0; cinfo->rwvec = 0; cinfo->sbvec = 0;
          } else {
           isnew = (cinfo->rovec == 0);
           cinfo->rovec |=  mask; cinfo->sbvec &= ~mask;
           if (isrw) cinfo->rwvec |=  mask;
              else   cinfo->rwvec &= ~mask;
          }
      } else if (dltime)
                {cinfo = new XrdOlbCInfo();
                 cinfo->rovec = mask; cinfo->sbvec = 0; isnew = 1;
                 if (isrw) cinfo->rwvec =  mask;
                    else   cinfo->rwvec = 0;
                 if (dltime > 0) cinfo->deadline = dltime + time(0);
                 PTable.Add(path, cinfo, LifeTime);
                }

// All done
//
   PTMutex.UnLock();
   return isnew;
}
  
/******************************************************************************/
/*                               D e l F i l e                                */
/******************************************************************************/
  
int XrdOlbCache::DelFile(char *path, SMask_t mask, int dltime)
{
   XrdOlbCInfo *cinfo;
   int gone4good;

// Lock the hash table
//
   PTMutex.Lock();

// Look up the entry and remove server
//
   if ((cinfo = PTable.Find(path)))
      {cinfo->rovec &= ~mask;
       cinfo->rwvec &= ~mask;
       cinfo->sbvec &= ~mask;
       gone4good = (cinfo->rovec == 0);
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
  
int  XrdOlbCache::GetFile(char *path, XrdOlbCInfo &cinfo)
{
   XrdOlbCInfo *info;

// Lock the hash table
//
   PTMutex.Lock();

// Look up the entry and remove server
//
   if ((info = PTable.Find(path)))
      {cinfo.rovec = info->rovec;
       cinfo.rwvec = info->rwvec;
       cinfo.sbvec = info->sbvec;
       if (info->deadline && info->deadline <= time(0))
          info->deadline = 0;
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
  
void XrdOlbCache::Apply(int (*func)(const char *,XrdOlbCInfo *,void *), void *Arg)
{
     PTMutex.Lock();
     PTable.Apply(func, Arg);
     PTMutex.UnLock();
}
 
/******************************************************************************/
/*                                B o u n c e                                 */
/******************************************************************************/

void XrdOlbCache::Bounce(SMask_t smask, char *path)
{

// Remove server from cache entries and indicate that it bounced
//
   if (!path)
      {PTMutex.Lock();
       PTable.Apply(XrdOlbBounceAll, (void *)&smask);
       PTMutex.UnLock();
      } else {
       struct XrdOlbBNCArgs xargs = {smask, path, strlen(path)};
       PTMutex.Lock();
       PTable.Apply(XrdOlbBounceSome, (void *)&xargs);
       PTMutex.UnLock();
      }
}
  
/******************************************************************************/
/*                               E x t r a c t                                */
/******************************************************************************/

void XrdOlbCache::Extract(char *pathpfx, XrdOucHash<char> *hashp)
{
   struct XrdOlbEXTArgs xargs = {hashp, pathpfx, strlen(pathpfx)};

// Search the cache for all matching elements and insert them into the new hash
//
   PTMutex.Lock();
   PTable.Apply(XrdOlbExtractFN, (void *)&xargs);
   PTMutex.UnLock();
}
  
/******************************************************************************/
/*                                 R e s e t                                  */
/******************************************************************************/
  
void XrdOlbCache::Reset(int servid)
{
     PTMutex.Lock();
     PTable.Apply(XrdOlbClearVec, (void *)&servid);
     PTMutex.UnLock();
}

/******************************************************************************/
/*                                 S c r u b                                  */
/******************************************************************************/
  
void XrdOlbCache::Scrub()
{
     PTMutex.Lock();
     PTable.Apply(XrdOlbScrubScan, (void *)0);
     PTMutex.UnLock();
}
