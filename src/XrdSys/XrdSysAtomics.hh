#ifndef _XRDSYSATOMICS_
#define _XRDSYSATOMICS_
/******************************************************************************/
/*                                                                            */
/*                      X r d S y s A t o m i c s . h h                       */
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

/* The following instruction acronyms are used:
   AtomicCAS() -> Compare And [if equal] Set
   AtomicFAZ() -> Fetch And Zero
*/

/* Portability note: When using fetch-add or fetch-sub in the context of an
                     assignment statement, you must use the the AtomicFAdd
                     and AtomicFSub macros to ensure portability and correct
                     results. Additionally, whenever you use AtomicFAZ as the
                     only consequent of an if-else statement you *must* places
                     braces around it otherwise the code will not be portable!
                     Alternatively, always use the AtomicFZAP macro.
*/
  
#ifdef HAVE_ATOMICS
#define AtomicBeg(Mtx)
#define AtomicEnd(Mtx)
#define AtomicAdd(x, y)     __sync_fetch_and_add(&x, y)
#define AtomicFAdd(w,x,y)   w =  __sync_fetch_and_add(&x, y)
#define AtomicCAS(x, y, z)  __sync_bool_compare_and_swap(&x, y, z)
#define AtomicDec(x)        __sync_fetch_and_sub(&x, 1)
#define AtomicFAZ(x)        __sync_fetch_and_and(&x, 0)
#define AtomicFZAP(w,x)     w =  __sync_fetch_and_and(&x, 0)
#define AtomicGet(x)        __sync_fetch_and_or(&x, 0)
#define AtomicInc(x)        __sync_fetch_and_add(&x, 1)
#define AtomicSub(x, y)     __sync_fetch_and_sub(&x, y)
#define AtomicFSub(w,x,y)   w =  __sync_fetch_and_sub(&x, y)
#define AtomicZAP(x)        __sync_fetch_and_and(&x, 0)
#else
#define AtomicBeg(Mtx)      Mtx.Lock()
#define AtomicEnd(Mtx)      Mtx.UnLock()
#define AtomicAdd(x, y)     x += y          // When assigning use AtomicFAdd!
#define AtomicFAdd(w,x,y)  {w = x; x += y;}
#define AtomicCAS(x, y, z)  if (x == y) x = z
#define AtomicDec(x)        x--
#define AtomicFAZ(x)        x; x = 0        // Braces when used with if-else!
#define AtomicFZAP(w,x)    {w = x; x = 0;}
#define AtomicGet(x)        x
#define AtomicInc(x)        x++
#define AtomicSub(x, y)     x -= y          // When assigning use AtomicFSub!
#define AtomicFSub(w,x,y)  {w = x; x -= y;}
#define AtomicZAP(x)        x = 0
#endif
#endif

/*
 * The following definitions give a mechanism for using C++ atomics
 * (when using at least C++11).  *Note* that these can't be relied
 * on for correct behavior as they are non-atomic for C++03 compilers.
 *
 * Only use them for standards correctness (eliminating C++11 undefined
 * behavior).
 */
#if __cplusplus >= 201103L
#include <atomic>
#define CPP_ATOMIC_LOAD(x, order)       x.load(order)
#define CPP_ATOMIC_STORE(x, val, order) x.store(val, order)
#define CPP_ATOMIC_TYPE(kind)           std::atomic<kind>
#else
#define CPP_ATOMIC_LOAD(x, order)       x
#define CPP_ATOMIC_STORE(x, val, order) x = val
#define CPP_ATOMIC_TYPE(kind)           kind
#endif


