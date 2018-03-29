#ifndef __XRDCnsSsiSay_H_
#define __XRDCnsSsiSay_H_
/******************************************************************************/
/*                                                                            */
/*                          X r d C n s S a y . h h                           */
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

#include "XrdSys/XrdSysError.hh"

class XrdCnsSsiSay
{
public:

inline void M(const char *txt1,   const char *txt2=0, const char *txt3=0,
              const char *txt4=0, const char *txt5=0)
             {eDest->Say("cns_ssi: ", txt1, txt2, txt3, txt4, txt5);}

inline void V(const char *txt1,   const char *txt2=0, const char *txt3=0,
              const char *txt4=0, const char *txt5=0)
             {if (Verbose) M(txt1, txt2, txt3, txt4, txt5);}

inline void setV(int val) {Verbose = val;}

       XrdCnsSsiSay(XrdSysError *erp) : eDest(erp), Verbose(0) {}
      ~XrdCnsSsiSay() {}

private:

XrdSysError *eDest;
int          Verbose;
};
#endif
