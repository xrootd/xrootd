#ifndef __XRDCMSCLUSTID_HH__
#define __XRDCMSCLUSTID_HH__
/******************************************************************************/
/*                                                                            */
/*                      X r d C m s C l u s t I D . h h                       */
/*                                                                            */
/* (c) 2014 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*                DE-AC02-76-SFO0515 with the Deprtment of Energy             */
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

#include <stdlib.h>
#include <string.h>

class XrdLink;
class XrdCmsNode;

#include "XrdCms/XrdCmsTypes.hh"

class XrdCmsClustID
{
public:

static XrdCmsClustID *AddID(const char *cID);

       bool           AddNode(XrdCmsNode *nP, bool isMan);

inline bool           Avail()    {return npNum < altMax;}

       bool           Exists(XrdLink *lp, const char *nid, int port);

static XrdCmsClustID *Find(const char *cID);

static SMask_t        Mask(const char *cID);

inline bool           IsEmpty()  {return npNum < 1;}

inline bool           IsSingle() {return npNum == 1;}

       XrdCmsNode    *RemNode(XrdCmsNode *nP);

inline int            Slot()     {return ntSlot;}

       XrdCmsClustID() : cidMask(0), cidName(0), ntSlot(-1), npNum(0)
                         {memset(nodeP, 0, sizeof(nodeP));}

      ~XrdCmsClustID() {if (cidName) free(cidName);}

private:

static const int   altMax = 8;

       SMask_t     cidMask;
       char       *cidName;
       int         ntSlot;
       int         npNum;
       XrdCmsNode *nodeP[altMax];
};
#endif
