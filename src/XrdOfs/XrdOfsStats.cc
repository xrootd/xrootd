/******************************************************************************/
/*                                                                            */
/*                        X r d O f s S t a t s . c c                         */
/*                                                                            */
/* (c) 2009 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdOfs/XrdOfsStats.hh"

/******************************************************************************/
/*                                R e p o r t                                 */
/******************************************************************************/
  
int XrdOfsStats::Report(char *buff, int blen)
{
    static const char stats1[] = "<stats id=\"ofs\"><role>%s</role>"
           "<opr>%d</opr><opw>%d</opw><opp>%d</opp><ups>%d</ups><han>%d</han>"
           "<rdr>%d</rdr><bxq>%d</bxq><rep>%d</rep><err>%d</err><dly>%d</dly>"
           "<sok>%d</sok><ser>%d</ser>"
           "<tpc><grnt>%d</grnt><deny>%d</deny><err>%d</err><exp>%d</exp></tpc>"
           "</stats>";
    static const int  statsz = sizeof(stats1) + (12*10) + 64;

    StatsData myData;

// If only the size is wanted, return the size
//
   if (!buff) return statsz;

// Make sure buffer is large enough
//
   if (blen < statsz) return 0;

// Get a copy of the statistics
//
   sdMutex.Lock();
   myData = Data;
   sdMutex.UnLock();

// Format the buffer
//
   return sprintf(buff, stats1, myRole, myData.numOpenR,   myData.numOpenW,
                    myData.numOpenP,    myData.numUnpsist, myData.numHandles,
                    myData.numRedirect, myData.numStarted, myData.numReplies,
                    myData.numErrors,   myData.numDelays,
                    myData.numSeventOK, myData.numSeventER,
                    myData.numTPCgrant, myData.numTPCdeny,
                    myData.numTPCerrs,  myData.numTPCexpr);
}
