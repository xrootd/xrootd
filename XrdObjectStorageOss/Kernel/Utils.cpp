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

#include "Utils.h"
#include "StringUtils.h"
#include "Url.h"
#include "Path.h"
#include "NetService.h"
#include "File.h"
#include "CloudStorage.h"

#include <cctype>
#include <iomanip>
#include <fstream>
#include <assert.h>

#include <unistd.h>

namespace Visus {
  
///////////////////////////////////////////////////////////////////////////////////////
int Utils::getPid()
{
  return ::getpid();
}

///////////////////////////////////////////////////////////////////////////////////////
String Utils::getEnv(String key)
{
  auto e = getenv(key.c_str());
  return e? e : "";
}

///////////////////////////////////////////////////////////////////////////////////////
void Utils::setEnv(String key, String value)
{
#if WIN32
  _putenv_s(key.c_str(), value.c_str());
#else
  setenv(key.c_str(), value.c_str(), 1);
#endif
}



///////////////////////////////////////////////////////////////////////////////////////
void Utils::breakInDebugger()
{
#ifdef _DEBUG

        //break in debugger
    #if WIN32 
    _CrtDbgBreak();
    #elif __APPLE__
        asm("int $3");
    #else
        ::kill(0, SIGTRAP);
        assert(0);
    #endif

#endif
}

//////////////////////////////////////////////////////////////////
String Utils::loadTextDocument(String s_url)
{
  Url url(s_url);

  if (url.isFile())
  {
    String filename=url.getPath();

    if (filename.empty()) 
      return "";

    Path path(filename);
    if (path.empty()) 
      return "";

    String fullpath=path.toString();
    std::ifstream file(fullpath.c_str(), std::ios::binary);
    if (!file.is_open())
    {
	    PrintWarning("Failed to loadTextDocument", s_url, "Reason: ", strerror(errno));
      return "";
    }

    std::stringstream sstream;
    sstream << file.rdbuf();
    String ret=sstream.str();

    //I got some weird files! (rtrim does not work!)
    int ch; 
    while (!ret.empty() && ((ch=ret[ret.size()-1])=='\0' || ch==' ' || ch=='\t' || ch=='\r' || ch=='\n'))
      ret.resize(ret.size()-1);

    return ret;
  }
  else
  {
    if (auto cloud_storage = CloudStorage::createInstance(url))
    {
      auto blob_name = url.getPath();
      auto blob = cloud_storage->getBlob(SharedPtr<NetService>(), blob_name).get();
      if (blob->valid())
        return String((char*)blob->body->c_ptr(), (size_t)blob->body->c_size());
    }

    auto net_response=NetService::getNetResponse(url);
    if (!net_response.isSuccessful()) return "";
    return net_response.getTextBody();
  }
}

//////////////////////////////////////////////////////////////////
void Utils::saveTextDocument(String filename,String content)
{
  if (filename.empty())
    ThrowException("invalid filename");

  Path path(filename);
  String fullpath= path.toString();
  std::ofstream file(fullpath.c_str(), std::ios::binary);

  if (!file.is_open())
  {
    //try to create the directory
    FileUtils::createDirectory(path.getParent());
    file.open(fullpath.c_str(), std::ios::binary);

    if (!file.is_open())
      ThrowException("Failed to save text document",filename,strerror(errno));
  }

  file.write(content.c_str(),content.size());
  file.close();
}


///////////////////////////////////////////////////////////////////////////////////////////////
SharedPtr<HeapMemory> Utils::loadBinaryDocument(String url_)
{
  Url url(url_);

  if (url.isFile())
  {
    String filename=url.getPath();
    if (filename.empty()) 
      return SharedPtr<HeapMemory>();

    Path path(filename);
    if (path.empty()) 
      return SharedPtr<HeapMemory>();

    String fullpath=path.toString();
    std::ifstream file(fullpath.c_str(), std::ios::binary);
    if (!file.is_open()) 
      return SharedPtr<HeapMemory>();

    file.seekg(0, file.end);
    int length=(int)file.tellg(); 

    if (length <= 0)
    {
        file.close();
        return SharedPtr<HeapMemory>();
    }

    file.seekg(0,file.beg);

    auto dst=std::make_shared<HeapMemory>();

    if (!dst->resize(length, __FILE__, __LINE__))
    {
        file.close();
        return SharedPtr<HeapMemory>();
    }

    if (!file.read((char*)dst->c_ptr(), length))
    {
        file.close();
        return SharedPtr<HeapMemory>();
    }

    file.close();
    return dst;
  }
  else
  {
    auto net_response=NetService::getNetResponse(url);

    if (!net_response.isSuccessful()) 
      return SharedPtr<HeapMemory>();

    return net_response.body;
  }  
}

///////////////////////////////////////////////////////////////////////////////////////////////
void Utils::saveBinaryDocument(String filename,SharedPtr<HeapMemory> src)
{
  if (!src)
    ThrowException("src is empty()");

  //TODO!
  Path path(filename);
  String fullpath=path.toString();
  std::ofstream file(fullpath.c_str(), std::ios::binary);

  //try to create the directory
  if (!file.is_open())
  {
    FileUtils::createDirectory(path.getParent());
    file.open(fullpath.c_str(), std::ios::binary);
    if (!file.is_open())
      ThrowException("cannot open file for writing");
  }


  if (!file.write((char*)src->c_ptr(),src->c_size())) {
    file.close();
    ThrowException("cannot write binary buffer");
  }

  file.close();
}

} //namespace Visus


