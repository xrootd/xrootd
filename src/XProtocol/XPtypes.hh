#ifndef __XPTYPES_H
#define __XPTYPES_H
/******************************************************************************/
/*                                                                            */
/*                            X P t y p e s . h h                             */
/*                                                                            */
/* (c) 2012 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

// Full range type compatibility work done by Gerardo Ganis, CERN.

// Typical data types
//
// Only char and short are truly portable types
typedef unsigned char  kXR_char;
typedef short          kXR_int16;
typedef unsigned short kXR_unt16;

// Signed integer 4 bytes
//
#ifndef XR__INT16
#   if defined(LP32) || defined(__LP32) || defined(__LP32__) || \
       defined(BORLAND)
#      define XR__INT16
#   endif
#endif
#ifndef XR__INT64
#   if defined(ILP64) || defined(__ILP64) || defined(__ILP64__)
#      define XR__INT64
#   endif
#endif
#if defined(XR__INT16)
typedef long           kXR_int32;
typedef unsigned long  kXR_unt32;
#elif defined(XR__INT64)
typedef int32          kXR_int32;
typedef unsigned int32 kXR_unt32;
#else
typedef int            kXR_int32;
typedef unsigned int   kXR_unt32;
#endif

// Signed integer 8 bytes
//
//#if defined(_WIN32)
//typedef __int64        kXR_int64;
//#else
typedef long long          kXR_int64;
typedef unsigned long long kXR_unt64;
//#endif
#endif
