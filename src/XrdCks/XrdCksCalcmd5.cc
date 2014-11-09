/******************************************************************************/
/*                                                                            */
/*                      X r d C k s C a l c m d 5 . c c                       */
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

#include "XrdCks/XrdCksCalcmd5.hh"
#include "XrdSys/XrdSysPlatform.hh"
  
/*
 * This code implements the MD5 message-digest algorithm.
 * The algorithm is due to Ron Rivest.  This code was
 * written by Colin Plumb in 1993, no copyright is claimed.
 * This code is in the public domain; do with it what you wish.
 *
 * Equivalent code is available from RSA Data Security, Inc.
 * This code has been tested against that, and is equivalent,
 * except that you don't need to include two pages of legalese
 * with every copy.
 *
 * To compute the message digest of a chunk of bytes, declare an
 * MD5Context structure, pass it to MD5Init, call MD5Update as
 * needed on buffers full of bytes, and then call MD5Final, which
 * will fill a supplied 16-byte array with the digest.
 */

/******************************************************************************/
/*                           B y t e R e v e r s e                            */
/******************************************************************************/

#ifndef Xrd_Big_Endian
void XrdCksCalcmd5::byteReverse(unsigned char *buf, unsigned longs) {} /* Nothing */
#else
#ifndef ASM_MD5
void XrdCksCalcmd5::byteReverse(unsigned char *buf, unsigned longs)
{
   unsigned int t;
   do {t = (unsigned int) ((unsigned) buf[3] << 8 | buf[2]) << 16 |
                          ((unsigned) buf[1] << 8 | buf[0]);
       *(unsigned int *) buf = t;
       buf += 4;
      } while (--longs);
}
#endif
#endif

/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/
  
/******************************************************************************/
/*                               M D 5 I n i t                                */
/******************************************************************************/
  
/* Start MD5 accumulation.  Set bit count to 0 and buffer to mysterious
   initialization constants.
*/
void XrdCksCalcmd5::Init()
{
    myContext.buf[0]  = 0x67452301;
    myContext.buf[1]  = 0xefcdab89;
    myContext.buf[2]  = 0x98badcfe;
    myContext.buf[3]  = 0x10325476;

    myContext.bits[0] = 0;
    myContext.bits[1] = 0;
}

/******************************************************************************/
/*                             M D 5 U p d a t e                              */
/******************************************************************************/
  
/* Update context to reflect the concatenation of another buffer full of bytes.
*/
void XrdCksCalcmd5::MD5Update(unsigned char const *buf, unsigned int len)
{
   unsigned int t;

// Update bitcount
//
   t = myContext.bits[0];
   if ((myContext.bits[0] = t + ((unsigned int) len << 3)) < t)

// Carry from low to high
//
   myContext.bits[1]++;
   myContext.bits[1] += len >> 29;

// Bytes already in shsInfo->data
//
   t = (t >> 3) & 0x3f;

// Handle any leading odd-sized chunks
//
   if (t) {unsigned char *p = (unsigned char *) myContext.in + t;
           t = 64 - t;
           if (len < t) {memcpy(p, buf, len); return;}
           memcpy(p, buf, t);
           byteReverse(myContext.in, 16);
           MD5Transform(myContext.buf, (unsigned int *) myContext.in);
           buf += t;
           len -= t;
          }

// Process data in 64-byte chunks
//
   while(len >= 64)
        {memcpy(myContext.in, buf, 64);
         byteReverse(myContext.in, 16);
         MD5Transform(myContext.buf, (unsigned int *) myContext.in);
         buf += 64;
         len -= 64;
        }

// Handle any remaining bytes of data.

   memcpy(myContext.in, buf, len);
}

/******************************************************************************/
/*                                 F i n a l                                  */
/******************************************************************************/
  
