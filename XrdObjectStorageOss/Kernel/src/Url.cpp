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

#include <Visus/Url.h>

namespace Visus {

////////////////////////////////////////////////////////
Url::Url(String value) : port(0)
{
  //parse protocol
  int idx_protocol=StringUtils::find(value,"://");
  if (idx_protocol>=0)
  {
    this->protocol=value.substr(0,idx_protocol);
    value =value.substr(idx_protocol+3);
  }
  else
  {
    //default is file protocol
    protocol="file";
  }

  //parse params
  {
    ParseStringParams parse(value);
    value=parse.without_params;
    this->params=parse.params;
  }

  //parse the hostname
  if (protocol!="file")
  {
    int idx_hostname=StringUtils::find(value,"/");
    if (idx_hostname>=0)
    {
      hostname = value.substr(0,idx_hostname);
      value    = value.substr(idx_hostname);
    }
    else
    {
      hostname=value;
      value="";
    }

    //eventually parse the port
    int idx_port=StringUtils::find(hostname,":");
    if (idx_port>=0)
    {
      port    =cint(hostname.substr(idx_port+1));
      hostname=hostname.substr(0,idx_port);
    }
    else
    {
      this->port=80;
    }
  }
  
  //remaining is the path!
  this->path=value;

  //fix the path contains a letter name (example /c:/path/visus.idx -> c:/path)
  if (isFile() && path.size()>=3 && path[0]=='/' && path[2]==':')
    path=path.substr(1);

  //failed to parse url
  if (!this->valid())
    {(*this)=Url();return;}
}


///////////////////////////////////////////////////////
String Url::toString() const
{
  if (!this->valid()) return "";

  std::ostringstream out;

  if (!protocol.empty())
    out<<protocol<<"://";

  if (isRemote()) 
    out<<this->hostname<< ((port==80)? ("") : (":" + cstring(this->port)));

  //example path=c:/path/visus.idx -> file:///c:/path/visus.idx
  if (isFile() && StringUtils::contains(path,":")) 
    out<<"/";

  out<<path;

  int nparam=0;
  for (auto it=params.begin();it!=params.end();it++,nparam++)
    out<< ((nparam==0)?"?":"&") << it->first<<"="<<StringUtils::addEscapeChars(it->second);

  return out.str();
}


} //namespace Visus

