#ifndef XRD_CLIENT_READV
#define XRD_CLIENT_READV
/******************************************************************************/
/*                                                                            */
/*                   X r d C l i e n t R e a d V . h h                        */
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
// Helper functions for the vectored read functionality                 //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

class XrdClientConn;
#include "XProtocol/XPtypes.hh"
#include "XProtocol/XProtocol.hh"
#include "XrdClient/XrdClientVector.hh"

struct XrdClientReadVinfo {
  kXR_int64 offset;
  kXR_int32 len;
};

class XrdClientReadV {
public:
  
  // Builds a request and sends it to the server
    // If destbuf == 0 the request is sent asynchronously
  static kXR_int64 ReqReadV(XrdClientConn *xrdc, char *handle, char *destbuf,
			    XrdClientVector<XrdClientReadVinfo> &reqvect,
			    int firstreq, int nreq, int streamtosend);
  
  // Picks a readv response and puts the individual chunks into the dest buffer
  static kXR_int32 UnpackReadVResp(char *destbuf, char *respdata, kXR_int32 respdatalen,
				   readahead_list *buflis, int nbuf);
  
  // Picks a readv response and puts the individual chunks into the cache
  static kXR_int32 SubmitToCacheReadVResp(XrdClientConn *xrdc, char *respdata,
					    kXR_int32 respdatalen);
  
  static void PreProcessChunkRequest(XrdClientVector<XrdClientReadVinfo> &reqvect,
				     kXR_int64 offs, kXR_int32 len,
				     kXR_int64 filelen);
				     
  static void PreProcessChunkRequest(XrdClientVector<XrdClientReadVinfo> &reqvect,
				     kXR_int64 offs, kXR_int32 len,
				     kXR_int64 filelen,
				     kXR_int32 spltsize);
};
#endif
