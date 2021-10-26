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

#ifndef VISUS_URL_H
#define VISUS_URL_H

#include <Visus/Kernel.h>
#include <Visus/StringUtils.h>
#include <Visus/StringMap.h>

namespace Visus {


  /////////////////////////////////////////////////////////////////////
class VISUS_KERNEL_API Url 
{
public:

  VISUS_CLASS(Url)

  //params
  StringMap params;

  //default constructor
  inline Url() :port(0)
  {}

  //constructor from string description
  Url(String url);

  //getProtocol
  inline String getProtocol() const
  {return protocol;}

  //setProtocol
  inline void setProtocol(String value)
  {this->protocol=value;}

  //isFile
  inline bool isFile() const
  {return protocol=="file";}

  //isRemote
  inline bool isRemote() const
  {return !hostname.empty();}

  //getHostname
  inline String getHostname() const
  {return this->hostname;}

  //setHostname
  inline void setHostname(String value) 
  {this->hostname=value;}

  //getPort
  inline int getPort() const
  {return this->port;}

  inline void setPort(int value)
  {this->port=value;}

  //getPath
  inline String getPath() const
  {VisusAssert(!StringUtils::contains(path,"?"));return path;}

  //setPath
  inline void setPath(String path)
  {VisusAssert(!StringUtils::contains(path,"?"));this->path=path;}

  //withPath
  Url withPath(String path)
  {
    auto ret = *this;
    ret.setPath(path);
    return ret;
  }

  //hasParam
  inline bool hasParam(String key) const
  {return params.hasValue(key);}

  //getParam
  inline String getParam(String key,String default_value="") const
  {return params.getValue(key,default_value);}

  //setParam
  inline void setParam(String key,String value)
  {params.setValue(key,value);}

  //convert to string (if invalid url return "")
  String toString() const;

  //valid
  inline bool valid() const
  {return !protocol.empty();}

  //equality operator
  bool operator==(const Url& src) const
  {return this->toString()==src.toString();}

private:

  //protocol (example "http" "https")
  String protocol;

  //the hostname (ex "localhost")
  String  hostname;

  //only if http (ex 10000)
  int     port;

  //path (example: /mod_visus)
  String path;


}; //end class

} //namespace Visus

#endif //VISUS_URL_H

