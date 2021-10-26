/*-----------------------------------------------------------------------------
Copyright(c) 2010 - 2018 ViSUS L.L.C.,
Scientific Computing and Imaging Institute of the University of Utah

ViSUS L.L.C., 50 W.Broadway, Ste. 300, 84101 - 2044 Salt Lake City, UT
University of Utah, 72 S Central Campus Dr, Room 3750, 84112 Salt Lake City, UT

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met :

* Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

* Neither the name of the copyright holder nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

For additional information about this project contact : pascucci@acm.org
For support : support@visus.net
-----------------------------------------------------------------------------*/

#include <Visus/NetMessage.h>
#include <Visus/Encoder.h>

namespace Visus {


///////////////////////////////////////////////////////////////////
bool NetMessage::setArrayBody(String compression,Array decoded)
{
  auto encoded=ArrayUtils::encodeArray(compression,decoded);

  if (!encoded)
    return false;

  setHeader("visus-compression"        , compression);
  setHeader("visus-nsamples"           , decoded.dims.toString());
  setHeader("visus-dtype"              , decoded.dtype.toString());
  setHeader("visus-layout"             , decoded.layout);
  setHeader("Content-Transfer-Encoding", "binary");

  if      (compression == "lz4")           setContentType("application/x-lz4");
  else if (compression == "zip")           setContentType("application/zip");
  else if (compression == "png")           setContentType("image/png");
  else if (compression == "jpg")           setContentType("image/jpeg");
  else if (compression == "tif")           setContentType("image/tiff");
  else { VisusAssert(compression.empty()); setContentType("application/octet-stream"); }

  setContentLength(encoded->c_size());

  this->body=encoded;
  return true;
}


///////////////////////////////////////////////////////////////////
String NetRequest::getHeadersAsString() const
{
  if (body && !hasContentLength())
    const_cast<NetRequest*>(this)->setContentLength(body->c_size());

  VisusAssert(!body || getContentLength()==body->c_size());

  std::ostringstream out;
  out<<method<<" "<<url.toString()<<" HTTP/1.1"<<"\r\n";
  for (auto it=headers.begin();it!=headers.end();it++)
    out<<it->first<<": "<<it->second<<"\r\n";
  out<<"\r\n";

  return out.str();
}

///////////////////////////////////////////////////////////////////
bool NetRequest::setHeadersFromString(String headers)
{
  std::vector<String> v=StringUtils::split(headers,"\r\n");

  if (v.empty()) 
    return false;

  String protocol,path,method;
  std::istringstream parser(v[0]);
  parser >> method >> path >> protocol;

  if (method.empty() || path.empty()) 
    ThrowException("invalid request");

  this->url="http://localhost"+path;
  this->method=method;

  for (int i=1;i<(int)v.size();i++)
  {
    int idx=StringUtils::find(v[i],":");
    if (idx<0) continue;
    String key  =StringUtils::trim(v[i].substr(0,idx));
    String value=StringUtils::trim(v[i].substr(idx+1));
    if (!key.empty()) setHeader(key,value);
  }

  return true;
}

///////////////////////////////////////////////////////////////////
String NetRequest::toString() const 
{
  std::ostringstream out;
  out<<"url("<<url.toString()<<") "<<" method("<<method<<") ";
  for (auto it=headers.begin();it!=headers.end();it++)
    out<<it->first<<"("<<it->second<<") ";
  return out.str();
}

///////////////////////////////////////////////////////////////////
String NetResponse::getStatusDescription() const
{
  #define HttpStatusCheckIf(value) case HttpStatus::value: return #value;

  switch (status)
  {
    HttpStatusCheckIf(STATUS_NONE)
    HttpStatusCheckIf(STATUS_CANCELLED)
    HttpStatusCheckIf(STATUS_CANT_RESOLVE)
    HttpStatusCheckIf(STATUS_CANT_RESOLVE_PROXY)
    HttpStatusCheckIf(STATUS_CANT_CONNECT)
    HttpStatusCheckIf(STATUS_CANT_CONNECT_PROXY)
    HttpStatusCheckIf(STATUS_SSL_FAILED)
    HttpStatusCheckIf(STATUS_IO_ERROR)
    HttpStatusCheckIf(STATUS_MALFORMED)
    HttpStatusCheckIf(STATUS_TRY_AGAIN)
    HttpStatusCheckIf(STATUS_CONTINUE)
    HttpStatusCheckIf(STATUS_SWITCHING_PROTOCOLS)
    HttpStatusCheckIf(STATUS_PROCESSING)
    HttpStatusCheckIf(STATUS_OK)
    HttpStatusCheckIf(STATUS_CREATED)
    HttpStatusCheckIf(STATUS_ACCEPTED)
    HttpStatusCheckIf(STATUS_NON_AUTHORITATIVE)
    HttpStatusCheckIf(STATUS_NO_CONTENT)
    HttpStatusCheckIf(STATUS_RESET_CONTENT)
    HttpStatusCheckIf(STATUS_PARTIAL_CONTENT)
    HttpStatusCheckIf(STATUS_MULTI_STATUS)
    HttpStatusCheckIf(STATUS_MULTIPLE_CHOICES)
    HttpStatusCheckIf(STATUS_MOVED_PERMANENTLY)
    HttpStatusCheckIf(STATUS_FOUND)
    //HttpStatusCheckIf(STATUS_MOVED_TEMPORARILY)
    HttpStatusCheckIf(STATUS_SEE_OTHER)
    HttpStatusCheckIf(STATUS_NOT_MODIFIED)
    HttpStatusCheckIf(STATUS_USE_PROXY)
    HttpStatusCheckIf(STATUS_NOT_APPEARING_IN_THIS_PROTOCOL)
    HttpStatusCheckIf(STATUS_TEMPORARY_REDIRECT)
    HttpStatusCheckIf(STATUS_BAD_REQUEST)
    HttpStatusCheckIf(STATUS_UNAUTHORIZED)
    HttpStatusCheckIf(STATUS_PAYMENT_REQUIRED)
    HttpStatusCheckIf(STATUS_FORBIDDEN)
    HttpStatusCheckIf(STATUS_NOT_FOUND)
    HttpStatusCheckIf(STATUS_METHOD_NOT_ALLOWED)
    HttpStatusCheckIf(STATUS_NOT_ACCEPTABLE)
    HttpStatusCheckIf(STATUS_PROXY_AUTHENTICATION_REQUIRED)
    //HttpStatusCheckIf(STATUS_PROXY_UNAUTHORIZED)
    HttpStatusCheckIf(STATUS_REQUEST_TIMEOUT)
    HttpStatusCheckIf(STATUS_CONFLICT)
    HttpStatusCheckIf(STATUS_GONE)
    HttpStatusCheckIf(STATUS_LENGTH_REQUIRED)
    HttpStatusCheckIf(STATUS_PRECONDITION_FAILED)
    HttpStatusCheckIf(STATUS_REQUEST_ENTITY_TOO_LARGE)
    HttpStatusCheckIf(STATUS_REQUEST_URI_TOO_LONG)
    HttpStatusCheckIf(STATUS_UNSUPPORTED_MEDIA_TYPE)
    HttpStatusCheckIf(STATUS_REQUESTED_RANGE_NOT_SATISFIABLE)
    //HttpStatusCheckIf(STATUS_INVALID_RANGE)
    HttpStatusCheckIf(STATUS_EXPECTATION_FAILED)
    HttpStatusCheckIf(STATUS_UNPROCESSABLE_ENTITY)
    HttpStatusCheckIf(STATUS_LOCKED)
    HttpStatusCheckIf(STATUS_FAILED_DEPENDENCY)
    HttpStatusCheckIf(STATUS_INTERNAL_SERVER_ERROR)
    HttpStatusCheckIf(STATUS_NOT_IMPLEMENTED)
    HttpStatusCheckIf(STATUS_BAD_GATEWAY)
    HttpStatusCheckIf(STATUS_SERVICE_UNAVAILABLE)
    HttpStatusCheckIf(STATUS_GATEWAY_TIMEOUT)
    HttpStatusCheckIf(STATUS_HTTP_VERSION_NOT_SUPPORTED)
    HttpStatusCheckIf(STATUS_INSUFFICIENT_STORAGE)
    HttpStatusCheckIf(STATUS_NOT_EXTENDED)

    default:
      break;
  }

  #undef HttpStatusCheckIf

  VisusAssert(false);
  return "<unknown status>";
}

///////////////////////////////////////////////////////////////////
String NetResponse::getHeadersAsString() const
{
  if (body && !hasContentLength())
    const_cast<NetResponse*>(this)->setContentLength(body->c_size());
  VisusAssert(!body || getContentLength()==body->c_size());

  std::ostringstream out;
  out<<"HTTP/1.1"<<" "<<status<<" "<<getStatusDescription()<<"\r\n";
  for (auto it=headers.begin();it!=headers.end();it++)
    out<<it->first<<": "<<it->second<<"\r\n";
  out<<"\r\n";

  return out.str();
}

///////////////////////////////////////////////////////////////////
bool NetResponse::setHeadersFromString(String headers)
{
  std::vector<String> v=StringUtils::split(headers,"\r\n");

  if (v.empty()) 
    return false;

  String http,status,status_descr;
  std::istringstream parser(v[0]);
  parser >> http >> status >> status_descr;

  if (http.empty() || status.empty() || status_descr.empty()) 
    return false;

  this->status=cint(status);

  for (int i=1;i<(int)v.size();i++)
  {
    int idx=StringUtils::find(v[i],":");
    if (idx<0) continue;
    String key  =StringUtils::trim(v[i].substr(0,idx));
    String value=StringUtils::trim(v[i].substr(idx+1));
    if (!key.empty()) setHeader(key,value);
  }

  return true;
}


///////////////////////////////////////////////////////////////////
String NetResponse::toString() const 
{
  std::ostringstream out;
  out<<"status("<<getStatusDescription()<<") "<<" errormsg("<<getErrorMessage()<<") ";
  for (auto it=headers.begin();it!=headers.end();it++)
    out<<it->first<<"("<<it->second<<") ";
  return out.str();
}


///////////////////////////////////////////////////////////////////
NetResponse NetResponse::compose(const std::vector<NetResponse>& responses)
{
  VisusAssert(!responses.empty());

  if (responses.size()==1)
    return responses[0];

  NetResponse RESPONSE(HttpStatus::STATUS_OK);
  RESPONSE.setHeader("response-compose-num",cstring((int)responses.size()));

  Int64 body_size=0;
  for (auto response : responses)
    body_size+=response.body? response.body->c_size() : 0;

  if (body_size)
  {
    RESPONSE.body=std::make_shared<HeapMemory>();
    if (!RESPONSE.body->resize(body_size,__FILE__,__LINE__))
      return NetResponse(HttpStatus::STATUS_NOT_FOUND,"Out of memory");
  }

  Uint8* BODY=RESPONSE.body? RESPONSE.body->c_ptr() : nullptr;

  for (int I=0;I<responses.size();I++)
  {
    auto postfix="-"+cstring(I);

    const auto& response=responses[I];

    RESPONSE.setHeader("status"   +postfix,cstring(response.status));
    RESPONSE.setHeader("body-size"+postfix,cstring(response.body? response.body->c_size() : 0));

    for (auto header : response.headers)
      RESPONSE.setHeader(header.first+postfix,header.second);

    if (response.body)
    {
      memcpy(BODY,response.body->c_ptr(),response.body->c_size());
      BODY+=response.body->c_size();
    }
  }

  return RESPONSE;
}

///////////////////////////////////////////////////////////////////
std::vector<NetResponse> NetResponse::decompose(NetResponse RESPONSE)
{
  int num=cint(RESPONSE.getHeader("response-compose-num"));

  if (!num)
    return std::vector<NetResponse>({RESPONSE});

  std::vector<NetResponse> responses(num);
    
  Uint8* BODY=RESPONSE.body? RESPONSE.body->c_ptr() : nullptr;

  for (int I=0;I<num;I++)
  {
    auto postfix="-"+cstring(I);

    responses[I].status=RESPONSE.isSuccessful()? cint(RESPONSE.getHeader("status"+postfix,cstring(RESPONSE.status))) : RESPONSE.status;

    for (auto header : RESPONSE.headers)
    {
      if (StringUtils::endsWith(header.first,postfix))
        responses[I].setHeader(header.first.substr(0,header.first.size()-postfix.size()),header.second);
    }

    auto body_size=cint64(RESPONSE.getHeader("body-size"+postfix));
    if (!body_size)
      continue;

    responses[I].body=std::make_shared<HeapMemory>();
    if (!responses[I].body->resize(body_size,__FILE__,__LINE__)) 
    {
      responses[I]=NetResponse(HttpStatus::STATUS_INTERNAL_SERVER_ERROR,"Out of memory");
      continue;
    }

    memcpy(responses[I].body->c_ptr(),BODY,body_size);
    BODY+=body_size;
  }

  return responses;
}

} //namespace Visus

