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

#ifndef VISUS_NET_REQUEST_H
#define VISUS_NET_REQUEST_H

#include <Visus/Kernel.h>
#include <Visus/Array.h>
#include <Visus/Url.h>
#include <Visus/StringMap.h>
#include <Visus/Time.h>

namespace Visus {

/////////////////////////////////////////////////////////////////
class VISUS_KERNEL_API HttpStatus
{
public:

  VISUS_CLASS(HttpStatus)

  enum {
    
    STATUS_NONE,

    //Informational  1xx
    STATUS_CANCELLED                       = 1,
    STATUS_CANT_RESOLVE,
    STATUS_CANT_RESOLVE_PROXY,
    STATUS_CANT_CONNECT,
    STATUS_CANT_CONNECT_PROXY,
    STATUS_SSL_FAILED,
    STATUS_IO_ERROR,
    STATUS_MALFORMED,
    STATUS_TRY_AGAIN,

    //Successful 2xx
    STATUS_CONTINUE                        = 100,
    STATUS_SWITCHING_PROTOCOLS             = 101,
    STATUS_PROCESSING                      = 102, 

    STATUS_OK                              = 200,
    STATUS_CREATED                         = 201,
    STATUS_ACCEPTED                        = 202,
    STATUS_NON_AUTHORITATIVE               = 203,
    STATUS_NO_CONTENT                      = 204,
    STATUS_RESET_CONTENT                   = 205,
    STATUS_PARTIAL_CONTENT                 = 206,
    STATUS_MULTI_STATUS                    = 207, 

    //Redirection 3xx
    STATUS_MULTIPLE_CHOICES                = 300,
    STATUS_MOVED_PERMANENTLY               = 301,
    STATUS_FOUND                           = 302,
    STATUS_MOVED_TEMPORARILY               = 302, 
    STATUS_SEE_OTHER                       = 303,
    STATUS_NOT_MODIFIED                    = 304,
    STATUS_USE_PROXY                       = 305,
    STATUS_NOT_APPEARING_IN_THIS_PROTOCOL  = 306, 
    STATUS_TEMPORARY_REDIRECT              = 307,

    //Client Error 4xx
    STATUS_BAD_REQUEST                     = 400,
    STATUS_UNAUTHORIZED                    = 401,
    STATUS_PAYMENT_REQUIRED                = 402, 
    STATUS_FORBIDDEN                       = 403,
    STATUS_NOT_FOUND                       = 404,
    STATUS_METHOD_NOT_ALLOWED              = 405,
    STATUS_NOT_ACCEPTABLE                  = 406,
    STATUS_PROXY_AUTHENTICATION_REQUIRED   = 407,
    STATUS_PROXY_UNAUTHORIZED              = STATUS_PROXY_AUTHENTICATION_REQUIRED,
    STATUS_REQUEST_TIMEOUT                 = 408,
    STATUS_CONFLICT                        = 409,
    STATUS_GONE                            = 410,
    STATUS_LENGTH_REQUIRED                 = 411,
    STATUS_PRECONDITION_FAILED             = 412,
    STATUS_REQUEST_ENTITY_TOO_LARGE        = 413,
    STATUS_REQUEST_URI_TOO_LONG            = 414,
    STATUS_UNSUPPORTED_MEDIA_TYPE          = 415,
    STATUS_REQUESTED_RANGE_NOT_SATISFIABLE = 416,
    STATUS_INVALID_RANGE                   = STATUS_REQUESTED_RANGE_NOT_SATISFIABLE,
    STATUS_EXPECTATION_FAILED              = 417,
    STATUS_UNPROCESSABLE_ENTITY            = 422, 
    STATUS_LOCKED                          = 423, 
    STATUS_FAILED_DEPENDENCY               = 424, 

    //Server Error 5xx
    STATUS_INTERNAL_SERVER_ERROR           = 500,
    STATUS_NOT_IMPLEMENTED                 = 501,
    STATUS_BAD_GATEWAY                     = 502,
    STATUS_SERVICE_UNAVAILABLE             = 503,
    STATUS_GATEWAY_TIMEOUT                 = 504,
    STATUS_HTTP_VERSION_NOT_SUPPORTED      = 505,
    STATUS_INSUFFICIENT_STORAGE            = 507,
    STATUS_NOT_EXTENDED                    = 510 
  } ;

private:

  HttpStatus();
};



///////////////////////////////////////////////////////////////////////////////////////
class VISUS_KERNEL_API NetMessage 
{
public:

  VISUS_CLASS(NetMessage)

  //headers (example: keep-alive: true)
  StringMap headers;

  //this is needed for POST,PUT
  SharedPtr<HeapMemory> body;

  //constructor
  NetMessage(){
  }

  //destructor
  virtual ~NetMessage(){
  }

  //access a header, , return <default_value> if not found
  String getHeader(String key, String default_value = "") const{
    return headers.getValue(key, default_value);
  }

  //setHeader
  void setHeader(String key, String value){
    headers.setValue(key, value);
  }

  //eraseHeader
  void eraseHeader(const String& key){
    headers.eraseValue(key);
  }

  //hasHeader
  bool hasHeader(String key) const{
    return headers.hasValue(key);
  }

  //getTextBody
  String getTextBody() const {
    return body ? String((char*)body->c_ptr(), (size_t)body->c_size()) : String();
  }

