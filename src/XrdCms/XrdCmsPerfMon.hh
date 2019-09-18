#ifndef __CMS_PERFMON__
#define __CMS_PERFMON__
/******************************************************************************/
/*                                                                            */
/*                      X r d C m s P e r f M o n . h h                       */
/*                                                                            */
/* (c) 2019 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

class  XrdOucEnv;
class  XrdSysLogger;

/******************************************************************************/
/*                   c l a s s   X r d C m s P e r f M o n                    */
/******************************************************************************/

/* The XrdCmsPerfMon is used to report performance metrics. It is obtained
   from the shared library loaded at run time and identified by the cms.perf
   directive. The library should contain an implementation of this class.
*/
  
class XrdCmsPerfMon
{
public:

//------------------------------------------------------------------------------
//! Configure the PerfMon plugin object. This is called after the plugin
//! is loaded via the shared library.
//!
//! @param  cfn     The configuration file name.
//! @param  Parms   Any parameters specified in the perf directive. If none,
//!                 the pointer may be null.
//! @param  Logger  The logging object.
//! @param  cmsMon  The object to be used for async reporting.
//! @param  EnvInfo Environmental information of the caller, may be nil.
//! @param  isCMS   True if loaded by the cmsd and false if loaded by xrootd.
//!
//! @return True  upon success.
//!         False upon failure.
//------------------------------------------------------------------------------

virtual bool   Configure(const char    *cfn,
                               char    *Parms,
                         XrdSysLogger  &Logger,
                         XrdCmsPerfMon &cmsMon,
                         XrdOucEnv     *EnvInfo,
                         bool           isCMS)
                        {(void)cfn; (void)Parms; (void)Logger; (void)cmsMon;
                         (void)EnvInfo; (void)isCMS;
                         return false;
                        }

//------------------------------------------------------------------------------
//! Structure used for reporting performance metrics.
//------------------------------------------------------------------------------

struct PerfInfo
      {unsigned char cpu_load; //!< CPU       0 to 100 utilization
       unsigned char mem_load; //!< Memory    0 to 100 utilization
       unsigned char net_load; //!< Network   0 to 100 utilization
       unsigned char pag_load; //!< Paging    0 to 100 utilization
       unsigned char xeq_load; //!< Other     0 to 100 utilization (arbitrary)
       unsigned char xxx_load; //!< Reserved
       unsigned char yyy_load; //!< Reserved
       unsigned char zzz_load; //!< Reserved

       void Clear() {cpu_load = mem_load = net_load = pag_load = xeq_load = 0;
                     xxx_load = yyy_load = zzz_load = 0;
                    }

       PerfInfo() {Clear();}
      ~PerfInfo() {}
      };

//------------------------------------------------------------------------------
//! Obtain performance statistics as load values from 0 to 100. The system
//! calls this method at periodic intervals.
//!
//! @param  info  Reference to the structure that should be filled out with
//!               load values, as desired. See the PerfInfo structure.
//------------------------------------------------------------------------------

virtual void   GetInfo(PerfInfo &info) {(void)info;}

//------------------------------------------------------------------------------
//! Report performance statistics as load values from 0 to 100. The performance
//! monitor plugin may call this method to asynchronously report performance
//! via the passed XrdCmsPerfMon object during configuration.
//!
//! @param  info  Reference to the structure that should be filled out with
//!               load values. See the PerfInfo structure.
//! @param  alert When true, load information is forcibly sent to the cluster's
//!               manager. Otherwise, it is only sent if it significantly
//!               changes. See the cms.sched directive fuzz parameter.
//------------------------------------------------------------------------------

virtual void   PutInfo(PerfInfo &info, bool alert=false)
                      {(void)info; (void)alert;}

//------------------------------------------------------------------------------
//! Constructor & Destructor
//------------------------------------------------------------------------------

               XrdCmsPerfMon() {}

virtual       ~XrdCmsPerfMon() {}
};

/******************************************************************************/
/*       L i b r a r y   X r d C m s P e r f M o n   D e f i n i i o n        */
/******************************************************************************/

//------------------------------------------------------------------------------
//! Your implementation should inherit XrdCmsPerfMon and override the
//! Configure() and GetInfo() methods. Your PutInfo() method will never be
//! called. Then you should set the upcasted address of your implementation at
//! file level as follows (note that use of new here is merely as example):
//!
//! XrdCmsPerfMon *XrdCmsPerfMonitor = new myPerfMon();
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//! Declare compilation version.
//!
//! Additionally, you *should* declare the xrootd version you used to compile
//! your plug-in.
//------------------------------------------------------------------------------

/*! #include "XrdVersion.hh"
    XrdVERSIONINFO(XrdCmsPerfMonitor,<name>);

*/
#endif
