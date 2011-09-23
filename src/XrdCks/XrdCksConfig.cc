/******************************************************************************/
/*                                                                            */
/*                       X r d C k s C o n f i g . c c                        */
/*                                                                            */
/* (c) 2011 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/
  
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
  
#include "XrdCks/XrdCks.hh"
#include "XrdCks/XrdCksData.hh"
#include "XrdCks/XrdCksConfig.hh"
#include "XrdCks/XrdCksManager.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPlugin.hh"

/******************************************************************************/
/*                             C o n f i g u r e                              */
/******************************************************************************/
  
XrdCks *XrdCksConfig::Configure(const char *dfltCalc, int rdsz)
{
   XrdCks *myCks = getCks(rdsz);
   XrdOucTList *tP = CksList;
   int NoGo = 0;

// Check if we have a cks object
//
   if (!myCks) return 0;

// Configure the object
//
   while(tP) {NoGo |= myCks->Config("ckslib", tP->text); tP = tP->next;}

// Configure if all went well
//
   if (!NoGo) NoGo = !myCks->Init(cfgFN, dfltCalc);

// All done
//
   if (NoGo) {delete myCks; myCks = 0;}
   return myCks;
}

/******************************************************************************/
/*                                g e t C k s                                 */
/******************************************************************************/

XrdCks *XrdCksConfig::getCks(int rdsz)
{
   XrdSysPlugin  *myLib;
   XrdCks       *(*ep)(XRDCKSINITPARMS);

// Authorization comes from the library or we use the default
//
   if (!CksLib) return (XrdCks *)new XrdCksManager(eDest, rdsz);

// Create a plugin object (we will throw this away without deletion because
// the library must stay open but we never want to reference it again).
//
   if (!(myLib = new XrdSysPlugin(eDest, CksLib))) return 0;

// Now get the entry point of the object creator
//
   ep = (XrdCks *(*)(XRDCKSINITPARMS))(myLib->getPlugin("XrdCksInit"));
   if (!ep) return 0;

// Get the Object now
//
   return ep(eDest, cfgFN, CksParm);
}
  
/******************************************************************************/
/*                               M a n a g e r                                */
/******************************************************************************/
  
/* Function: Manager

   Purpose:  Reset the manager plugin library path and parameters.

             <path>    path to the library.
             <parms>   optional parms to be passed

  Output: 0 upon success or !0 upon failure.
*/

int XrdCksConfig::Manager(const char *Path, const char *Parms)
{
// Replace the library path and parameters
//
   if (CksLib) free(CksLib);
   CksLib = strdup(Path);
   if (CksParm) free(CksParm);
   CksParm = (Parms  && *Parms ? strdup(Parms) : 0);
   return 0;
}
  
/******************************************************************************/
/*                              P a r s e L i b                               */
/******************************************************************************/
  
/* Function: ParseLib

   Purpose:  To parse the directive: ckslib <digest> <path> [<parms>]

             <digest>  the name of the checksum. The special name "*" is used
                       load the checksum manager library.
             <path>    the path of the checksum library to be used.
             <parms>   optional parms to be passed

  Output: 0 upon success or !0 upon failure.
*/

int XrdCksConfig::ParseLib(XrdOucStream &Config)
{
   static const int nameSize = XrdCksData::NameSize;
   static const int pathSize = MAXPATHLEN;
   static const int parmSize = 1024;
   XrdOucTList *tP;
   char *val, buff[nameSize + pathSize + parmSize + 8], parms[parmSize], *bP;
   int n;

// Get the digest
//
   if (!(val = Config.GetWord()) || !val[0])
      {eDest->Emsg("Config", "ckslib digest not specified"); return 1;}
   n = strlen(val);
   if (n >= nameSize)
      {eDest->Emsg("Config", "ckslib digest name too long -", val); return 1;}
   strcpy(buff, val); bP = buff+n; *bP++ = ' ';

// Get the path
//
   if (!(val = Config.GetWord()) || !val[0])
      {eDest->Emsg("Config", "ckslib path not specified for", buff); return 1;}
   n = strlen(val);
   if (n > pathSize)
      {eDest->Emsg("Config", "ckslib path name too long -", val); return 1;}
   strcpy(bP, val); bP += n;

// Record any parms
//
   *parms = 0;
   if (!Config.GetRest(parms, parmSize))
      {eDest->Emsg("Config", "ckslib parameters too long for", buff); return 1;}

// Check if this is for the manager
//
   if (*buff == '*' && *(buff+1) == ' ') return Manager(buff+2, parms);

// Add this digest to the list of digests
//
   *bP++ = ' '; strcpy(bP, parms);
   tP = new XrdOucTList(buff);
   if (CksLast) CksLast->next = tP;
      else      CksList = tP;
   CksLast = tP;
   return 0;
}
