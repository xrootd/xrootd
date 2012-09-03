#ifndef __OSS_OPAQUE_H__
#define __OSS_OPAQUE_H__
/******************************************************************************/
/*                                                                            */
/*                       X r d O s s O p a q u e . h h                        */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

/******************************************************************************/
/*                E x t e r n a l   C o n f i g u r a t i o n                 */
/******************************************************************************/
  
#define OSS_ASIZE           (char *)"oss.asize"
#define OSS_CGROUP          (char *)"oss.cgroup"
#define OSS_USRPRTY         (char *)"oss.sprty"
#define OSS_SYSPRTY         (char *)"oss&sprty"
#define OSS_CGROUP_DEFAULT  (char *)"public"

#define OSS_VARLEN          32

#define OSS_MAX_PRTY        15
#define OSS_USE_PRTY         7

#endif
