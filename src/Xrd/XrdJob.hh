#ifndef ___XRD_JOB_H___
#define ___XRD_JOB_H___
/******************************************************************************/
/*                                                                            */
/*                             X r d J o b . h h                              */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cstdlib>
#include <cstring>
#include <strings.h>
#include <ctime>
  
// The XrdJob class is a super-class that is inherited by any class that needs
// to schedule work on behalf of itself. The XrdJob class is optimized for
// queue processing since that's where it spends a lot of time. This class
// should not be depedent on any other class.

class XrdJob
{
friend class XrdScheduler;
public:
XrdJob    *NextJob;   // -> Next job in the queue (zero if last)
const char *Comment;   // -> Description of work for debugging (static!)

virtual void  DoIt() = 0;

              XrdJob(const char *desc="")
                    {Comment = desc; NextJob = 0; SchedTime = 0;}
virtual      ~XrdJob() {}

private:
time_t      SchedTime; // -> Time job is to be scheduled
};
#endif
