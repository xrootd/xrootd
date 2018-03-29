#ifndef XRD_CLI_READAHEAD
#define XRD_CLI_READAHEAD
/******************************************************************************/
/*                                                                            */
/*               X r d C l i e n t R e a d A h e a d . h h                    */
/*                                                                            */
/* Author: Fabrizio Furano (CERN IT-DM, 2009)                                 */
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

//////////////////////////////////////////////////////////////////////////
//                                                                      //
// Classes to implement a selectable read ahead decision maker          //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

class XrdClientReadAheadMgr {
public:
   enum XrdClient_RAStrategy {
      RAStr_none,
      RAStr_pureseq,
      RAStr_SlidingAvg
   };

protected:
   long RASize;
   XrdClient_RAStrategy currstrategy;

public:
      
   static XrdClientReadAheadMgr *CreateReadAheadMgr(XrdClient_RAStrategy strategy);
   

   XrdClientReadAheadMgr() { RASize = 0; };
   virtual ~XrdClientReadAheadMgr() {};

   virtual int GetReadAheadHint(long long offset, long len, long long &raoffset, long &ralen, long blksize) = 0;
   virtual int Reset() = 0;
   virtual void SetRASize(long bytes) { RASize = bytes; };
   
   static bool TrimReadRequest(long long &offs, long &len, long rasize, long blksize);

   XrdClient_RAStrategy GetCurrentStrategy() { return currstrategy; }
};
#endif
