/******************************************************************************/
/*                                                                            */
/*                         X r d D i g A u t h . c c                          */
/*                                                                            */
/* (C) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*               DE-AC02-76-SFO0515 with the Deprtment of Energy              */
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
  
#include <unistd.h>
#include <cctype>
#include <fcntl.h>
#include <cstdio>
#include <cstdlib>
#include <strings.h>
#include <sys/param.h>
#include <sys/stat.h>

#include "XrdDig/XrdDigAuth.hh"

#include "XrdNet/XrdNetAddrInfo.hh"

#include "XrdOuc/XrdOucStream.hh"

#include "XrdSys/XrdSysE2T.hh"
#include "XrdSys/XrdSysError.hh"
  
/******************************************************************************/
/*                               d e f i n e s                                */
/******************************************************************************/

#define TS_Xeq(x,m)   if (!strcmp(x,var)) return m(Config);

/******************************************************************************/
/*                 G l o b a l   S t a t i c   O b j e c t s                  */
/******************************************************************************/
  
namespace XrdDig
{
   extern XrdSysError *eDest;

          XrdDigAuth   Auth;
};

using namespace XrdDig;

/******************************************************************************/
/*                   S t a t i c   L o c a l   V a l u e s                    */
/******************************************************************************/
  
namespace
{
   const char eVec[] = "nhorg";

   struct aToks {const char *aTok; XrdDigAuthEnt::aType aRef;} aTab[] =
                {{"conf", XrdDigAuthEnt::aConf},
                 {"core", XrdDigAuthEnt::aCore},
                 {"logs", XrdDigAuthEnt::aLogs},
                 {"proc", XrdDigAuthEnt::aProc}
                };

};

/******************************************************************************/
/*                             A u t h o r i z e                              */
/******************************************************************************/
  
bool XrdDigAuth::Authorize(const XrdSecEntity  *client,
                           XrdDigAuthEnt::aType aType,
                           bool aVec[XrdDigAuthEnt::aNum]
                          )
{
   XrdSysMutexHelper mHelp(&authMutex);
   time_t tNow = time(0);
   XrdDigAuthEnt *aP;
   int rc;

// Check if we need to refresh the auth list
//
   if (tNow >= authCHK)
      {struct stat Stat;
       if ((rc = stat(authFN, &Stat)) && errno != ENOENT)
          {eDest->Emsg("Config",errno,"stat dig auth file", authFN);
           authCHK = tNow + 30;
          } else {
                if (rc) {if (authList) {if (!Refresh()) authCHK = tNow + 30;}
                            else                        authCHK = tNow + 60;
                        }
           else if (authTOD == Stat.st_mtime)           authCHK = tNow +  5;
           else if (!Refresh())                         authCHK = tNow + 30;
          }
      }

// Clear aVec if so supplied (client's auth mask)
//
   if (aVec) memset(aVec, false, XrdDigAuthEnt::aNum);

// Check if we have anything to authorize with
//
   if (!authList) return false;

// Check if we are granting access to this resouce at all
//
   if (aType != XrdDigAuthEnt::aNum && !accOK[aType]) return false;

// Go through the access list and try to match the client
//
   aP = authList;
   while(aP)
    {do {if (strcmp(client->prot, aP->prot)) break;
         if (aP->eChk[XrdDigAuthEnt::eName] && (!client->name ||
             strcmp(client->name, aP->eChk[XrdDigAuthEnt::eName]))) break;

         if (aP->eChk[XrdDigAuthEnt::eHost]
         &&  strcmp(client->addrInfo->Name(""),
                    aP->eChk[XrdDigAuthEnt::eHost])) break;

         if (aP->eChk[XrdDigAuthEnt::eVorg] && (!client->vorg ||
             strcmp(client->vorg, aP->eChk[XrdDigAuthEnt::eVorg]))) break;

         if (aP->eChk[XrdDigAuthEnt::eRole] && (!client->role ||
             strcmp(client->role, aP->eChk[XrdDigAuthEnt::eRole]))) break;

         if (aP->eChk[XrdDigAuthEnt::eGrp ] && (!client->grps ||
             !OkGrp(client->grps, aP->eChk[XrdDigAuthEnt::eGrp ]))) break;

         if (aVec) memcpy(aVec, aP->accOK, XrdDigAuthEnt::aNum);
         return (aType == XrdDigAuthEnt::aNum ? false : aP->accOK[aType]);
        } while(1);
     aP = aP->next;
    }

// Client failed the test
//
   return false;
}

