/******************************************************************************/
/*                                                                            */
/*                       X r d C k s A s s i s t . c c                        */
/*                                                                            */
/* (c) 2017 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cctype>
#include <cerrno>
#include <string>
#include <cstring>
#include <strings.h>
#include <ctime>
#include <vector>
#include <sys/types.h>

#include "XrdCks/XrdCksData.hh"

/******************************************************************************/
/*                               S t a t i c s                                */
/******************************************************************************/
  
namespace
{
struct csTable {const char *csName; int csLenC; int csLenB;} csTab[]
               = {{"adler32",   8,   4},
                  {"crc32",     8,   4},
                  {"crc64",    16,   8},
                  {"md5",      32,  16},
                  {"sha1",     40,  20},
                  {"sha2",     64,  32},
                  {"sha256",   64,  32},
                  {"sha512",  128,  64}
                 };

static const int csNum = sizeof(csTab)/sizeof(csTable);

bool LowerCase(const char *src, char *dst, int dstlen)
{
   int n = strlen(src);

// Verify that the result will fit in the supplied buffer with a null
//
   if (n >= dstlen) return false;

// Convert to ower case with trailing nulls
//
   memset(dst+n, 0, dstlen-n);
   for (int i = 0; i < n; i++) dst[i] = tolower(src[i]);
   return true;
}
}

/******************************************************************************/
/*                        X r d C k s A t t r D a t a                         */
/******************************************************************************/

// Return the data portion of a checksum attribute so that it can be use
// to set an attribute value.

std::vector<char> XrdCksAttrData(const char *cstype,
                                 const char *csval, time_t mtime)
{
   std::vector<char> cksError; // Null vector
   XrdCksData cksData;
   char csName[XrdCksData::NameSize];
   int n = strlen(cstype);

// Convert the checksum type to lower case
//
   if (!LowerCase(cstype, csName, sizeof(csName)))
      {errno = ENAMETOOLONG; return cksError;}

// For cheksums we know, verify that the legnth of the input string
// corresponds to the checksum type.
//
   n = strlen(csval);
   for (int i = 0; i < csNum; i++)
       {if (!strcmp(csTab[i].csName, csName) && csTab[i].csLenC != n)
           {errno = EINVAL; return cksError;}
       }

// we simply fill out the cksdata structure with the provided information
//
   if (!cksData.Set(csName))   {errno = ENAMETOOLONG; return cksError;}
   if (!cksData.Set(csval, n)) {errno = EOVERFLOW;    return cksError;}
   cksData.fmTime = mtime;
   cksData.csTime = time(0) - mtime;

// Convert the checksum data to a string of bytes and return the vector
//
   return std::vector<char>( (char *)&cksData,
                            ((char *)&cksData) + sizeof(cksData));
}

/******************************************************************************/
/*                        X r d C k s A t t r N a m e                         */
/******************************************************************************/
  
// Return the extended attribute variable name for a particular checksum type.

std::string XrdCksAttrName(const char *cstype, const char *nspfx)
{
   std::string xaName;
   char csName[XrdCksData::NameSize];
   int  pfxlen = strlen(nspfx);

// Do some easy checks for this we know are common
//
   if (!pfxlen)
      {if (!strcmp(cstype, "adler32")) return std::string("XrdCks.adler32");
       if (!strcmp(cstype, "md5"    )) return std::string("XrdCks.md5");
       if (!strcmp(cstype, "crc32"  )) return std::string("XrdCks.crc32");
      }

// Convert the checksum type to lower case
//
   if (!LowerCase(cstype, csName, sizeof(csName)))
      {errno = ENAMETOOLONG; return xaName;}

// Reserve storage for the string and construct the variable name
//
   xaName.reserve(strlen(nspfx) + strlen(cstype) + 8);
   if (pfxlen)
      {xaName = nspfx;
       if (nspfx[pfxlen-1] != '.') xaName += '.';
      }
   xaName += "XrdCks.";
   xaName += csName;

// Return the variable name
//
   return xaName;
}

/******************************************************************************/
/*                       X r d C k s A t t r V a l u e                        */
/******************************************************************************/
  
std::string XrdCksAttrValue(const char *cstype,
                            const char *csbuff, int csblen)
{
   XrdCksData cksData;
   std::string csError;
   char csBuff[XrdCksData::ValuSize*2+1];

// Verify that the length matches our object length
//
   if (csblen != (int)sizeof(cksData)) {errno = EMSGSIZE; return csError;}

// Copy the buffer into the object
//
   memcpy(&cksData, csbuff, sizeof(cksData));

// Now verify that all the fields are consistent
//
   if (strncasecmp(cksData.Name, cstype, XrdCksData::NameSize))
      {errno = ENOENT; return csError;}
   if (cksData.Length <= 0 || cksData.Length > XrdCksData::ValuSize)
      {errno = EINVAL; return csError;}

// For known checksum values make sure the length matches
//
   for (int i = 0; i < csNum; i++)
       {if (!strcmp(csTab[i].csName, cstype)
        &&  csTab[i].csLenB != int(cksData.Length))
           {errno = EINVAL; return csError;}
       }

// Convert value to a hex string
//
   if (!cksData.Get(csBuff, sizeof(csBuff)))
      {errno = EOVERFLOW; return csError;}

// Return string version of the hex string
//
   return std::string(csBuff);
}
