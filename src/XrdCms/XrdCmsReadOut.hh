#ifndef XRDCMSREADOUT__H
#define XRDCMSREADOUT__H
/******************************************************************************/
/*                                                                            */
/*                      X r d C m s R e a d O u t . h h                       */
/*                                                                            */
/* (c) 2025 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

class XrdCmsReadOut
{
public:

size_t Next()
           {size_t nowBit;
            if ((nowBit = pendBit) < maxiBit)
               pendBit = theVec._Find_next(nowBit);
            return nowBit;
           }

    XrdCmsReadOut(SMask_t& vec) : theVec(vec),
                                  pendBit(vec._Find_first()),
                                  maxiBit(vec.size()) {}

   ~XrdCmsReadOut() {}

private:
SMask_t& theVec;
size_t   pendBit;
size_t   maxiBit;
};
#endif
