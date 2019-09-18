#ifndef __XRDSSICMS_H__
#define __XRDSSICMS_H__
/******************************************************************************/
/*                                                                            */
/*                          X r d S s i C m s . h h                           */
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

#include "XrdCms/XrdCmsClient.hh"
#include "XrdSsi/XrdSsiCluster.hh"


class XrdSsiCms : public XrdSsiCluster
{
public:

        void   Added(const char *name, bool pend=false);

        bool   DataContext() {return true;}

const char *
const      *   Managers(int &mNum) {mNum = manNum; return manList;}

        void   Removed(const char *name);

        void   Resume (bool perm=true)
                    {if (theCms) theCms->Resume(perm);}

        void   Suspend(bool perm=true)
                    {if (theCms) theCms->Suspend(perm);}

        int    Resource(int n)
                    {if (theCms) return theCms->Resource(n);
                     return 0;
                    }

        int    Reserve (int n=1)
                    {if (theCms) return theCms->Reserve(n);
                     return 0;
                    }

        int    Release (int n=1)
                    {if (theCms) return theCms->Release(n);
                     return 0;
                    }

        void   Utilization(unsigned int util, bool alert=false)
                    {if (theCms) return theCms->Utilization(util, alert);}

               XrdSsiCms() : theCms(0), manList(0), manNum(0) {}

               XrdSsiCms(XrdCmsClient *cmsP);

virtual       ~XrdSsiCms();

private:

XrdCmsClient  *theCms;
char         **manList;
int            manNum;
};
#endif
