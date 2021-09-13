/******************************************************************************/
/*                                                                            */
/*                    X r d O u c T o k e n i z e r . c c                     */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Deprtment of Energy               */
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

#ifndef WIN32
#include <unistd.h>
#endif
#include <cctype>
#include <cstdlib>
#include <cstring>

#include "XrdOuc/XrdOucTokenizer.hh"

/******************************************************************************/
/*                                A t t a c h                                 */
/******************************************************************************/
  
void XrdOucTokenizer::Attach(char *bp)
{
     buff  = bp;
     token = 0;
     tnext = (char *)"";
     notabs = 0;
}

/******************************************************************************/
/*                               G e t L i n e                                */
/******************************************************************************/
  
char *XrdOucTokenizer::GetLine()
{
   char *bp;

// Check if end of buffer has been reached.
//
   if (*buff == '\0') return (char *)NULL;

// Find the next record in the buffer
//
   bp = buff;
   if (notabs)
            while(*bp && (*bp == ' ' || *bp == '\t')) bp++;
       else while(*bp &&  *bp == ' '                ) bp++;

   tnext = bp;

// Find the end of the record
//
   if (notabs)
            while(*bp && *bp != '\n') {if (*bp == '\t') *bp = ' '; bp++;}
       else while(*bp && *bp != '\n') bp++;

// Set the end of the line
//
   if (*bp) {*bp = '\0'; buff = bp+1;}
      else buff = bp;

// All done
//
   token = 0;
   return tnext;
}

/******************************************************************************/
/*                              G e t T o k e n                               */
/******************************************************************************/
  
char *XrdOucTokenizer::GetToken(char **rest, int lowcase)
{

     // Skip to the first non-blank character.
     //
     while (*tnext && *tnext == ' ') tnext++;
     if (!*tnext) return (char *)NULL;
     token = tnext;

     // Find the end of the token.
     //
     if (lowcase) while (*tnext && *tnext != ' ')
                        {*tnext = (char)tolower((int)*tnext); tnext++;}
        else      while (*tnext && *tnext != ' ') {tnext++;}
     if (*tnext) {*tnext = '\0'; tnext++;}

     // Check if remaining line is to be returned
     //
     if (rest)
        {while (*tnext && *tnext == ' ') tnext++;
         *rest = tnext;
        }

     // All done here.
     //
     return token;
}

/******************************************************************************/
/*                              R e t T o k e n                               */
/******************************************************************************/
  
void XrdOucTokenizer::RetToken()
{
     // Backup one token, we can only back up once
     //
     if (token)
        {if (*tnext) token[strlen(token)] = ' ';
         tnext = token;
         token = 0;
        }
}