/* Final wrapup - pad to 64-byte boundary with the bit pattern
   1 0* (64-bit count of bits processed, MSB-first)
*/
char *XrdCksCalcmd5::Final()
{
   unsigned count;
   unsigned char *p;

// Compute number of bytes mod 64
//
   count = (myContext.bits[0] >> 3) & 0x3F;

// Set the first char of padding to 0x80.  This is safe since there is
// always at least one byte free.
//
   p = myContext.in + count;
   *p++ = 0x80;

// Bytes of padding needed to make 64 bytes
//
   count = 64 - 1 - count;

// Pad out to 56 mod 64
//
   if (count < 8)  // Two lots of padding:  Pad the first block to 64 bytes
      {memset(p, 0, count);
       byteReverse(myContext.in, 16);
       MD5Transform(myContext.buf, (unsigned int *) myContext.in);
       memset(myContext.in, 0, 56);        // Now fill the next block with 56 bytes
      } else memset(p, 0, count - 8); // Else pad block to 56 bytes

   byteReverse(myContext.in, 14);

// Append length in bits and transform (original code in comments)
//
// ((unsigned int *) myContext.in)[14] = myContext.bits[0];
// ((unsigned int *) myContext.in)[15] = myContext.bits[1];
   myContext.i64[7] = myContext.b64;

   MD5Transform(myContext.buf, (unsigned int *) myContext.in);
   byteReverse((unsigned char *) myContext.buf, 4);

// Copy to a separate buffer and return ASCII value if so wanted
//
   memcpy(myDigest, myContext.buf, 16);
   return (char *)myDigest;
}

/******************************************************************************/
/*                          M D 5 T r a n s f o r m                           */
/******************************************************************************/
  
#ifndef ASM_MD5

/* The four core functions - F1 is optimized somewhat */

// #define F1(x, y, z) (x & y | ~x & z)
//
#define F1(x, y, z) (z ^ (x & (y ^ z)))
#define F2(x, y, z) F1(z, x, y)
#define F3(x, y, z) (x ^ y ^ z)
#define F4(x, y, z) (y ^ (x | ~z))

// This is the central step in the MD5 algorithm.
//
#define MD5STEP(f, w, x, y, z, data, s) \
               ( w += f(x, y, z) + data,  w = w<<s | w>>(32-s),  w += x )

