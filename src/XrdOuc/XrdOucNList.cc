/******************************************************************************/
/*                                                                            */
/*                        X r d O u c N L i s t . c c                         */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*       All Rights Reserved. See XrdInfo.cc for complete License Terms       */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//        $Id$

const char *XrdOucNListCVSID = "$Id$";

#include <string.h>
#include <strings.h>

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
/*                                N a m e O K                                 */
/******************************************************************************/
  
int XrdOucNList::NameOK(const char *pd, const int pl)
{

// Check if exact match wanted
//
   if (namelenR < 0) return !strcmp(pd, (const char *)nameL);

// Make sure the prefix matches
//
   if (namelenL && namelenL <= pl && strncmp(pd,(const char *)nameL,namelenL))
      return 0;

// Make sure suffix matches
//
   if (!namelenR)     return 1;
   if (namelenR > pl) return 0;
   return !strcmp((const char *)(pd + pl - namelenR), (const char *)nameR);
}
