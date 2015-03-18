#ifndef __XRDSSIRRTABLE_HH__
#define __XRDSSIRRTABLE_HH__
/******************************************************************************/
/*                                                                            */
/*                      X r d S s i R R T a b l e . h h                       */
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

#include <string.h>
  
template<class T>
class XrdSsiRRTable
{
public:

static const int maxID = 255;

void  Add(T *item, int itemID)
         {T **frV;
          int i = itemID >> sftID, j = itemID & mskID;
          if (!(frV = tVec[i]))
             {frV = tVec[i] = new   T * [vecSZ];
              memset(frV, 0, sizeof(T *)*vecSZ);
              if (i > tVecLast) tVecLast = i;
             }
          frV[j] = item;
          if (tEnd[i] < i) tEnd[i] = j;
         }

void  Del(int itemID)
         {T **tP;
          int i = itemID >> sftID, j = itemID & mskID, k;
          if ((tP = tVec[i]))
             {tP[j] = 0;
              if (tEnd[i] == j) // Record last entry
                 {for (k = j-1; k >= 0; k--) if (tP[k]) break;
                  tEnd[i] = k;
                 }
             }
         }

T    *LookUp(int itemID)
            {T **tP;
             if (itemID < vecSZ) return tPrm[itemID]; // Fast lookup
             if (!(tP = tVec[itemID >> sftID])) return 0;
             return tP[itemID & mskID];
            }

void  Reset()
           {T **frV;
            int i, j;
            for (i = 0; i <= tVecLast; i++)
                {if ((frV = tVec[i]))
                    {for (j = 0; j <= tEnd[i]; j++)
                         {if (frV[j]) {frV[j]->Finalize(); frV[j] = 0;}}
                     tEnd[i] = -1;
                     if (i) {delete [] frV; tVec[i] = 0;}
                    }
                }
            tVecLast =  0;
           }

      XrdSsiRRTable() {memset(tEnd,-1, sizeof(tEnd));
                       memset(tPrm, 0, sizeof(tPrm));
                       memset(tVec, 0, sizeof(tVec));
                       tVec[0]    = tPrm;
                       tVecLast   = 0;
                      }

     ~XrdSsiRRTable() {Reset();}

private:
static const int         vecSZ =  16;
static const int         mskID =  15;
static const int         sftID =   4;

char                     tEnd[vecSZ];
int                      tVecLast;
T                       *tPrm[vecSZ];
T                      **tVec[vecSZ];
};
#endif
