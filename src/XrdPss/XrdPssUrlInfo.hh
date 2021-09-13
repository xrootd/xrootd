#ifndef _XRDPSS_URLINFO_H
#define _XRDPSS_URLINFO_H
/******************************************************************************/
/*                                                                            */
/*                      X r d P s s U r l I n f o . h h                       */
/*                                                                            */
/* (c) 2018 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cstdio>

class XrdOucEnv;

class XrdPssUrlInfo
{
public:

      bool  addCGI(const char *prot, char *buff, int blen);

      bool  Extend(const char *cgi, int cgiln);

const char *getID() {return theID;}

      bool  hasCGI() {return CgiSsz || CgiUsz;}

      void  setID(const char *tid=0);

      void  setID(XrdOucSid *sP)
                 {if (sP != 0 && !(sP->Obtain(&idVal))) return;
                  sidP = sP;
                  snprintf(theID, sizeof(theID), "p%d@", idVal.sidS);
                 }

static void setMapID(bool onoff) {MapID = onoff;}

const char *thePath() {return Path;}

const char *Tident() {return tident;}

      XrdPssUrlInfo(XrdOucEnv *envP, const char *path, const char *xtra="",
                    bool addusrcgi=true, bool addident=true);

     ~XrdPssUrlInfo() {if (*theID == 'p' && sidP) sidP->Release(&idVal);}

private:

static bool       MapID;

const char       *tident;
const char       *Path;
const char       *CgiUsr;
      int         CgiUsz;
      int         CgiSsz;
      XrdOucSid  *sidP;
unsigned
      int         entityID;
      bool        eIDvalid;
      char        theID[13];
XrdOucSid::theSid idVal;
      char        CgiSfx[512];
};
#endif