/* The core of the MD5 algorithm, this alters an existing MD5 hash to
   reflect the addition of 16 longwords of new data.  MD5Update blocks
   the data and converts bytes into longwords for this routine.
*/
void XrdCksCalcmd5::MD5Transform(unsigned int buf[4], unsigned int const in[16])
{
    unsigned int a, b, c, d;

    a = buf[0];
    b = buf[1];
    c = buf[2];
    d = buf[3];

    MD5STEP(F1, a, b, c, d, in[0] + 0xd76aa478, 7);
    MD5STEP(F1, d, a, b, c, in[1] + 0xe8c7b756, 12);
    MD5STEP(F1, c, d, a, b, in[2] + 0x242070db, 17);
    MD5STEP(F1, b, c, d, a, in[3] + 0xc1bdceee, 22);
    MD5STEP(F1, a, b, c, d, in[4] + 0xf57c0faf, 7);
    MD5STEP(F1, d, a, b, c, in[5] + 0x4787c62a, 12);
    MD5STEP(F1, c, d, a, b, in[6] + 0xa8304613, 17);
    MD5STEP(F1, b, c, d, a, in[7] + 0xfd469501, 22);
    MD5STEP(F1, a, b, c, d, in[8] + 0x698098d8, 7);
    MD5STEP(F1, d, a, b, c, in[9] + 0x8b44f7af, 12);
    MD5STEP(F1, c, d, a, b, in[10] + 0xffff5bb1, 17);
    MD5STEP(F1, b, c, d, a, in[11] + 0x895cd7be, 22);
    MD5STEP(F1, a, b, c, d, in[12] + 0x6b901122, 7);
    MD5STEP(F1, d, a, b, c, in[13] + 0xfd987193, 12);
    MD5STEP(F1, c, d, a, b, in[14] + 0xa679438e, 17);
    MD5STEP(F1, b, c, d, a, in[15] + 0x49b40821, 22);

    MD5STEP(F2, a, b, c, d, in[1] + 0xf61e2562, 5);
    MD5STEP(F2, d, a, b, c, in[6] + 0xc040b340, 9);
    MD5STEP(F2, c, d, a, b, in[11] + 0x265e5a51, 14);
    MD5STEP(F2, b, c, d, a, in[0] + 0xe9b6c7aa, 20);
    MD5STEP(F2, a, b, c, d, in[5] + 0xd62f105d, 5);
    MD5STEP(F2, d, a, b, c, in[10] + 0x02441453, 9);
    MD5STEP(F2, c, d, a, b, in[15] + 0xd8a1e681, 14);
    MD5STEP(F2, b, c, d, a, in[4] + 0xe7d3fbc8, 20);
    MD5STEP(F2, a, b, c, d, in[9] + 0x21e1cde6, 5);
    MD5STEP(F2, d, a, b, c, in[14] + 0xc33707d6, 9);
    MD5STEP(F2, c, d, a, b, in[3] + 0xf4d50d87, 14);
    MD5STEP(F2, b, c, d, a, in[8] + 0x455a14ed, 20);
    MD5STEP(F2, a, b, c, d, in[13] + 0xa9e3e905, 5);
    MD5STEP(F2, d, a, b, c, in[2] + 0xfcefa3f8, 9);
    MD5STEP(F2, c, d, a, b, in[7] + 0x676f02d9, 14);
    MD5STEP(F2, b, c, d, a, in[12] + 0x8d2a4c8a, 20);

    MD5STEP(F3, a, b, c, d, in[5] + 0xfffa3942, 4);
    MD5STEP(F3, d, a, b, c, in[8] + 0x8771f681, 11);
    MD5STEP(F3, c, d, a, b, in[11] + 0x6d9d6122, 16);
    MD5STEP(F3, b, c, d, a, in[14] + 0xfde5380c, 23);
    MD5STEP(F3, a, b, c, d, in[1] + 0xa4beea44, 4);
    MD5STEP(F3, d, a, b, c, in[4] + 0x4bdecfa9, 11);
    MD5STEP(F3, c, d, a, b, in[7] + 0xf6bb4b60, 16);
    MD5STEP(F3, b, c, d, a, in[10] + 0xbebfbc70, 23);
    MD5STEP(F3, a, b, c, d, in[13] + 0x289b7ec6, 4);
    MD5STEP(F3, d, a, b, c, in[0] + 0xeaa127fa, 11);
    MD5STEP(F3, c, d, a, b, in[3] + 0xd4ef3085, 16);
    MD5STEP(F3, b, c, d, a, in[6] + 0x04881d05, 23);
    MD5STEP(F3, a, b, c, d, in[9] + 0xd9d4d039, 4);
    MD5STEP(F3, d, a, b, c, in[12] + 0xe6db99e5, 11);
    MD5STEP(F3, c, d, a, b, in[15] + 0x1fa27cf8, 16);
    MD5STEP(F3, b, c, d, a, in[2] + 0xc4ac5665, 23);

    MD5STEP(F4, a, b, c, d, in[0] + 0xf4292244, 6);
    MD5STEP(F4, d, a, b, c, in[7] + 0x432aff97, 10);
    MD5STEP(F4, c, d, a, b, in[14] + 0xab9423a7, 15);
    MD5STEP(F4, b, c, d, a, in[5] + 0xfc93a039, 21);
    MD5STEP(F4, a, b, c, d, in[12] + 0x655b59c3, 6);
    MD5STEP(F4, d, a, b, c, in[3] + 0x8f0ccc92, 10);
    MD5STEP(F4, c, d, a, b, in[10] + 0xffeff47d, 15);
    MD5STEP(F4, b, c, d, a, in[1] + 0x85845dd1, 21);
    MD5STEP(F4, a, b, c, d, in[8] + 0x6fa87e4f, 6);
    MD5STEP(F4, d, a, b, c, in[15] + 0xfe2ce6e0, 10);
    MD5STEP(F4, c, d, a, b, in[6] + 0xa3014314, 15);
    MD5STEP(F4, b, c, d, a, in[13] + 0x4e0811a1, 21);
    MD5STEP(F4, a, b, c, d, in[4] + 0xf7537e82, 6);
    MD5STEP(F4, d, a, b, c, in[11] + 0xbd3af235, 10);
    MD5STEP(F4, c, d, a, b, in[2] + 0x2ad7d2bb, 15);
    MD5STEP(F4, b, c, d, a, in[9] + 0xeb86d391, 21);

    buf[0] += a;
    buf[1] += b;
    buf[2] += c;
    buf[3] += d;
}
#endif
