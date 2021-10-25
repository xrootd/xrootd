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

#include "Path.h"

namespace Visus {



///////////////////////////////////////////////////////////////////////
bool Path::isGoodNormalizedPath(String path)
{
  if (path.empty())
    return true;

  return
    //always start with [/] or [c:] or [d:] or ... 
    (((path.size() >= 1 && path[0] == '/') || (path.size() >= 2 && isalpha(path[0]) && path[1] == ':'))) &&
    //does not end with "/" (unless is [/])
    (path == "/" || !StringUtils::endsWith(path, "/")) &&
    //does not contain window separator
    (!StringUtils::contains(path, "\\")) &&

    //i can use the tilde for temporary path
#if 0
    //does not contain home alias
    (!StringUtils::contains(path, "~")) &&
#endif

    //does not contain any other alias
    (!StringUtils::contains(path, "$"));
}


///////////////////////////////////////////////////////////////////////
const String Path::normalizePath(String value)
{
  if (value.empty())
    return "";

  //don't want window separators
  value =StringUtils::replaceAll(value,"\\", "/");

  //example [c:/something/] -> [c:/something]
  if (StringUtils::endsWith(value,"/") && value !="/")
    value = value.substr(0, value.length()-1);
    
  //example [//something] -> [/something] 
  while (StringUtils::startsWith(value,"//"))
    value = value.substr(1);

  //example [/C:] -> [C:]
  if (value.size()>=3 && value[0]=='/' && isalpha(value[1]) && value[2]==':')
    value = value.substr(1);

  VisusAssert(isGoodNormalizedPath(value));
  return value;
}

//////////////////////////////////////////////////////////
Path Path::getParent(bool normalize) const
{
  if (path.empty() || isRootDirectory())
    return Path();

  const int idx = (int)path.rfind("/");

  if (idx>0) 
  {
    // example [/mnt/free] -> [/mnt]
    String ret=path.substr(0, idx);
    VisusAssert(isGoodNormalizedPath(ret));
    return Path(ret,normalize);
  }
  else 
  {
    //example [/mnt] -> [/]
    VisusAssert(idx==0); //since is not root directory IT MUST HAVE a parent (see normalizePath function)
    return Path("/");
  }

}


} //namespace Visus



