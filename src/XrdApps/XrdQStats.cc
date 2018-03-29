/******************************************************************************/
/*                                                                            */
/*                          X r d Q S t a t s . c c                           */
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

#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/uio.h>

#include "XrdApps/XrdMpxXml.hh"
#include "XrdCl/XrdClBuffer.hh"
#include "XrdCl/XrdClFileSystem.hh"
#include "XrdCl/XrdClURL.hh"
#include "XrdCl/XrdClXRootDResponses.hh"

using namespace std;
  
/******************************************************************************/
/*                                 F a t a l                                  */
/******************************************************************************/

void Fatal(const XrdCl::XRootDStatus &Status)
{
   std::string eText;

// If this is an xrootd error then get the xrootd generated error
//
   eText = (Status.code == XrdCl::errErrorResponse ?
                           eText = Status.GetErrorMessage() : Status.ToStr());

// Blither and exit
//
   cerr <<"xrdqstats: Unable to obtain statistic - " <<eText <<endl;
   exit(3);
}
  
/******************************************************************************/
/*                                 U s a g e                                  */
/******************************************************************************/
  
void Usage(int rc)
{
   cerr <<"\nUsage: xrdqstats [opts] <host>[:<port>]\n"
          "\nopts: -f {cgi|flat|xml} -h -i <sec> -n <cnt> -s what -z\n"
          "\n-f specify display format (default is wordy text format)."
          "\n-i number of seconds to wait before between redisplays, default 10."
          "\n-n number of redisplays; if -s > 0 and -n unspecified goes forever."
          "\n-z does not display items with a zero value (wordy text only).\n"
          "\nwhat: one or more of the following letters to select statistics:"
          "\na - All (default)   b - Buffer usage    d - Device polling"
          "\ni - Identification  c - Connections     p - Protocols"
          "\ns - Scheduling      u - Usage data      z - Synchronized info"
          <<endl;
   exit(rc);
}

/******************************************************************************/
/*                                  m a i n                                   */
/******************************************************************************/
  
int main(int argc, char *argv[])
{
   extern char *optarg;
   extern int optind, opterr, optopt;
   XrdMpxXml::fmtType fType = XrdMpxXml::fmtText;
   XrdMpxXml *xP = 0;
   const char *Stats = "bldpsu", *pgm = "xrdqstats: ";
   const char *valOpts = "df:i:n:s:z";
   int WTime = 0, Count = 0;
   char obuff[65536];
   char *sP, c;
   bool Debug = false, nozed = false;

// Process the options
//
   opterr = 0;
   if (argc > 1 && '-' == *argv[1]) 
      while ((c = getopt(argc,argv,valOpts)) && ((unsigned char)c != 0xff))
     { switch(c)
       {
       case 'd': Debug = true;
                 break;
       case 'f':      if (!strcmp(optarg, "cgi" )) fType = XrdMpxXml::fmtCGI;
                 else if (!strcmp(optarg, "flat")) fType = XrdMpxXml::fmtFlat;
                 else if (!strcmp(optarg, "xml" )) fType = XrdMpxXml::fmtXML;
                 else {cerr <<pgm <<"Invalid format - " <<optarg <<endl;
                       Usage(1);
                      }
                 break;
       case 'h': Usage(0);
                 break;
       case 'i': if ((WTime = atoi(optarg)) <= 0)
                    {cerr <<pgm <<"Invalid interval - " <<optarg <<endl;
                     Usage(1);
                    }
                 break;
       case 'n': if ((Count = atoi(optarg)) <= 0)
                    {cerr <<pgm <<"Invalid count - " <<optarg <<endl;
                     Usage(1);
                    }
                 break;
       case 's': sP = optarg;
                 while(*sP)
                      {if (!index("abcdipsuz", *sP))
                          {cerr <<pgm<<"Invalid statistic letter - "<<*sP<<endl;
                           Usage(1);
                          } else if (*sP == 'c') *sP = 'l';
                       sP++;
                      }
                 Stats = optarg;
                 break;
       case 'z': nozed = true;
                 break;
       default:  cerr <<pgm <<'-' <<char(optopt);
                 if (c == ':') cerr <<" value not specified." <<endl;
                    else cerr <<" option is invalid" <<endl;
                 Usage(1);
                 break;
       }
     }

// Make sure host has been specified
//
   if (optind >= argc)
      {cerr <<pgm <<"Host has not been specified." <<endl; Usage(1);}

// Construct the URL to get to the server
//
   std::string sURL("root://");
   sURL += argv[optind];
   XrdCl::URL fsURL(sURL);
   if (!fsURL.IsValid())
      {cerr <<pgm <<"Invalid host specification - " <<argv[optind] <<endl;
       Usage(1);
      }

// Establish what we wiil be asking for
//
   XrdCl::Buffer wantStats;
   wantStats.FromString(std::string(Stats));

// Establish count and interval
//
   if (!WTime && Count) WTime = 10;
      else if (WTime && !Count) Count = -1;
              else if (!WTime && !Count) Count = 1;

// Establish format
//
   if (fType != XrdMpxXml::fmtXML) xP = new XrdMpxXml(fType, nozed, Debug);

// Create the file system to query the stats
//
   XrdCl::FileSystem theFS(fsURL);

// Perform statistics gathering and display
//
   XrdCl::Buffer *theStats;
   XrdCl::XRootDStatus xStatus;
   while(Count--)
        {xStatus = theFS.Query(XrdCl::QueryCode::Stats, wantStats, theStats);
         if (!xStatus.IsOK()) Fatal(xStatus);
         if (!xP) std::cout <<theStats->GetBuffer() <<endl;
            else {int rc, wLen = xP->Format(0, theStats->GetBuffer(), obuff);
                  char *bP = obuff;
                  while(wLen > 0)
                       {do {rc = write(STDOUT_FILENO, bP, wLen);}
                                 while(rc < 0 && errno == EINTR);
                        wLen -= rc; bP += rc;
                       }
                 }
         delete theStats;
         if (WTime) sleep(WTime);
         if (Count) cout <<"\n";
        }

// All done
//
   return 0;
}
