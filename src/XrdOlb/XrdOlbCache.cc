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
  
#include "XrdOuc/XrdOucLink.hh"
#include "XrdOlb/XrdOlbCache.hh"

/******************************************************************************/
/*                      L o c a l   S t r u c t u r e s                       */
/******************************************************************************/
  
struct XrdOlbEXTArgs
       {char            *ppfx;
        int              plen;
        XrdOucHash<char> *hp;
       };

/******************************************************************************/
/*                    E x t e r n a l   F u n c t i o n s                     */
/******************************************************************************/
  
int XrdOlbClearVec(const char *key, XrdOlbCInfo *cinfo, void *sid)
{
    SMask_t smask = 1<<*(int *)sid;

// Clear the vector for this server
//
   smask = ~smask;
   cinfo->rovec &= smask;
   cinfo->rwvec &= smask;

// Return indicating whether we should delete this or not
//
   return (cinfo->rovec || cinfo->rwvec ? 0 : -1);
}

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

int XrdOlbScrubScan(const char *key, XrdOlbCInfo *cip, void *xargp)
{
   return 0;
}

/******************************************************************************/
/*                               A d d F i l e                                */
/******************************************************************************/
  
void XrdOlbCache::AddFile(char *path, SMask_t mask, int isrw, int dltime)
{
   XrdOlbPInfo  pinfo;
   XrdOlbCInfo *cinfo;

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
           cinfo->rovec = 0;
           cinfo->rwvec = 0;
          } else {
           cinfo->rovec |=  mask;
           if (isrw) cinfo->rwvec |=  mask;
              else   cinfo->rwvec &= ~mask;
          }
      } else if (dltime)
                {cinfo = new XrdOlbCInfo();
                 cinfo->rovec = mask;
                 if (isrw) cinfo->rwvec =  mask;
                    else   cinfo->rwvec = 0;
                 if (dltime > 0) cinfo->deadline = dltime + time(0);
                 PTable.Add(path, cinfo, LifeTime);
                }

// All done
//
   PTMutex.UnLock();
}
  
/******************************************************************************/
/*                               D e l F i l e                                */
/******************************************************************************/
  
void XrdOlbCache::DelFile(char *path, SMask_t mask)
{
   XrdOlbCInfo *cinfo;

// Lock the hash table
//
   PTMutex.Lock();

// Look up the entry and remove server
//
   if ((cinfo = PTable.Find(path)))
      {cinfo->rovec &= ~mask;
       cinfo->rwvec &= ~mask;
      }

// All done
//
   PTMutex.UnLock();
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
/*                               E x t r a c t                                */
/******************************************************************************/

void XrdOlbCache::Extract(char *pathpfx, XrdOucHash<char> *hashp)
{
   struct XrdOlbEXTArgs xargs = {pathpfx, strlen(pathpfx), hashp};

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
