#ifndef __XRDOSS_VS_H__
#define __XRDOSS_VS_H__
/******************************************************************************/
/*                                                                            */
/*                           X r d O s s V S . h h                            */
/*                                                                            */
/* (c) 2020 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
/*                    C l a s s   X r d O s s V S P a r t                     */
/******************************************************************************/
  
//-----------------------------------------------------------------------------
//! Class describing the patitions associated with a space. This is returned
//! as a vector by StatVS() when so requested. Note that pPath points to a path
//! associated with the partition. No inference should be made about this
//! path other than it is one of, perhaps of many, paths associated with the
//! partition. The aPath vector provides specific paths that are associated
//! with allocated files in the partition relative to the space name. There
//! may be many of these for the partition. The vector always ends with a nil
//! pointer.
//!
//! The bdevID is a unique identifier for each block device. It is currently
//! only supported in Linux. Other systems assign all partitions the generic ID
//! of 0. Even in Linux, the ID will be zero for softwared devices (though not
//! for LVM devices backed by a real device). Softwared devices such as
//! distributed file systems cannot be identified correctly as they do not
//! provide sufficiently distinguishing information.
//!
//! The partID is an ascending value that uniquely identifies a partition
//! irrespective of its associated bdevID. However, using this information to
//! schedule read/write requests is not trivial and may overwhelm the
//! underlying device if it has many partitions. You can check if the system
//! correctly identified all devices using DevID(0,x,y). If 'x' is one upon
//! return, then using partID for scheduling is the only recourse as either
//! no real devices were found or the system only has one such device.
//-----------------------------------------------------------------------------

class XrdOssVSPart
{
public:
const char    *pPath;   // Valid path to partition (not the allocation path)
const char   **aPath;   // Allocation root paths for this partition
long long      Total;   // Total bytes
long long      Free;    // Total bytes free
unsigned short bdevID;  // Device    unique ID (bdevs may have many partitions)
unsigned short partID;  // Partition unique ID (parts may have the same bdevID)
int            rsvd1;
void          *rsvd2;   // Reserved

               XrdOssVSPart() : pPath(0),  aPath(0), Total(0), Free(0),
                                bdevID(0), partID(0), rsvd1(0), rsvd2(0)
                              {}
              ~XrdOssVSPart() {}
};

/******************************************************************************/
/*                    C l a s s   X r d O s s V S I n f o                     */
/******************************************************************************/
  
// Class passed to StatVS()
//
class XrdOssVSInfo
{
public:
long long     Total;   // Total bytes
long long     Free;    // Total bytes free
long long     Large;   // Total bytes in largest partition
long long     LFree;   // Max   bytes free in contiguous chunk
long long     Usage;   // Used  bytes (if usage enabled)
long long     Quota;   // Quota bytes (if quota enabled)
int           Extents; // Number of partitions/extents
int           Reserved;
XrdOssVSPart *vsPart;  // Partition info as vsPart[extents] may be nil

//-----------------------------------------------------------------------------
//! Export the partition table to avoid deletion when this object goes
//! out of scope.
//!
//! @param  nParts - Reference to where the number of partitions is stored/
//!
//! @return Pointer to the partion vector. You are responsible for deletion
//!         using "delete [] <prt>".
//-----------------------------------------------------------------------------

XrdOssVSPart *Export(int &nParts)
                    {XrdOssVSPart *pVec = vsPart;
                     vsPart = 0;
                     nParts = Extents;
                     return pVec;
                    }

              XrdOssVSInfo() : Total(0),Free(0),Large(0),LFree(0),Usage(-1),
                               Quota(-1),Extents(0),Reserved(0), vsPart(0) {}
             ~XrdOssVSInfo() {if (vsPart) delete [] vsPart;}
};
#endif
