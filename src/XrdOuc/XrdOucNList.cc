/******************************************************************************/
/*                                                                            */
/*                        X r d O u c N L i s t . c c                         */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
#include "XrdOuc/XrdOucNList.hh"
  
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdOucNList::XrdOucNList(const char *name, int nval)
{
   char *ast;

// Do the default assignments
//
   nameL = strdup(name);
   next  = 0;
   flags = nval;

// First find the asterisk, if any in the name
//
   if ((ast = index(nameL, '*')))
      {namelenL = ast - nameL;
       *ast  = 0;
       nameR = ast+1;
       namelenR = strlen(nameR);
      } else {
       namelenL = strlen(nameL);
       namelenR = -1;
      }
}
 
/******************************************************************************/
/*                                N a m e K O                                 */
/******************************************************************************/
  
int XrdOucNList::NameKO(const char *pd, const int pl)
{

// Check if exact match wanted
//
   if (namelenR < 0) return !strcasecmp(pd, nameL);

// Make sure the prefix matches
//
   if (namelenL && namelenL <= pl && strncasecmp(pd,nameL,namelenL))
      return 0;

// Make sure suffix matches
//
   if (!namelenR)     return 1;
   if (namelenR > pl) return 0;
   return !strcasecmp((pd + pl - namelenR), nameR);
}
 
/******************************************************************************/
/*                                N a m e O K                                 */
/******************************************************************************/
  
int XrdOucNList::NameOK(const char *pd, const int pl)
{

// Check if exact match wanted
//
   if (namelenR < 0) return !strcmp(pd, nameL);

// Make sure the prefix matches
//
   if (namelenL && namelenL <= pl && strncmp(pd,nameL,namelenL))
      return 0;

// Make sure suffix matches
//
   if (!namelenR)     return 1;
   if (namelenR > pl) return 0;
   return !strcmp((pd + pl - namelenR), nameR);
}

/******************************************************************************/
/*                               R e p l a c e                                */
/******************************************************************************/
  
void XrdOucNList_Anchor::Replace(const char *name, int nval)
{
   XrdOucNList *xp = new XrdOucNList(name, nval);

   Replace(xp);
}


void XrdOucNList_Anchor::Replace(XrdOucNList *xp)
{
   XrdOucNList *np, *pp = 0;

// Lock ourselves
//
   Lock();
   np = next;

// Find the matching item or the place to insert the item
//
   while(np && np->namelenL >= xp->namelenL)
        {if (np->namelenL == xp->namelenL
         &&  np->namelenR == xp->namelenR
         && (np->nameL && xp->nameL && !strcmp(np->nameL, xp->nameL))
         && (np->nameR && xp->nameR && !strcmp(np->nameR, xp->nameR)))
            {np->Set(xp->flags);
             UnLock();
             delete xp;
             return;
            }
          pp = np; np = np->next;
         }

// Must insert a new item
//
   if (pp) {xp->next = np; pp->next = xp;}
      else {xp->next = next;   next = xp;}

// All done
//
   UnLock();
}