/******************************************************************************/
/*                             C o n f i g u r e                              */
/******************************************************************************/
  
bool XrdDigAuth::Configure(const char *aFN)
{
/*
  Function: Configure authorization (one time call).

  Input:    None.

  Output:   true upon success or false otherwise.
*/

// Establish the location of the auth file (stable string do not copy)
//
   if (!aFN || !(*aFN))
      {eDest->Emsg("Config", "Dig authorization file not specified.");
       return false;
      }

// Initialize authorization
//
   authFN = strdup(aFN);
   SetupAuth(false);
   return true;
}

/******************************************************************************/
/* Private:                      F a i l u r e                                */
/******************************************************************************/
  
bool XrdDigAuth::Failure(int lNum, const char *txt1, const char *txt2)
{
   char buff[256];

   sprintf(buff, "Error in dig authfile line %d:", lNum);
   eDest->Emsg("Auth", buff, txt1, txt2);
   return false;
}

/******************************************************************************/
/* Private:                        O k G r p                                  */
/******************************************************************************/

bool XrdDigAuth::OkGrp(const char *glist, const char *gname)
{
   const char *ghit;
   int glen = strlen(gname);

// Attempt to find a match in the list
//
   do {if (!(ghit = strstr(glist, gname))) return false;
       ghit += glen;
       if (!(*ghit) || *ghit == ' ') return true;
       glist = ghit;
      } while(1);
   return false;
}
  
/******************************************************************************/
/* Private:                        P a r s e                                  */
/******************************************************************************/
  
bool XrdDigAuth::Parse(XrdOucStream &aFile, int lNum)
{
   struct aEntHelper
         {XrdDigAuthEnt *eP;
                      aEntHelper() {eP = new XrdDigAuthEnt;}
                     ~aEntHelper() {if (eP) delete eP;}
         } aEnt;
   static const char *eCode;
   char buff[4096];
   char *var, *rec, *bP = buff;
   int  k, n, bLeft = sizeof(buff);
   bool aOK = false, tfVal;

// Get the record type tokens first
//
   while((var = aFile.GetToken()) && *var)
        {     if (!strcmp(var, "all"))
                 {for (k = 0; k < (int)XrdDigAuthEnt::aNum; k++)
                      aEnt.eP->accOK[k] = true;
                  aOK = true; continue;
                 }
         else if (!strcmp(var, "allow")) break;
         else{if (*var == '-') {tfVal = false; var++;}
                  else tfVal = true;

              for (n = 0; n < (int)XrdDigAuthEnt::aNum; n++)
                   if (!strcmp(var, aTab[n].aTok))
                      {aEnt.eP->accOK[aTab[n].aRef] = tfVal; aOK = true; break;}

              if (n >= (int)XrdDigAuthEnt::aNum)
                 return Failure(lNum, "Invalid token -", var);
             }
        }


// Make sure a type has been specified
//
   if (!aOK) return Failure(lNum, "Information type not specified.");

// Now scan for the security protocol
//
   if (!(var = aFile.GetToken()) || !(*var))
      return Failure(lNum, "Auth protocol not specified.");

// Make sure it is not too big
//
   if (strlen(var) >= sizeof(aEnt.eP->prot))
      return Failure(lNum, "Invalid auth protocol -", var);
   strcpy(aEnt.eP->prot, var);

// Now start getting the auth values
//
   aOK = false;
   while((var = aFile.GetToken()) && *var)
        {if (!(eCode = index(eVec, *var))) // "nhorg" lookup
            return Failure(lNum, "Invalid entity type -", var);
         if (*(var+1) != '=' || !*(var+2))
            return Failure(lNum, "Badly formed entity value in", var);
         n = snprintf(bP, bLeft, "%s", var+2) + 1;
         if ((bLeft -= n) <= 0) break;
         if ((var = index(bP, '\\'))) Squash(var);
         aEnt.eP->eChk[eCode-eVec] = bP; bP += n;
         aOK = true;
        }

// Check if we over-ran the buffer
//
   if (bLeft <= 0) return Failure(lNum, "Too many auth values.");

// Make sure we have somthing here
//
   if (!aOK) return Failure(lNum, "No entity values specified.");

// Create composite mask (we assume no memory failures)
//
   aOK = false;
   for (n = 0; n < (int)XrdDigAuthEnt::aNum; n++)
       if (aEnt.eP->accOK[n]) accOK[n] = aOK = true;
   if(!aOK) return Failure(lNum, "Entity has no effective access.");

// Allocate a new value record
//
   if (!(rec = (char *)malloc(bP-buff)))
      return Failure(lNum, "Insufficient memory.");
   memcpy(rec, buff, bP-buff);
   aEnt.eP->rec = rec;

// Relocate pointers
//
   for (k = (int)XrdDigAuthEnt::eName; k < (int)XrdDigAuthEnt::eNum; k++)
      {if (aEnt.eP->eChk[k])
           aEnt.eP->eChk[k] = rec + (aEnt.eP->eChk[k] - buff);
      }

// Chain this record into the record list and return success
//
    aEnt.eP->next = authList;
    authList = aEnt.eP;
    aEnt.eP = 0;
    return true;
}

