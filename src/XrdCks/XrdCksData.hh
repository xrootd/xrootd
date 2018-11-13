#ifndef __XRDCKSDATA_HH__
#define __XRDCKSDATA_HH__
/******************************************************************************/
/*                                                                            */
/*                         X r d C k s D a t a . h h                          */
/*                                                                            */
/* (c) 2011 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <string.h>

class XrdOucEnv;

class XrdCksData
{
public:

static const int NameSize = 16; // Max name  length is NameSize - 1
static const int ValuSize = 64; // Max value length is 512 bits

char      Name[NameSize];       // Checksum algorithm name
union    {
long long fmTime;               // File's mtime when checksum was computed.
const
char     *tident;               // Set for get & calc only if proxy server!
         };
int       csTime;               // Delta from fmTime when checksum was computed.
short     Rsvd1;                // Reserved field
char      Rsvd2;                // Reserved field
char      Length;               // Length, in bytes, of the checksum value
char      Value[ValuSize];      // The binary checksum value

inline
int       operator==(const XrdCksData &oth)
                    {return (!strncmp(Name, oth.Name, NameSize)
                         &&  Length == oth.Length
                         &&  !memcmp(Value, oth.Value, Length));
                    }

inline
int       operator!=(const XrdCksData &oth)
                    {return (strncmp(Name, oth.Name, NameSize)
                         ||  Length != oth.Length
                         ||  memcmp(Value, oth.Value, Length));
                    }

int       Get(char *Buff, int Blen)
             {const char *hv = "0123456789abcdef";
              int i, j = 0;
              if (Blen < Length*2+1) return 0;
              for (i = 0; i < Length; i++)
                  {Buff[j++] = hv[(Value[i] >> 4) & 0x0f];
                   Buff[j++] = hv[ Value[i]       & 0x0f];
                  }
              Buff[j] = '\0';
              return Length*2;
             }

int       Set(const char *csName)
             {size_t len = strlen(csName);
              if (len >= sizeof(Name)) return 0;
              memcpy(Name, csName, len);
	      Name[len]=0;
              return 1;
             }

int       Set(const void *csVal, int csLen)
             {if (csLen > ValuSize || csLen < 1) return 0;
              memcpy(Value, csVal, csLen);
              Length = csLen;
              return 1;
             }

int       Set(const char *csVal, int csLen)
             {int n, i = 0, Odd = 0;
              if (csLen > (int)sizeof(Value)*2 || (csLen & 1)) return 0;
              Length = csLen/2;
              while(csLen--)
                   {     if (*csVal >= '0' && *csVal <= '9') n = *csVal-48;
                    else if (*csVal >= 'a' && *csVal <= 'f') n = *csVal-87;
                    else if (*csVal >= 'A' && *csVal <= 'F') n = *csVal-55;
                    else return 0;
                    if (Odd) Value[i++] |= n;
                       else  Value[i  ]  = n << 4;
                    csVal++; Odd = ~Odd;
                   }
              return 1;
             }

          void Reset()
                    {memset(Name, 0, sizeof(Name));
                     memset(Value,0, sizeof(Value));
                     fmTime = 0;
                     csTime = 0;
                     Rsvd1  = 0;
                     Rsvd2  = 0;
                     Length = 0;
                    }

          XrdCksData()
                       {Reset();}

bool      HasValue()
                  {
                   return *Value;
                  }
};
#endif
