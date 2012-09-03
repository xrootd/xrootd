#ifndef __LINK_MATCH__
#define __LINK_MATCH__
/******************************************************************************/
/*                                                                            */
/*                       X r d L i n k M a t c h . h h                        */
/*                                                                            */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <strings.h>
#include <stdlib.h>
  
class XrdLinkMatch
{
public:


int                Match(const char *uname, int unlen,
                         const char *hname, int hnlen);
inline int         Match(const char *uname, int unlen,
                         const char *hname)
                        {return Match(uname, unlen, hname, strlen(hname));}

// Target: [<user>][*][@[<hostpfx>][*][<hostsfx>]]
//
       void        Set(const char *target);

             XrdLinkMatch(const char *target=0)
                         {Uname = HnameL = HnameR = 0;
                          Unamelen = Hnamelen = 0;
                          if (target) Set(target);
                         }

            ~XrdLinkMatch() {}

private:

char               Buff[256];
int                Unamelen;
char              *Uname;
int                HnamelenL;
char              *HnameL;
int                HnamelenR;
char              *HnameR;
int                Hnamelen;
};
#endif
