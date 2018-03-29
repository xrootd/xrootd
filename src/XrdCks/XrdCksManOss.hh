#ifndef __XRDCKSMANOSS_HH__
#define __XRDCKSMANOSS_HH__
/******************************************************************************/
/*                                                                            */
/*                        X r d k s M a n O s s . h h                         */
/*                                                                            */
/* (c) 2014 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "sys/types.h"

#include "XrdCks/XrdCksManager.hh"

/* This class defines the checksum management interface using the oss plugin.
   It is used internally to provide checksums for oss-based storage systems.
*/

class XrdOss;

class XrdCksManOss : public XrdCksManager
{
public:
virtual int         Calc(const char *Lfn, XrdCksData &Cks, int doSet=1);

virtual int         Del( const char *Lfn, XrdCksData &Cks);

virtual int         Get( const char *Lfn, XrdCksData &Cks);

virtual char       *List(const char *Lfn, char *Buff, int Blen, char Sep=' ');

virtual int         Set( const char *Lfn, XrdCksData &Cks, int myTime=0);

virtual int         Ver( const char *Lfn, XrdCksData &Cks);

                    XrdCksManOss(XrdOss *ossX, XrdSysError *erP, int iosz,
                                 XrdVersionInfo &vInfo, bool autoload=false);

virtual            ~XrdCksManOss() {}

protected:
virtual int         Calc(const char *Lfn, time_t &MTime, XrdCksCalc *CksObj);
virtual int         ModTime(const char *Pfn, time_t &MTime);

private:

int buffSZ;
};
#endif
