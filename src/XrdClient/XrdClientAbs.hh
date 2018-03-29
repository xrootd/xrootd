#ifndef XRD_ABSCLIENTBASE_H
#define XRD_ABSCLIENTBASE_H
/******************************************************************************/
/*                                                                            */
/*                     X r d C l i e n t A b s . h h                          */
/*                                                                            */
/* Author: Fabrizio Furano (INFN Padova, 2004)                                */
/* Adapted from TXNetFile (root.cern.ch) originally done by                   */
/*  Alvise Dorigo, Fabrizio Furano                                            */
/*          INFN Padova, 2003                                                 */
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
// Base class for objects handling redirections keeping open files      //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#include <string.h>

#include "XrdClient/XrdClientUnsolMsg.hh"
#include "XrdClient/XrdClientUrlInfo.hh"

#include "XProtocol/XPtypes.hh"

class XrdClientCallback;
class XrdClientConn;

class XrdClientAbs: public XrdClientAbsUnsolMsgHandler {

   // Do NOT abuse of this
   friend class XrdClientConn;


protected:
   XrdClientConn*              fConnModule;

   char                        fHandle[4];  // The file handle returned by the server,
                                            // to be used for successive requests


   XrdClientCallback*          fXrdCcb;
   void *                      fXrdCcbArg;
   
   // After a redirection the file must be reopened.
   virtual bool OpenFileWhenRedirected(char *newfhandle, 
				       bool &wasopen) = 0;

   // In some error circumstances (e.g. when writing)
   // a redirection on error must be denied
   virtual bool CanRedirOnError() = 0;

public:

   XrdClientAbs(XrdClientCallback *XrdCcb = 0, void *XrdCcbArg = 0) {
      memset( fHandle, 0, sizeof(fHandle) );

      // Set the callback object, if any
      fXrdCcb = XrdCcb;
      fXrdCcbArg = XrdCcbArg;
   }

   virtual bool IsOpen_wait() {
     return true;
   };

   void SetParm(const char *parm, int val);
   void SetParm(const char *parm, double val);

   // Hook to the open connection (needed by TXNetFile)
   XrdClientConn              *GetClientConn() const { return fConnModule; }

   XrdClientUrlInfo GetCurrentUrl();

   // The last response got from a non-async request
   struct ServerResponseHeader *LastServerResp();

   struct ServerResponseBody_Error *LastServerError();

   // Asks for the value of some parameter
   //---------------------------------------------------------------------------
   //! @param ReqCode request code
   //! @param Args arguments
   //! @param Resp a prealocated buffer
   //! @param MaxResplen size of the buffer
   //---------------------------------------------------------------------------
   bool Query(kXR_int16 ReqCode, const kXR_char *Args, kXR_char *Resp, kXR_int32 MaxResplen);

   //---------------------------------------------------------------------------
   //! @param ReqCode request code
   //! @param Args arguments
   //! @param Resp pointer to a preallocated buffer or a pointer to 0 if a
   //!             sufficiently large buffer should be allocated automagically,
   //!             in which case the buffer needs to be freed with free()
   //! @param MaxResplen size of the buffer or 0 for automatic allocation
   //---------------------------------------------------------------------------
   bool Query( kXR_int16 ReqCode, const kXR_char *Args, kXR_char **Resp, kXR_int32 MaxResplen );

};
#endif
