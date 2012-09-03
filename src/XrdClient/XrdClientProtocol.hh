#ifndef XRD_CPROTOCOL_H
#define XRD_CPROTOCOL_H
/******************************************************************************/
/*                                                                            */
/*                X r d C l i e n t P r o t o c o l . h h                     */
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
// utility functions to deal with the protocol                          //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#include "XProtocol/XProtocol.hh"

void clientMarshall(ClientRequest* str);
void clientMarshallReadAheadList(readahead_list *buf_list, kXR_int32 dlen);
void clientUnMarshallReadAheadList(readahead_list *buf_list, kXR_int32 dlen);
void clientUnmarshall(struct ServerResponseHeader* str);

void ServerResponseHeader2NetFmt(struct ServerResponseHeader *srh);
void ServerInitHandShake2HostFmt(struct ServerInitHandShake *srh);

bool isRedir(struct ServerResponseHeader *ServerResponse);

char *convertRequestIdToChar(kXR_unt16 requestid);

void PutFilehandleInRequest(ClientRequest* str, char *fHandle);

char *convertRespStatusToChar(kXR_unt16 status);

void smartPrintClientHeader(ClientRequest* hdr);
void smartPrintServerHeader(struct ServerResponseHeader* hdr);

#endif
