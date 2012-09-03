#ifndef __FRMTSORT__
#define __FRMTSORT__
/******************************************************************************/
/*                                                                            */
/*                        X r d F r m T S o r t . h h                         */
/*                                                                            */
/* (c) 2009 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

class XrdFrmFileset;

class XrdFrmTSort
{
public:

int               Add(XrdFrmFileset *fsp);

int               Count() {return numEnt;}

XrdFrmFileset    *Oldest();

void              Purge();

                  XrdFrmTSort(int szSort=0) : sortSZ(szSort) {Reset();}
                 ~XrdFrmTSort() {Purge();}

private:
int               Bin(XrdFrmFileset *fsp, int j, int Shift);
XrdFrmFileset    *Insert(XrdFrmFileset *newP, XrdFrmFileset *oldP);
void              Reset();

static const int  SCshift =  0;
static const int  MNshift =  6;
static const int  HRshift = 12;
static const int  tMask = 0x3f;
static const int  dVal  = 24*60*60;

XrdFrmFileset    *FSTab[4][64];
time_t            baseT;
int               sortSZ;
int               numEnt;

int               DYent;   // [0,DYent]
int               HRent;   // [1,HRent]
int               MNent;   // [2,MNent]
int               SCent;   // [3,SCent]
};
#endif
