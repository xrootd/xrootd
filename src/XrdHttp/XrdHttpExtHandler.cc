//------------------------------------------------------------------------------
// This file is part of XrdHTTP: A pragmatic implementation of the
// HTTP/WebDAV protocol for the Xrootd framework
//
// Copyright (c) 2017 by European Organization for Nuclear Research (CERN)
// Author: Fabrizio Furano <furano@cern.ch>
// File Date: May 2017
//------------------------------------------------------------------------------
// XRootD is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// XRootD is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with XRootD.  If not, see <http://www.gnu.org/licenses/>.
//------------------------------------------------------------------------------



#include "XrdHttpExtHandler.hh"
#include "XrdHttpReq.hh"
#include "XrdHttpProtocol.hh"

/// Sends a basic response. If the length is < 0 then it is calculated internally
int XrdHttpExtReq::SendSimpleResp(int code, const char* desc, const char* header_to_add, const char* body, long long int bodylen)
{
  if (!prot) return -1;
  
  return prot->SendSimpleResp(code, desc, header_to_add, body, bodylen);
}

int XrdHttpExtReq::StartChunkedResp(int code, const char *desc, const char *header_to_add)
{
  if (!prot) return -1;

  return prot->StartChunkedResp(code, desc, header_to_add);
}

int XrdHttpExtReq::ChunkResp(const char *body, long long bodylen)
{
  if (!prot) return -1;

  return prot->ChunkResp(body, bodylen);
}

int XrdHttpExtReq::BuffgetData(int blen, char **data, bool wait) {

  if (!prot) return -1;
  int nb = prot->BuffgetData(blen, data, wait);
  
  return nb;
}


const XrdSecEntity &XrdHttpExtReq::GetSecEntity() const
{
  return prot->SecEntity;
}


XrdHttpExtReq::XrdHttpExtReq(XrdHttpReq *req, XrdHttpProtocol *pr): prot(pr),
verb(req->requestverb), headers(req->allheaders) {
  // Here we fill the request summary with all the fields we can
  resource = req->resource.c_str();
  
  // These fields usually identify the client that connected

  
  if (prot->SecEntity.moninfo) {
    clientdn = prot->SecEntity.moninfo;
    trim(clientdn);
  }
  if (prot->SecEntity.host) {
    clienthost = prot->SecEntity.host;
    trim(clienthost);
  }
  if (prot->SecEntity.vorg) {
    clientgroups = prot->SecEntity.vorg;
    trim(clientgroups);
  }
  
  length = req->length;
}
