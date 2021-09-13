/******************************************************************************/
/*                                                                            */
/*                          X r d O u c U r i . c c                           */
/*                                                                            */
/******************************************************************************/
  
/*
This software is Copyright (c) 2016 by David Farrell.

Source: https://github.com/dnmfarrell/URI-Encode-C

Modified by Andrew Hanushevsky, SLAC.
Changes: a) Make this C++ looking.
         b) Use XRoootD naming conventions.
         c) Avoid relying on unaligned uint32_t fetches and stores.
         d) Reduce the encode table size by 50%.
         e) Add a couple of handy methods.
         f) General code simplifications.

This is free software, licensed under:

  The (two-clause) FreeBSD License

The FreeBSD License

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the
     distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <cstdlib>
#include <cstring>

#include "XrdOuc/XrdOucUri.hh"

/******************************************************************************/
/*                        S t a t i c   O b j e c t s                         */
/******************************************************************************/
  
namespace
{

#define __ 0xFF

static const unsigned char hexval[0x100] = {
//  0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
    __,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__, /* 00-0F */
    __,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__, /* 10-1F */
    __,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__, /* 20-2F */
     0, 1, 2, 3, 4, 5, 6, 7, 8, 9,__,__,__,__,__,__, /* 30-3F */
    __,10,11,12,13,14,15,__,__,__,__,__,__,__,__,__, /* 40-4F */
    __,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__, /* 50-5F */
    __,10,11,12,13,14,15,__,__,__,__,__,__,__,__,__, /* 60-6F */
    __,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__, /* 70-7F */
    __,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__, /* 80-8F */
    __,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__, /* 90-9F */
    __,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__, /* A0-AF */
    __,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__, /* B0-BF */
    __,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__, /* C0-CF */
    __,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__, /* D0-DF */
    __,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__, /* E0-EF */
    __,__,__,__,__,__,__,__,__,__,__,__,__,__,__,__  /* F0-FF */
};

#undef __

#define ____ "\0\0"

static const unsigned char uri_encode_tbl[ (2 * 0x100) + 1 ] = {
/*  0    1    2    3    4    5    6    7    8    9    a    b    c    d    e    f                     */
    "00" "01" "02" "03" "04" "05" "06" "07" "08" "09" "0A" "0B" "0C" "0D" "0E" "0F"  /* 0:   0 ~  15 */
    "10" "11" "12" "13" "14" "15" "16" "17" "18" "19" "1A" "1B" "1C" "1D" "1E" "1F"  /* 1:  16 ~  31 */
    "20" "21" "22" "23" "24" "25" "26" "27" "28" "29" "2A" "2B" "2C" ____ ____ "2F"  /* 2:  32 ~  47 */
    ____ ____ ____ ____ ____ ____ ____ ____ ____ ____ "3A" "3B" "3C" "3D" "3E" "3F"  /* 3:  48 ~  63 */
    "40" ____ ____ ____ ____ ____ ____ ____ ____ ____ ____ ____ ____ ____ ____ ____  /* 4:  64 ~  79 */
    ____ ____ ____ ____ ____ ____ ____ ____ ____ ____ ____ "5B" "5C" "5D" "5E" ____  /* 5:  80 ~  95 */
    "60" ____ ____ ____ ____ ____ ____ ____ ____ ____ ____ ____ ____ ____ ____ ____  /* 6:  96 ~ 111 */
    ____ ____ ____ ____ ____ ____ ____ ____ ____ ____ ____ "7B" "7C" "7D" ____ "7F"  /* 7: 112 ~ 127 */
    "80" "81" "82" "83" "84" "85" "86" "87" "88" "89" "8A" "8B" "8C" "8D" "8E" "8F"  /* 8: 128 ~ 143 */
    "90" "91" "92" "93" "94" "95" "96" "97" "98" "99" "9A" "9B" "9C" "9D" "9E" "9F"  /* 9: 144 ~ 159 */
    "A0" "A1" "A2" "A3" "A4" "A5" "A6" "A7" "A8" "A9" "AA" "AB" "AC" "AD" "AE" "AF"  /* A: 160 ~ 175 */
    "B0" "B1" "B2" "B3" "B4" "B5" "B6" "B7" "B8" "B9" "BA" "BB" "BC" "BD" "BE" "BF"  /* B: 176 ~ 191 */
    "C0" "C1" "C2" "C3" "C4" "C5" "C6" "C7" "C8" "C9" "CA" "CB" "CC" "CD" "CE" "CF"  /* C: 192 ~ 207 */
    "D0" "D1" "D2" "D3" "D4" "D5" "D6" "D7" "D8" "D9" "DA" "DB" "DC" "DD" "DE" "DF"  /* D: 208 ~ 223 */
    "E0" "E1" "E2" "E3" "E4" "E5" "E6" "E7" "E8" "E9" "EA" "EB" "EC" "ED" "EE" "EF"  /* E: 224 ~ 239 */
    "F0" "F1" "F2" "F3" "F4" "F5" "F6" "F7" "F8" "F9" "FA" "FB" "FC" "FD" "FE" "FF"  /* F: 240 ~ 255 */
};
#undef ____
}

/******************************************************************************/
/*                                D e c o d e                                 */
/******************************************************************************/
  

int XrdOucUri::Decode (const char *src, int len, char *dst)
{
  int i = 0, j = 0;
  unsigned char v1, v2;

// Run through looking for any sequences that need to be converted.
//
  while(i < len)
       {if(src[i] == '%' && i + 2 < len)
          {v1 = hexval[ (unsigned char)src[i+1] ];
           v2 = hexval[ (unsigned char)src[i+2] ];

          /* skip invalid hex sequences */
          if ((v1 | v2) == 0xFF) dst[j++] = src[i++];
             else {dst[j++] = (v1 << 4) | v2;
                   i += 3;
                  }
          } else dst[j++] = src[i++];
       }

// All done, add null byte and return its index
//
  dst[j] = '\0';
  return j;
}

/******************************************************************************/
/*                                E n c o d e                                 */
/******************************************************************************/

int XrdOucUri::Encode(const char *src, int len, char **dst)
{

// Get the size of the destination buffer
//
   int n = Encoded(src, len);

// Allocate an output buffer
//
   if (!(*dst = (char *)malloc(n))) return 0;

// Return final result
//
   return Encode(src, len, *dst);
}

/******************************************************************************/

int XrdOucUri::Encode(const char *src, int len, char *dst)
{
  const unsigned char *code;
  int   j = 0;

// Encode every character that needs to be encoded
//
   for(int i = 0; i < len; i++)
      {code = &uri_encode_tbl[((unsigned int)((unsigned char)src[i])) << 1];
       if (*code)
          {dst[j++] = '%';
           memcpy(&dst[j], code, 2);
           j += 2;
          } else dst[j++] = src[i];
      }

// All done, end with null byte and return index to that byte
//
  dst[j] = '\0';
  return j;
}

/******************************************************************************/
/*                               E n c o d e d                                */
/******************************************************************************/
  
int XrdOucUri::Encoded(const char *src, int len)
{
  int totlen = 0;

// Calculate the size that the destination buffer must have
//
   for(int i = 0; i < len; i++)
      {totlen +=  (uri_encode_tbl[((unsigned int)src[i]) << 1] ? 3 : 1);}

// Return size needed for destination buffer
//
  return totlen + 1;
}
