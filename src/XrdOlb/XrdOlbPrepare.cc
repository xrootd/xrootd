/******************************************************************************/
/*                                                                            */
/*                      X r d O l b P r e p a r e . c c                       */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//         $Id$

const char *XrdOlbPrepareCVSID = "$Id$";
  
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "Experiment/Experiment.hh"

#include "XrdOlb/XrdOlbPrepare.hh"
#include "XrdOlb/XrdOlbTrace.hh"
#include "XrdOuc/XrdOucError.hh"
#include "XrdOuc/XrdOucTList.hh"

/******************************************************************************/
/*                    E x t e r n a l   F u n c t i o n s                     */
/******************************************************************************/

extern XrdOucError  XrdOlbSay;
  
extern XrdOucTrace  XrdOlbTrace;

int XrdOlbScrubScan(const char *key, char *cip, void *xargp)
{
   struct stat buf;
   if (stat(key, &buf)) return 0;
   return -1;
}

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdOlbPrepare::XrdOlbPrepare() : XrdOlbJob("File cache scrubber"),
                                 prepSched(&XrdOlbSay)
{prepif  = 0;
 preppid = 0;
 resetcnt = scrub2rst = 3;
 scrubtime= 20*60;
 SchedP = 0;
 NumFiles = 0;
 lastemsg = time(0);
}

/******************************************************************************/
/*                                   A d d                                    */
/******************************************************************************/
  
int XrdOlbPrepare::Add(XrdOlbPrepArgs &pargs)
{
   char *pdata[12];
   int rc, pdlen[12];

// Restart the scheduler if need be
//
   PTMutex.Lock();
   if (!prepif || !prepSched.isAlive())
      {XrdOlbSay.Emsg("Add", "No prepare manager; prepare", pargs.reqid,
                             (char *)"ignored.");
       PTMutex.UnLock();
       return 0;
      }

// Write out the header line
//
   pdata[0] = (char *)"+ ";
   pdlen[0] = 2;
   pdata[1] = pargs.reqid;
   pdlen[1] = strlen(pargs.reqid);
   pdata[2] = (char *)" ";
   pdlen[2] = 1;
   pdata[3] = pargs.user;
   pdlen[3] = strlen(pargs.user);
   pdata[4] = (char *)" ";
   pdlen[4] = 1;
   pdata[5] = pargs.prty;
   pdlen[5] = strlen(pargs.prty);
   pdata[6] = (char *)" ";
   pdlen[6] = 1;
   pdata[7] = pargs.mode;
   pdlen[7] = strlen(pargs.mode);
   pdata[8] = (char *)" ";
   pdlen[8] = 1;
   pdata[9] = pargs.path;
   pdlen[9] = strlen(pargs.path);
   pdata[10] = (char *)"\n";
   pdlen[10] = 1;
   pdata[11]= 0;
   pdlen[11]= 0;
   if (!(rc = prepSched.Put((const char **)pdata, (const int *)pdlen)))
      {PTable.Add(pargs.path, 0, 0, Hash_data_is_key); NumFiles++;}

// All done
//
   PTMutex.UnLock();
   return rc == 0;
}

/******************************************************************************/
/*                                   D e l                                    */
/******************************************************************************/
  
int XrdOlbPrepare::Del(char *reqid)
{
   char *pdata[4];
   int rc, pdlen[4];

// Restart the scheduler if need be
//
   PTMutex.Lock();
   if (!prepif || !prepSched.isAlive())
      {XrdOlbSay.Emsg("Del", "No prepare manager; unprepare", reqid,
                      (char *)"ignored.");
       PTMutex.UnLock();
       return 0;
      }

// Write out the delete request
//
   pdata[0] = (char *)"- ";
   pdlen[0] = 2;
   pdata[1] = reqid;
   pdlen[1] = strlen(reqid);
   pdata[2] = (char *)"\n";
   pdlen[2] = 1;
   pdata[3] = (char *)0;
   pdlen[3] = 0;
   rc = prepSched.Put((const char **)pdata, (const int *)pdlen) == 0;
   PTMutex.UnLock();
   return rc;
}
 