/******************************************************************************/
/* Private:                      R e f r e s h                                */
/******************************************************************************/
  
bool XrdDigAuth::Refresh() // authMutex must be locked!
{
   XrdDigAuthEnt *aP, *nP = authList;

// Delete the current auth list
//
   while((aP = nP)) {nP = aP->next; delete aP;}
   authList = 0;

// Resetup the auth list
//
   return SetupAuth(true);
}

/******************************************************************************/
/* Private:                    S e t u p A u t h                              */
/******************************************************************************/

bool XrdDigAuth::SetupAuth(bool isRefresh)
{
   XrdOucStream aFile(eDest);
   struct stat Stat;
   char *line;
   int  authFD, retc, lNum = 1;
   bool NoGo = false;

// Clear summary flags
//
   memset(accOK, 0, sizeof(accOK));

// Print message
//
   eDest->Say("++++++ Dig ", (isRefresh ? "refreshing" : "initializing"),
                             " from ", authFN);

// Try to open the configuration file.
//
   if ( (authFD = open(authFN, O_RDONLY, 0)) < 0)
      {NoGo = errno != ENOENT;
       eDest->Say("Config ",XrdSysE2T(errno)," opening dig auth file ",authFN);
       return SetupAuth(isRefresh, !NoGo);
      }
   aFile.Attach(authFD, 4096);

// Get the time the file was ctreated
//
   if (fstat(authFD, &Stat))
      {eDest->Say("Config ",XrdSysE2T(errno)," stating dig auth file ",authFN);
       close(authFD);
       return SetupAuth(isRefresh, false);
      }
   authTOD = Stat.st_mtime;

// Now start reading records until eof.
//
   while((line = aFile.GetLine()))
        {if (*line && *line != '#') NoGo |= !Parse(aFile, lNum);
         lNum++;
        }

// Now check if any errors occurred during file i/o
//
   if ((retc = aFile.LastError()))
      {eDest->Say("Config ",XrdSysE2T(-retc)," reading config file ",authFN);
       NoGo = true;
      }
   aFile.Close();

// All done
//
   return SetupAuth(isRefresh, !NoGo);
}

/******************************************************************************/
  
bool XrdDigAuth::SetupAuth(bool isRefresh, bool aOK)
{

// Indicate whether we are active or not
//
   if (!authList) eDest->Say("Config ","No users authorized to access digFS; "
                                       "access suspended.");

// All done
//
   eDest->Say("------ Dig auth ", (isRefresh ? "refresh" : "initialization"),
              (aOK ? " succeeded." : " encountered errors."));

   return aOK;
}

/******************************************************************************/
/* Private:                       S q u a s h                                 */
/******************************************************************************/
  
void XrdDigAuth::Squash(char *bP)
{

// Insert spaces where needed
//
   do {if (*(bP+1) == 's') {*bP = ' '; strcpy(bP+1, bP+2);}
      } while((bP = index(bP+1, '\\')));
}
