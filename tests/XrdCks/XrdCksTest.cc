/******************************************************************************/
/*                                                                            */
/*                       T e s t C h e c k S u m . c c                        */
/*                                                                            */
/* (c) 2026 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
#include <errno.h>
#include <fcntl.h>
#include <initializer_list>
#include <iostream>
#include <strings.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <sys/uio.h>

#include "XrdCl/XrdClFile.hh"
#include "XrdCl/XrdClFileSystem.hh"
#include "XrdCl/XrdClXRootDResponses.hh"

/******************************************************************************/
/*                    G l o b a l   I n f o r m a t i o n                     */
/******************************************************************************/

namespace
{
const char* thePgm;  // Will always have something

const char* csTest="unknown"; // Arbitrary name of test
const char* csName="unknown"; // Name of checksum digest being tested
std::string csValu;           // The checksum value the test should yield
const char* lclFile="";       // Path to the data source
std::string urlFile;          // URL to the server being tested
}

/******************************************************************************/
/*                                 U s a g e                                  */
/******************************************************************************/

void Usage(int rc)
{
   std::cerr <<"\nUsage: "<<thePgm <<" <cstst> <csname> <csval> <lclfile> <urlfile>\n\n"
          "<cstst>   the type of checksum test; an arbitrary value for messages\n"
          "<csname>  the name of the checksum (e.g.md5)\n"
          "<csval>   the hexadeimal value of the checksum\n"
          "<lclfile> path to local file having specified checksum\n"
          "<urlfile> the url to use for the target server test"
          <<std::endl;
   exit(rc);
}

/******************************************************************************/
/*                                  E m s g                                   */
/******************************************************************************/

void Emsg(int rc, std::initializer_list<const char*> txt)
{
   std::cerr <<thePgm <<": ";
   for (const auto& s : txt) std::cerr <<s;
   std::cerr <<std::endl;

   if (rc > 0) Usage(rc);
   if (rc < 0)
      {std::cerr<<thePgm<<' '<<csName<<' '<<csTest<<" test failed!";
       std::cerr <<std::endl;
       exit(-rc);
      }
}

/******************************************************************************/
/*                                 F a t a l                                  */
/******************************************************************************/

void Fatal(const XrdCl::XRootDStatus &Status, const char* what)
{
   std::string eText;

// If this is an xrootd error then get the xrootd generated error
//
   eText = (Status.code == XrdCl::errErrorResponse ?
                           eText = Status.GetErrorMessage() : Status.ToStr());

// Issue message and exit
//
   Emsg(-3, {what, " ", eText.c_str()});
}

/******************************************************************************/
/*                             F l u s h F i l e                              */
/******************************************************************************/

void FlushFile(off_t fOffs, int infd, XrdCl::File& clFile)
{
   char buff[65536];
   XrdCl::XRootDStatus Status;
   ssize_t rwSize = sizeof(buff);

// Copy the file as described
//
   while(rwSize == sizeof(buff))
        {if ((rwSize = pread(infd, buff, rwSize, fOffs)) < 0)
            Emsg(-errno, {"Unable to read the local file; ", strerror(errno)});

         Status = clFile.Write((uint64_t)fOffs, (uint32_t)rwSize, buff);
         if (!Status.IsOK()) Fatal(Status, "Unable to write urlfile;");

         fOffs += rwSize;
        }
}

/******************************************************************************/
/*                           g e t C h e c k s u m                            */
/******************************************************************************/

std::string getCheckSum(XrdCl::FileSystem& clFS, const std::string& path)
{
   XrdCl::Buffer arg(path.size()), *resp = 0;

   arg.FromString(path);
   XrdCl::XRootDStatus Status = clFS.Query(XrdCl::QueryCode::Checksum,arg,resp);

   if (!Status.IsOK())
      Emsg(-3, {"Unable to query checksum for ", path.c_str(), "; ",
                Status.ToStr().c_str()});

   std::string response = resp->ToString();
   delete resp;
   size_t space_pos = response.find(" ");
   if (space_pos == std::string::npos) return response;
   return response.substr(space_pos+1, std::string::npos);
}

/******************************************************************************/
/*                                  m a i n                                   */
/******************************************************************************/

int main(int argc, char *argv[])
{

// Get the name of this program
//
   char* slash = rindex(argv[0], '/');
   thePgm = (slash ? slash+1 : argv[0]);

// There must be exactly 5 arguments.
//
   if (argc < 6) Emsg(3, {"Missing arguments!"});

// Collect the arguments
//
   csTest  = argv[1];
   csName  = argv[2];
   csValu  = argv[3];
   lclFile = argv[4];
   urlFile = argv[5];

// Append checksum name to the url
//
   if (!index(urlFile.c_str(), '?')) urlFile += "?";
   urlFile += "&cks.type="; // It's OK to always use '&' prefix
   urlFile += csName;

// Validate the URL
//
   XrdCl::URL clUrl(urlFile);
   if (!clUrl.IsValid())
      Emsg(-3, {"URL '", urlFile.c_str(), "' is invalid!"});
   if (clUrl.GetPath().empty())
      Emsg(-3, {"URL '", urlFile.c_str(), "' does not specify a path!"});
   XrdCl::FileSystem clFS(clUrl);

// Open the local file
//
   struct stat Stat;
   int lclFD;
   if ((lclFD = open(lclFile, O_RDONLY)) < 0 || fstat(lclFD, &Stat) < 0)
      Emsg(-errno, {"Unable to open ", lclFile, "; ", strerror(errno)});

// Verify that the file has at least 1024 bytes
//
   if (Stat.st_size < 1024)
      Emsg(-3, {"Local file ", lclFile, " is less than 1024 bytes!"});

// Open the output file
//
   XrdCl::XRootDStatus Status;
   XrdCl::File clFile;
   XrdCl::OpenFlags::Flags oFlags = XrdCl::OpenFlags::Delete
                                  | XrdCl::OpenFlags::MakePath;
   XrdCl::Access::Mode oMode = XrdCl::Access::UR | XrdCl::Access::UW;

   Status = clFile.Open(urlFile, oFlags, oMode);
   if (!Status.IsOK()) Fatal(Status, "Unable to open urlfile;");

// We write the first short block of the file, then leave a hole until
// the very end where we then fill it. We know the file is at least 1k.
//
   char buff[512];
   uint64_t halfSZ = sizeof(buff)/2;

   if (pread(lclFD, buff, sizeof(buff), 0) < 0)
      Emsg(-errno, {"Unable to read the local file; ", strerror(errno)});

   Status = clFile.Write(0, halfSZ, buff);
   if (!Status.IsOK()) Fatal(Status, "Unable to write urlfile;");

   FlushFile(halfSZ, lclFD, clFile);

   Status = clFile.Write(halfSZ, halfSZ, buff+halfSZ);
   if (!Status.IsOK()) Fatal(Status, "Unable to write urlfile;");

// Close the file
//
   Status = clFile.Close();
   if (!Status.IsOK()) Fatal(Status, "Unable to close urlfile;");

// Get the checksum of the target file
//
   std::string fileCS = getCheckSum(clFS, clUrl.GetPathWithParams());

// Validate the checksum
//
   if (fileCS != csValu)
      Emsg(-13, {csName, " checksums are not equal '",
                         csValu.c_str(), " != ", fileCS.c_str()});

// The test has succeeded
//
   std::cerr<<thePgm<<' '<<csName<<' '<<csTest<<" test passed.";
   std::cerr <<std::endl;

// All done
//
   return 0;
}
