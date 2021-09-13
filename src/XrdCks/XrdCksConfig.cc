/******************************************************************************/
/*                                                                            */
/*                       X r d C k s C o n f i g . c c                        */
/*                                                                            */
/* (c) 2011 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
  
#include <cerrno>
#include <cstring>
#include <cstdio>
#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>

#include "XrdVersion.hh"
  
#include "XrdCks/XrdCks.hh"
#include "XrdCks/XrdCksData.hh"
#include "XrdCks/XrdCksConfig.hh"
#include "XrdCks/XrdCksManager.hh"
#include "XrdCks/XrdCksManOss.hh"
#include "XrdCks/XrdCksWrapper.hh"
#include "XrdOuc/XrdOucPinLoader.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucUtils.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdSys/XrdSysPlugin.hh"

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdCksConfig::XrdCksConfig(const char *cFN, XrdSysError *Eroute, int &aOK,
                           XrdVersionInfo &vInfo)
                          : eDest(Eroute), cfgFN(cFN), CksLib(0), CksParm(0),
                            CksList(0), CksLast(0), LibList(0), LibLast(0),
                            myVersion(vInfo)
{
   static XrdVERSIONINFODEF(myVer, XrdCks, XrdVNUMBER, XrdVERSION);

// Verify caller's version against ours
//
   if (vInfo.vNum <= 0 || vInfo.vNum == myVer.vNum
   ||  XrdSysPlugin::VerCmp(vInfo, myVer)) aOK = 1;
      else aOK = 0;
}

/******************************************************************************/
/*                                a d d C k s                                 */
/******************************************************************************/

XrdCks *XrdCksConfig::addCks(XrdCks *pCks, XrdOucEnv *envP)
{
   XrdOucPinLoader *myLib;
   XrdCks          *(*ep)(XRDCKSADD2PARMS);
   const char      *theParm;
   XrdOucTList     *tP = LibList;

// Create a plugin object (we will throw this away without deletion because
// the library must stay open but we never want to reference it again).
//
   while(tP)
        {if (!(myLib = new XrdOucPinLoader(eDest,&myVersion,"ckslib",tP->text)))
            return 0;

         // Now get the entry point of the object creator
         //
         ep = (XrdCks *(*)(XRDCKSADD2PARMS))(myLib->Resolve("XrdCksAdd2"));
         if (!ep) {myLib->Unload(true); return 0;}

         // Get the Object now
         //
         delete myLib;
         theParm = (tP->val ? (tP->text) + tP->val : 0);
         pCks = ep(*pCks, eDest, cfgFN, theParm, envP);
         if (!pCks) return 0;

         // Move on to the next stacked plugin
         //
         tP = tP->next;
        }

// All done
//
   return pCks;
}

/******************************************************************************/
/*                             C o n f i g u r e                              */
/******************************************************************************/
  
XrdCks *XrdCksConfig::Configure(const char *dfltCalc, int rdsz,
                                XrdOss *ossP, XrdOucEnv *envP)
{
   XrdCks *myCks = getCks(ossP, rdsz);
   XrdOucTList *tP = CksList;
   int NoGo = 0;

// Check if we have a cks object
//
   if (!myCks) return 0;

// Stack all pugins
//
   myCks = addCks(myCks, envP);
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
/* Private:                       g e t C k s                                 */
/******************************************************************************/

XrdCks *XrdCksConfig::getCks(XrdOss *ossP, int rdsz)
{
   XrdOucPinLoader *myLib;
   XrdCks          *(*ep)(XRDCKSINITPARMS);

// Cks manager comes from the library or we use the default
//
   if (!CksLib)
      {if (ossP) return (XrdCks *)new XrdCksManOss (ossP,eDest,rdsz,myVersion);
          else   return (XrdCks *)new XrdCksManager(     eDest,rdsz,myVersion);
      }

// Create a plugin object (we will throw this away without deletion because
// the library must stay open but we never want to reference it again).
//
   if (!(myLib = new XrdOucPinLoader(eDest, &myVersion, "ckslib", CksLib)))
      return 0;

// Now get the entry point of the object creator
//
   ep = (XrdCks *(*)(XRDCKSINITPARMS))(myLib->Resolve("XrdCksInit"));
   if (!ep) {myLib->Unload(true); return 0;}

// Get the Object now
//
   delete myLib;
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

int XrdCksConfig::ParseLib(XrdOucStream &Config, int &libType)
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
   strcpy(buff, val); XrdOucUtils::toLower(buff); bP = buff+n; *bP++ = ' ';

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
   if ((*buff == '*' || *buff == '=') && *(buff+1) == ' ')
      {libType = (*buff == '*' ? -1 : 1);
       return Manager(buff+2, parms);
      } else libType = 0;

// Create a new TList object either for a digest or stackable library
//
   n = (strncmp(buff, "++ ", 3) ? 0 : 3);
   *bP = ' '; strcpy(bP+1, parms);
   tP = new XrdOucTList(buff + n);

// Add this digest to the list of digests or stackable library list
//
   if (n)
      {n = (bP - buff) - n;
       tP->text[n] = 0;
       tP->val = (*parms ? n+1 : 0);
       if (LibLast) LibLast->next = tP;
          else      LibList = tP;
       LibLast = tP;
      } else {
       if (CksLast) CksLast->next = tP;
          else      CksList = tP;
       CksLast = tP;
      }
   return 0;
}
