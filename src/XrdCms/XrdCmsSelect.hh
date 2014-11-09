#ifndef __CMS_SELECT_HH
#define __CMS_SELECT_HH
/******************************************************************************/
/*                                                                            */
/*                       X r d C m s S e l e c t . h h                        */
/*                                                                            */
/* (c) 2007 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <netinet/in.h>

#include "XrdCms/XrdCmsKey.hh"

/******************************************************************************/
/*                    C l a s s   X r d C m s S e l e c t                     */
/******************************************************************************/

class XrdCmsRRQInfo;
  
class XrdCmsSelect
{
public:
XrdCmsKey      Path;    //  In: Path to select or lookup in the cache
XrdCmsRRQInfo *InfoP;   //  In: Fast redirect routing
SMask_t        nmask;   //  In: Nodes to avoid
SMask_t        smask;   // Out: Nodes selected
struct iovec  *iovP;    //  In: Prepare notification I/O vector
int            iovN;    //  In: Prepare notification I/O vector count
int            Opts;    //  In: One or more of the following enums

enum {Write   = 0x00010, // File will be open in write mode     (select & cache)
      NewFile = 0x00020, // File will be created may not exist  (select)
      Online  = 0x00040, // Only consider online files          (select & prep)
      Trunc   = 0x00080, // File will be truncated              (Select   only)
      Create  = 0x000A0, // Create file, truncate if exists
      Defer   = 0x00100, // Do not select a server now          (prep     only)
      Peers   = 0x00200, // Peer clusters may be selected       (select   only)
      Refresh = 0x00400, // Cache should be refreshed           (all)
      Asap    = 0x00800, // Respond as soon as possible         (locate   only)
      noBind  = 0x01000, // Do not new bind file to a server    (select   only)
      isMeta  = 0x02000, // Only inode information being changed(select   only)
      Freshen = 0x04000, // Freshen access times                (prep     only)
      Replica = 0x08000, // File will be replicated (w/ Create) (select   only)
      Advisory= 0x40000, // Cache A/D is advisory (no delay)    (have   & cache)
      Pending = 0x80000, // File being staged                   (have   & cache)
      ifWant  = 0x0000f  // XrdNetIF::ifType encoding location
     };

struct {SMask_t wf;     // Out: Writable locations
        SMask_t hf;     // Out: Existing locations
        SMask_t pf;     // Out: Pending  locations
        SMask_t bf;     // Out: Bounced  locations
       }        Vec;

struct {int  Port;      // Out: Target node port number
        char Data[256]; // Out: Target node or error message
        int  DLen;      // Out: Length of Data including null byte
       }     Resp;

             XrdCmsSelect(int opts=0, char *thePath=0, int thePLen=0)
                         : Path(thePath,thePLen), InfoP(0), smask(0), Opts(opts)
                         {Resp.Port = 0; *Resp.Data = '\0'; Resp.DLen = 0;}
            ~XrdCmsSelect() {}
};

/******************************************************************************/
/*                  C l a s s   X r d C m s S e l e c t e d                   */
/******************************************************************************/
  
class XrdCmsSelected   // Argument to List() after select or locate
{
public:

static const int IdentSize = 264;

XrdCmsSelected *next;
SMask_t         Mask;
int             Id;
int             IdentLen;                  // 12345678901234567890123456
char            Ident[IdentSize];          // [::123.123.123.123]:123456
int             Port;
int             RefTotW;
int             RefTotR;
int             Shrin;       // Share intervals used
char            Share;       // Share
char            RoleID;      // Role Identifier
char            Rsvd[2];
int             Status;      // One of the following

enum           {Disable = 0x0001,
                NoStage = 0x0002,
                Offline = 0x0004,
                Suspend = 0x0008,
                isRW    = 0x0010,
                isMangr = 0x0100
               };

               XrdCmsSelected(XrdCmsSelected *np=0) : next(np) {}

              ~XrdCmsSelected() {}
};

/******************************************************************************/
/*                  C l a s s   X r d C m s S e l e c t o r                   */
/******************************************************************************/
  
class XrdCmsSelector
{
public:
const  char *reason;
       int   delay;
       short nPick;
       char  needNet;
       char  needSpace;
       bool  xFull;
       bool  xNoNet;
       bool  xOff;
       bool  xOvld;
       bool  xSusp;

inline void  Reset() {reason = 0; delay = 0; nPick = 0;
                      xFull = xNoNet = xOff = xOvld = xSusp = false;
                     }
};
#endif
