/******************************************************************************/
/*                                                                            */
/*                           X r d C r c 3 2 . c c                            */
/*                                                                            */
/* (c) 2021 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <iostream>

#include <fcntl.h>
#include <cstdio>
#include <unistd.h>

#include <sys/stat.h>
#include <cstring>
#include <sys/types.h>
#include <sys/uio.h>

#include "XrdOuc/XrdOucCRC.hh"
#include "XrdSys/XrdSysE2T.hh"

using namespace std;

namespace
{const char *pgm = "xrdcrc32c";
}

#ifndef O_DIRECT
#define O_DIRECT 0
#endif
  
/******************************************************************************/
/*                                 F a t a l                                  */
/******************************************************************************/

void Fatal(const char *op, const char *target)
{

// Generate the message
//
   cerr <<"xrdcrc32c: Unable to "<<op<<' '<<target<<"; "<<XrdSysE2T(errno)<<endl;
   exit(3);
}
  
/******************************************************************************/
/*                                 U s a g e                                  */
/******************************************************************************/
  
void Usage(int rc)
{
   cerr <<"\nUsage: xrdcrc32c [opts] {<path> | -}\n"
          "\n<path> the path to the file whose checksum if to be computed."
          "\n-      compute checksum from data presented at standard in;"
          "\n       example: xrdcp <url> - | xrdcrc32c -\n"
          "\nopts: -d -h -n -s -x\n"
          "\n-d read data directly into the buffer, do not use the file cache."
          "\n-h display usage information (arguments ignored)."
          "\n-n do not end output with a newline character."
          "\n-s do not include file path in output result."
          "\n-x do not print leading zeroes in the checksum, if any."
          <<endl;
   exit(rc);
}

/******************************************************************************/
/*                                  m a i n                                   */
/******************************************************************************/
  
int main(int argc, char *argv[])
{
// extern char *optarg;
   extern int optind, opterr, optopt;
   static const int buffSZ = 1024*1024;
   const char *fPath, *fmt = "%08x";
   int bytes, fd, opts = O_RDONLY;
   uint32_t csVal = 0;
   bool addPath = true, addNL = true;
   char csBuff[16], c;

// Process the options
//
   opterr = 0;
   if (argc > 1 && '-' == *argv[1]) 
      while ((c = getopt(argc,argv,"dhnsx")) && ((unsigned char)c != 0xff))
     { switch(c)
       {
       case 'd': opts |= O_DIRECT;
                 break;
       case 'h': Usage(0);
                 break;
       case 'n': addNL = false;
                 break;
       case 's': addPath = false;
                 break;
       case 'x': fmt = "%x";
                 break;
       default:  cerr <<pgm <<'-' <<char(optopt) <<" option is invalid" <<endl;
                 Usage(1);
                 break;
       }
     }

// Make sure a path has been specified
//
   if (optind >= argc)
      {cerr <<pgm <<"File path has not been specified." <<endl; Usage(1);}

// Get the source argument
//
   if (strcmp(argv[optind], "-"))
      {fPath = argv[optind];
       if ((fd = open(fPath, opts)) < 0) Fatal("open", fPath);
      } else {
       fPath = "stdin";
       fd = STDIN_FILENO;
      }

// Allocate a 1 megabyte page aligned buffer
//
   void *buffP;
   int rc = posix_memalign(&buffP, sysconf(_SC_PAGESIZE), buffSZ);
   if (rc) {errno = rc; Fatal("allocate buffer to read", fPath);}

// Compute the checksum
//
   while((bytes = read(fd, buffP, buffSZ)) > 0)
        {csVal = XrdOucCRC::Calc32C(buffP, bytes, csVal);}

// Check if we ended with an error
//
   if (bytes < 0) Fatal("read", fPath);

// Produce the result
//
   sprintf(csBuff, fmt, csVal);
   cout <<(char *)csBuff;
   if (addPath) cout << ' ' <<fPath;
   if (addNL)   cout << endl;

// All done
//
   free(buffP);
   return 0;
}