/******************************************************************************/
/*                                E x i s t s                                 */
/******************************************************************************/
  
int  XrdOlbPrepare::Exists(char *path)
{
   int Found;

// Lock the hash table
//
   PTMutex.Lock();

// Look up the entry
//
   Found = (NumFiles ? PTable.Find(path) != 0 : 0);

// All done
//
   PTMutex.UnLock();
   return Found;
}
 
/******************************************************************************/
/*                                  G o n e                                   */
/******************************************************************************/
  
void XrdOlbPrepare::Gone(char *path)
{

// Lock the hash table
//
   PTMutex.Lock();

// Delete the entry
//
   if (NumFiles > 0 && PTable.Del(path) == 0) NumFiles--;

// All done
//
   PTMutex.UnLock();
}

/******************************************************************************/
/*                              s e t P a r m s                               */
/******************************************************************************/
  
int XrdOlbPrepare::setParms(int rcnt, int stime, int deco)
{if (rcnt  > 0) resetcnt  = scrub2rst = rcnt;
 if (stime > 0) scrubtime = stime;
 doEcho = deco;
 return 0;
}

int XrdOlbPrepare::setParms(char *ifpgm)
{if (ifpgm)
    {if (prepif) free(prepif);
     prepif = strdup(ifpgm);
    }
 return 0;
}
 
/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                                 R e s e t                                  */
/******************************************************************************/
  
int XrdOlbPrepare::Reset()  // Must be called with PTMutex locked!
{
     const char *epname = "Reset";
     char *lp,  *pdata[] = {(char *)"?\n", 0};
     int ok = 0, pdlen[] = {2, 0};

     if (!prepif)
        XrdOlbSay.Emsg("Reset", "Prepare program not specified; prepare disabled.");
        else {scrub2rst = resetcnt;
              if (!prepSched.isAlive() && !startIF()) return 0;
              if (prepSched.Put((const char **)pdata, (const int *)pdlen))
                 {XrdOlbSay.Emsg("Prepare", prepSched.LastError(),
                                 (char *)"writing to", prepif);
                  prepSched.Drain();
                 }
                 else {PTable.Purge(); ok = 1; NumFiles = 0;
                       while((lp = prepSched.GetLine()) && *lp)
                            {PTable.Add(lp, 0, 0, Hash_data_is_key);
                             NumFiles++;
                             if (doEcho) 
                                XrdOlbSay.Emsg("Reset","Prepare pending for",lp);
                            }
                      }
             }
    return ok;
}

/******************************************************************************/
/*                                 S c r u b                                  */
/******************************************************************************/
  
void XrdOlbPrepare::Scrub()
{
     PTMutex.Lock();
     if (scrub2rst <= 0) Reset();
        else {PTable.Apply(XrdOlbScrubScan, (void *)0);
              scrub2rst--;
             }
     if (!prepSched.isAlive()) startIF();
     PTMutex.UnLock();
}

/******************************************************************************/
/*                               s t a r t I F                                */
/******************************************************************************/
  
int XrdOlbPrepare::startIF()  // Must be called with PTMutex locked!
{   const char *epname = "startIF";
    int NoGo = 0;

    if (!prepif)
       {XrdOlbSay.Emsg("startIF","Prepare program not specified; prepare disabled.");
        NoGo = 1;
       }
       else {DEBUG("Prepare: Starting " <<prepif);
             if (NoGo = prepSched.Exec(prepif, 1))
                {time_t eNow = time(0);
                 if ((eNow - lastemsg) >= 60)
                    {lastemsg = eNow;
                     XrdOlbSay.Emsg("Prepare", prepSched.LastError(),
                                    (char *)"starting", prepif);
                    }
                }
            }
    return !NoGo;
}
