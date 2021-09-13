#ifndef __XRDSSIBVEC_HH__
#define __XRDSSIBVEC_HH__
/******************************************************************************/
/*                                                                            */
/*                         X r d S s i B V e c . h h                          */
/*                                                                            */
/* (c) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <set>
#include <cstdint>
  
class XrdSsiBVec
{
public:

inline void  Set(uint32_t bval) 
                {if (bval < 64) bitVec |= 1LL << bval;
                    else theSet.insert(bval);
                }

inline bool  IsSet(uint32_t bval)
                  {if (bval < 64) return bitVec & 1LL << bval;
                   std::set<uint32_t>::iterator it = theSet.find(bval);
                   return it != theSet.end();
                  }

inline void  UnSet(uint32_t bval)
                  {if (bval < 64) bitVec &= ~(1LL<<bval);
                      else theSet.erase(bval);
                  }

inline void  Reset() {bitVec = 0; theSet.clear();}

             XrdSsiBVec() : bitVec(0) {}
            ~XrdSsiBVec() {}

private:

uint64_t           bitVec;
std::set<uint32_t> theSet;
};
#endif