  //setTextBody
  void setTextBody(const String& value, bool bAsBinary = false)
  {
    if (bAsBinary) {
      setContentType("application/octet-stream");
      setHeader("Content-Transfer-Encoding", "binary");
    }

    int N = (int)value.size();
    if (!body) body = std::make_shared<HeapMemory>();

    if (!body->resize(N, __FILE__, __LINE__))
      ThrowException("out of memory");
    memcpy(body->c_ptr(), value.c_str(), N);
    setContentLength(N);
  }


  //setXmlBody
  void setXmlBody(const String& value) {
    setContentType("text/xml");
    setTextBody(value);
  }

  //setHtmlBody
  void setHtmlBody(const String& value) {
    setContentType("text/html");
    setTextBody(value);
  }

  //setJSONBody
  void setJSONBody(const String& value){
    setContentType("application/json");
    setTextBody(value);
  }

  //setArrayBody
  bool setArrayBody(String compression,Array value);

  //getArrayBody
  Array getArrayBody() const {
    return ArrayUtils::decodeArray(this->headers, this->body);
  }

  //getCompatibleArrayBody
  Array getCompatibleArrayBody(PointNi requested_dims, DType requested_dtype) {
    Array ret = getArrayBody();

    if (ret.dtype != requested_dtype)
      return Array();

    //backward compatible
    if (ret.dims.innerProduct() != requested_dims.innerProduct())
      return Array();

    ret.resize(requested_dims, requested_dtype, __FILE__, __LINE__);
    return ret;
  }

public:

  //hasContentLength
  bool hasContentLength() const {
    return hasHeader("Content-Length");
  }

  //getContentLenght
  Int64 getContentLength() const{
    return cint64(getHeader("Content-Length"));
  }

  //setContentLength
  void setContentLength(Int64 value){
    setHeader("Content-Length", cstring(value));
  }

  //hasContentType
  bool hasContentType() const{
    return hasHeader("Content-Type");
  }

  //getContentType
  String getContentType() const{
    return getHeader("Content-Type");
  }

  //setContentType
  void setContentType(String value){
    setHeader("Content-Type", value);
  }

  //eraseContentType
  void eraseContentType(){
    eraseHeader("Content-Type");
  }

  //hasAttachedFilename
  bool hasAttachedFilename() const{
    return hasHeader("Content-Disposition");
  }

  //getAttachedFilename
  String getAttachedFilename() const{
    return StringUtils::trim(StringUtils::nextToken(getHeader("Content-Disposition"), "filename="));
  }

  //setAttachedFilename
  void setAttachedFilename(String filename){
    setHeader("Content-Disposition", "attachment; filename=" + filename);
  }

  //hasErrorMessage
  bool hasErrorMessage() const{
    return hasHeader("visus-errormsg");
  }

  //getErrorMessage
  String getErrorMessage() const{
    return getHeader("visus-errormsg");
  }

  //setErrorMessage
  void setErrorMessage(String value){
    setHeader("visus-errormsg", value);
  }

};

///////////////////////////////////////////////////////////////////////////////////////
class VISUS_KERNEL_API NetRequest : public NetMessage
{
public:

  VISUS_CLASS(NetRequest)

  //aborted
  Aborted aborted;
  
  //url
  Url url;

  //method DELETE,GET,HEAD,POST,PUT
  String method;

  struct
  {
  public:
    Time enter_t1;
    Time run_t1;
    int  wait_msec=0;
    int  run_msec=0;
  }
  statistics;

  //default constructor
  NetRequest() : method("GET")
  {}

  //constructor
  NetRequest(Url url_,String method_="GET") : url(url_),method(method_)
  {}

  //constructor
  NetRequest(String url_) : url(url_),method("GET")
  {}

  //destructor
  virtual ~NetRequest()
  {}

  //valid
  bool valid() const
  {return url.valid();}

  //getHeadersAsString
  String getHeadersAsString() const;

  //setHeadersFromString
  bool setHeadersFromString(String value);

  //toString
  String toString() const;

};


///////////////////////////////////////////////////////////////////////////////////////
class VISUS_KERNEL_API NetResponse : public NetMessage
{
public:

  VISUS_CLASS(NetResponse)

  int  status;

  //default constructor
  NetResponse() : status(HttpStatus::STATUS_NONE) {
  }

  //default constructor
  explicit NetResponse(int status_,String errormsg="") : status(status_) {
    if (!errormsg.empty()) 
      setErrorMessage(errormsg);
  }

  //destructor
  virtual ~NetResponse() {
  }

  //isXXXX
  bool isInformational() const {return status>=100 && status<200;}
  bool isSuccessful   () const {return status>=200 && status<300;}
  bool isRedirection  () const {return status>=300 && status<400;}
  bool isClientError  () const {return status>=400 && status<500;}
  bool isServerError  () const {return status>=500 && status<600;}
  
  //getStatusDescription
  String getStatusDescription() const;

  //getHeadersAsString
  String getHeadersAsString() const;

  //setHeadersFromString
  bool setHeadersFromString(String value);

  //toString
  String toString() const;

public:

  //compose
  static NetResponse compose(const std::vector<NetResponse>& responses);

  //decompose
  static std::vector<NetResponse> decompose(NetResponse RESPONSE);

};

} //namespace Visus

#endif //VISUS_NET_REQUEST_H

