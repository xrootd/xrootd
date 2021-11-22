/******************************************************************************/
/*                                                                            */
/*                   X r d O u c G a t h e r C o n f . c c                    */
/*                                                                            */
/* (c) 2021 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*                DE-AC02-76-SFO0515 with the Deprtment of Energy             */
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

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucGatherConf.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucString.hh"
#include "XrdOuc/XrdOucTList.hh"
#include "XrdSys/XrdSysError.hh"

/******************************************************************************/
/*                        C o n s t r u c t o r   # 1                         */
/******************************************************************************/

XrdOucGatherConf::XrdOucGatherConf(const char *want, XrdSysError *errP)
                 : XrdOucTokenizer(0), eDest(errP), Match(0), gBuff(0)
{
   XrdOucString wlist(want), wtoken;
   int wlen, wPos = 0;
  
   while((wPos = wlist.tokenize(wtoken, wPos, ' ')) != -1)
        {wlen = (wtoken.endswith('.') ? wtoken.length() : 0);
         Match = new XrdOucTList(wtoken.c_str(), wlen, Match);
        }
}

/******************************************************************************/
/*                        C o n s t r u c t o r   # 2                         */
/******************************************************************************/

XrdOucGatherConf::XrdOucGatherConf(const char **&want, XrdSysError *errP)
                 : XrdOucTokenizer(0), eDest(errP), Match(0), gBuff(0)
{
   int n, i = 0;

   while(want[i])
       {if ((n = strlen(want[i])))
           {if (*(want[i]+(n-1)) != '.') n = 0;
            Match = new XrdOucTList(want[i], n, Match);
           }
       }
}

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/
  
XrdOucGatherConf::~XrdOucGatherConf()
{
   XrdOucTList *tP;

   while((tP = Match))
        {Match = tP->next;
         delete tP;
        }

   if (gBuff) free(gBuff);
}

/******************************************************************************/
/*                                G a t h e r                                 */
/******************************************************************************/
  
int XrdOucGatherConf::Gather(const char *cfname, Level lvl, const char *parms)
{
   XrdOucEnv myEnv;
   XrdOucStream Config(eDest, getenv("XRDINSTANCE"), &myEnv, "=====> ");
   XrdOucTList *tP;
   XrdOucString theGrab;
   char *var, drctv[64], body[4096];
   int cfgFD, n, rc;
   bool trim = false, addKey = true;

// Make sure we have something to compare
//
   if (!Match) return 0;

// Reset the buffer if it has been set
//
   if (gBuff) {free(gBuff); gBuff = 0; Attach(0);}

// Open the config file
//
   if ( (cfgFD = open(cfname, O_RDONLY, 0)) < 0)
      {rc = errno;
       if (eDest) eDest->Emsg("Gcf", rc, "open config file", cfname);
       return -rc;
      }

// Attach the file to our stream object and size the grab buffer
//
   Config.Attach(cfgFD, 4096);
   theGrab.resize(4096);
   if (parms && *parms) theGrab = parms;

// Setup for processing
//
   switch(lvl)
         {case full_lines: *drctv = '\n'; trim = false; addKey = true;  break;
          case trim_lines: *drctv = '\n'; trim = true;  addKey = true;  break;
          case only_body:  *drctv = ' ';  trim = false; addKey = false; break;
          case trim_body:  *drctv = ' ';  trim = true;  addKey = true;  break;
          default: break;  return 0;    // Should never happen
                           break;
         }

// Process the config file
//
   while((var = Config.GetMyFirstWord()))
        {tP = Match;
         while(tP && ((tP->val && strncmp(var, tP->text, tP->val)) ||
                                  strcmp( var, tP->text))) tP = tP->next;

         if (tP)
            {if (addKey)
                {if (trim)
                    {char *dot = index(var, '.');
                     if (dot && *(dot+1)) var = dot+1;
                    }
                 int n =  snprintf(drctv+1, sizeof(drctv)-1, "%s ", var);
                 if (n >= (int)sizeof(drctv)-1)
                    {if (eDest) eDest->Emsg("Gcf", E2BIG, "handle", var);
                     return -E2BIG;
                    }
                } else drctv[1] = 0;

              if (!Config.GetRest(body, sizeof(body)))
                 {if (eDest) eDest->Emsg("Gcf", E2BIG, "handle arguments");
                  return -E2BIG;
                 }

              if (*body || addKey)
                 {theGrab += drctv;
                  theGrab += body;
                 }
            }
        }

// Now check if any errors occurred during file i/o
//
   if ((rc = Config.LastError()))
      {if (eDest) eDest->Emsg("Gcf", rc, "read config file", cfname);
       return (rc < 0 ? rc : -rc);
      }


// Copy the grab to a modifiable buffer.
//
   if ((n = theGrab.length()) <= 1) n = 0;
      else {gBuff = (char *)malloc(n);
            strcpy(gBuff, theGrab.c_str()+1); // skip 1st byte but add null
            Attach(gBuff);
            n--;
           }
   return n;
}

/******************************************************************************/
/*                               h a s D a t a                                */
/******************************************************************************/
  
bool XrdOucGatherConf::hasData()
{
   return gBuff != 0 && *gBuff != 0;
}

/******************************************************************************/
/*                               u s e D a t a                                */
/******************************************************************************/
  
bool XrdOucGatherConf::useData(const char *data)
{
   if (!data || *data == 0) return false;

   if (gBuff) free(gBuff);
   gBuff = strdup(data);
   Attach(gBuff);
   return true;
}
