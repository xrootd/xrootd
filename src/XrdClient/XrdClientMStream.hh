#ifndef XRD_CLI_MSTREAM
#define XRD_CLI_MSTREAM
/******************************************************************************/
/*                                                                            */
/*                 X r d C l i e n t M S t r e a m . h h                      */
/*                                                                            */
/* Author: Fabrizio Furano (INFN Padova, 2006)                                */
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
// Helper code for XrdClient to handle multistream behavior             //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#include "XrdClient/XrdClientConn.hh"

class XrdClientMStream {
public:
  
    // Compute the parameters to split blocks
    static void GetGoodSplitParameters(XrdClientConn *cliconn,
			      int &spltsize, int &reqsperstream,
			      kXR_int32 len);

    // Establish all the parallel streams, stop
    // adding streams at the first creation refusal/failure
    static int EstablishParallelStreams(XrdClientConn *cliconn);

    // Add a parallel stream to the pool used by the given client inst
   static int AddParallelStream(XrdClientConn *cliconn, int port, int windowsz, int tempid);

    // Remove a parallel stream to the pool used by the given client inst
    static int RemoveParallelStream(XrdClientConn *cliconn, int substream);

    // Binds the pending temporary parallel stream to the current session
    // Returns into newid the substreamid assigned by the server
    static bool BindPendingStream(XrdClientConn *cliconn, int substreamid, int &newid);

    struct ReadChunk {
	kXR_int64 offset;
	kXR_int32 len;
	int streamtosend;
    };
    

    // This splits a long requests into many smaller requests, to be sent in parallel
    //  through multiple streams
    // Returns false if the chunk is not worth splitting
    static bool SplitReadRequest(XrdClientConn *cliconn, kXR_int64 offset, kXR_int32 len,
				 XrdClientVector<ReadChunk> &reqlists);


};
#endif
