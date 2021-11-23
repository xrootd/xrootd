#ifndef __OSSMIOFILE_H__
#define __OSSMIOFILE_H__
/******************************************************************************/
/*                                                                            */
/*                      X r d O s s M i o F i l e . h h                       */
/*                                                                            */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <ctime>
#include <sys/types.h>
  
class XrdOssMioFile
{
public:
friend class XrdOssMio;

off_t Export(void **Addr) {*Addr = Base; return Size;}

       XrdOssMioFile(char *hname)
                    {strcpy(HashName, hname); 
                     inUse = 1; Next = 0; Size = 0;
                    }
      ~XrdOssMioFile();

private:

XrdOssMioFile *Next;
dev_t          Dev;
ino_t          Ino;
int            Status;
int            inUse;
void          *Base;
off_t          Size;
char           HashName[64];
};
#endif
