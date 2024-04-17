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


#include "Xrd/XrdLink.hh"
#include "XrdHttpExtHandler.hh"
#include "XrdHttpReq.hh"
#include "XrdHttpProtocol.hh"
#include "XrdOuc/XrdOucEnv.hh"

/// Sends a basic response. If the length is < 0 then it is calculated internally
int XrdHttpExtReq::SendSimpleResp(int code, const char* desc, const char* header_to_add, const char* body, long long int bodylen)
{
  if (!prot) return -1;

  // @FIXME
  // - need this to circumvent missing API calls and keep ABI compatibility
  // - when large files are returned we cannot return them in a single buffer
  // @TODO: for XRootD 5.0 this two hidden calls should just be added to the external handler API and the code here can be removed

  if ( code == 0 ) {
    return prot->StartSimpleResp(200, desc, header_to_add, bodylen, true);
  }

  if ( code == 1 ) {
    return prot->SendData(body, bodylen);
  }

  return prot->SendSimpleResp(code, desc, header_to_add, body, bodylen, true);
}

int XrdHttpExtReq::StartChunkedResp(int code, const char *desc, const char *header_to_add)
{
  if (!prot) return -1;

  return prot->StartChunkedResp(code, desc, header_to_add, -1, true);
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

void XrdHttpExtReq::GetClientID(std::string &clid)
{
   char buff[512];
   prot->Link->Client(buff, sizeof(buff));
   clid = buff;
}

const XrdSecEntity &XrdHttpExtReq::GetSecEntity() const
{
  return prot->SecEntity;
}


XrdHttpExtReq::XrdHttpExtReq(XrdHttpReq *req, XrdHttpProtocol *pr): prot(pr),
verb(req->requestverb), headers(req->allheaders) {
  // Here we fill the request summary with all the fields we can
  resource = req->resource.c_str();
  int envlen = 0;
  
  const char *p = nullptr;
  if (req->opaque)
    p = req->opaque->Env(envlen);
  headers["xrd-http-query"] = p ? p:"";
  p = req->resourceplusopaque.c_str();
  headers["xrd-http-fullresource"] = p ? p:"";
  headers["xrd-http-prot"] = prot->isHTTPS()?"https":"http";
  
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

  // Get the packet marking handle and the client scitag from the XrdHttp layer
  pmark = prot->pmarkHandle;
  mSciTag = req->mScitag;

  length = req->length;
}
