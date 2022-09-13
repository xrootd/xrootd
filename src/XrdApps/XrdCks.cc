/******************************************************************************/
/*                                                                            */
/*                             X r d C k s . c c                              */
/*                                                                            */
/* (c) 2022 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>

#include "XrdCks/XrdCksXAttr.hh"
#include "XrdOuc/XrdOucXAttr.hh"

/******************************************************************************/
/*                           G l o b a l   D a t a                            */
/******************************************************************************/

    XrdOucXAttr<XrdCksXAttr> xCS;
    struct stat Stat;
    const char *csCmd;
  
/******************************************************************************/
/*                               D i s p l a y                                */
/******************************************************************************/

void Display()
{
   const char *stale;
   char csBuff[512];

// Check if the checksum is stale
//
   if (xCS.Attr.Cks.fmTime != Stat.st_mtime) stale = " stale";
      else stale = "";

// Get displayable checksum
//
   xCS.Attr.Cks.Get(csBuff, sizeof(csBuff));

// Display the information
//
   std::cout <<xCS.Attr.Cks.Name<<' '<<csBuff<<stale<<std::endl;
}
  
/******************************************************************************/
/*                                U n a b l e                                 */
/******************************************************************************/
  
void Unable(int rc)
{
   char eBuff[256];
   if (rc < 0) rc = -rc;
   snprintf(eBuff, sizeof(eBuff), "%s", strerror(rc));
   *eBuff = tolower(*eBuff);
   std::cerr<<"xrdcks: Unable to "<<csCmd<<" checksum; "<<eBuff<<std::endl;
   exit(5);
}

/******************************************************************************/
/*                                 U s a g e                                  */
/******************************************************************************/
  
void Usage(int rc)
{
   std::cerr <<"Usage: xrdcks <path> <cksname> [<cksval>|delete]\n"
               "       xrdcks -h\n\n"
               "Where: <path>    - path to the target file\n"
               "       <cksname> - checksum name (e.g. adler32)\n"
               "       <cksval>  - ASCII hex of value (even number of digits)"
             <<std::endl;
   exit(rc);
}

/******************************************************************************/
/*                                  m a i n                                   */
/******************************************************************************/
  
int main(int argc, char *argv[])
{
   char *csName, *csPath, *csVal;
   int rc;

// Make sure the right number of arguments are here
//
    if (argc <= 2)
       {if (argc > 1 && !strcmp(argv[1], "-h")) Usage(0);
        Usage(1);
       }

// Verify the name
//
   csName = argv[2];
   if (!xCS.Attr.Cks.Set(csName))
      {std::cerr <<"xrdsetcks: checksum name '"<<csName<<"' is invalid"<<std::endl;
       exit(3);
      }

// Determine what we should be doing
//
   if (argc < 3) csCmd = "query";
      else {csVal = argv[3];
            if (!strcmp("delete", csVal)) csCmd = "delete";
               else {csCmd = "set";
                     if (strncmp("0x", csVal, 2)) csVal += 2;
                     if (!xCS.Attr.Cks.Set(csVal, strlen(csVal)))
                        {std::cerr <<"xrdcks: checksum value is invalid"
                                   <<std::endl;
                         exit(3);
                        }
                    }
           }

// Verify the path
//
   csPath = argv[1];
   if (stat(csPath, &Stat)) Unable(errno);

// Handle query request
//
   if (*csCmd == 'q')
      {if ((rc = xCS.Get(csPath))) Unable(rc);
       if (strcmp(xCS.Attr.Cks.Name, csName)) Unable(EILSEQ);
       Display();
       exit(0);
      }

// Handle delete
//
   if (*csCmd == 'd')
      {if ((rc = xCS.Del(csPath))) Unable(rc);
       exit(0);
      }

// Handle the set
//
   xCS.Attr.Cks.fmTime = static_cast<long long>(Stat.st_mtime);
   xCS.Attr.Cks.csTime = 0;
   if ((rc = xCS.Set(csPath))) Unable(rc);
   exit(0);
}
