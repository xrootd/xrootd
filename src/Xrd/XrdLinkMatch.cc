/******************************************************************************/
/*                                                                            */
/*                       X r d L i n k M a t c h . c c                        */
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

#include <cstring>

#include "Xrd/XrdLinkMatch.hh"
#include "XrdSys/XrdSysPlatform.hh"
 
/******************************************************************************/
/*                                 M a t c h                                  */
/******************************************************************************/
  
int XrdLinkMatch::Match(const char *uname, int unlen, 
                        const char *hname, int hnlen)
{

// Check if we should try to match the username
//
   if (Unamelen && (Unamelen > unlen+1 || strncmp(uname,Uname,Unamelen))) return 0;

// Check if we should match the full host name
//
   if (HnameL && !HnamelenL) return !strcmp(HnameL, hname);

// Check if prefix suffix matching might succeed
//
   if (HnamelenL > hnlen) return 0;

// Check if we should match the host name prefix
//
   if (HnameL && strncmp(HnameL, hname, HnamelenL)) return 0;

// Check if we should match the host name suffix
//
   if (!HnameR) return 1;
   return !strcmp(hname+hnlen-HnamelenR, hname);
}

/******************************************************************************/
/*                                   S e t                                    */
/******************************************************************************/
  
void XrdLinkMatch::Set(const char *target)
{
   char *theast;

// Free any existing target
//
   if (!target || !strcmp(target, "*")) 
      {Uname = HnameL = HnameR = 0; 
       Unamelen = HnamelenL = HnamelenR = 0;
       return;
      }
   strlcpy(Buff, target, sizeof(Buff)-1);
   Uname = Buff;

// Find the '@' as the pivot in this name
//
   if (!(HnameL = index(Uname, '@')))
      {if ((Unamelen = strlen(Uname)))
          {if (Uname[Unamelen-1] == '*') Unamelen--;
              else if (index(Uname, ':')) Uname[Unamelen++] = '@';
                      else if (index(Uname, '.')) Uname[Unamelen++] = ':';
                           else Uname[Unamelen++] = '.';
          }
       HnameR = 0;
       return;
      }

// We have a form of <string>@<string>
//
   *HnameL++ = '\0';
   if ((Unamelen = strlen(Uname)))
      {if (Uname[Unamelen-1] == '*') Unamelen--;
          else if (index(Uname, ':')) Uname[Unamelen++] = '@';
                  else if (index(Uname, '.')) Uname[Unamelen++] = ':';
                       else Uname[Unamelen++] = '.';
      }

// The post string may have an asterisk.
//
   if (!(theast = index(HnameL, '*')))
      {HnamelenL = 0;
       HnameR    = 0;
       return;
      }

// Indicate how much of the prefix should match
//
   *theast = '\0';
   if (!(HnamelenL = strlen(HnameL))) HnameL = 0;

// Indicate how much of the suffix should match
//
   if ((HnamelenR = strlen(theast))) HnameR = theast+1;
      else HnameR = 0;
   Hnamelen = HnamelenL+HnamelenR;
}
