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

#ifndef __VISUS_PATH_H__
#define __VISUS_PATH_H__

#include <Visus/Kernel.h>
#include <Visus/StringUtils.h>

namespace Visus {

///////////////////////////////////////////////////////////////////////
class VISUS_KERNEL_API Path
{
public:

  VISUS_CLASS(Path)

  //constructor empty (invalid) file
  Path(){
  }

  // Creates a file from an absolute path
  Path(String path, bool normalize = true){
    this->path = normalize ? normalizePath(path) : path;
  }

  // copy constructor
  Path(const Path& other){
    this->path = other.path;
  }

  //destructor
  ~Path(){
  }

  //invalid
  static Path invalid() {
    return Path();
  }

  //empty
  bool empty() const {
    return path.empty();
  }

  //isRootDirectory (example [/] | [c:])
  bool isRootDirectory() const {
    int N = (int)path.size(); return (N == 1 && path[0] == '/') || (N == 2 && isalpha(path[0]) && path[1] == ':');
  }

  //string
  #if !SWIG
  operator String() const {
    return path;
  }
  #endif

  // returns the complete, absolute path of this file
  String toString() const {
    return path;
  }

  // Returns the directory that contains this file or directory (e.g. for "/moose/fish/foo.txt" this will return "/moose/fish")
  Path getParent(bool normalize = true) const;

  //return a child
  Path getChild(String child) const {
    return Path(path + String("/") + child);
  }

  // Returns the last section of the pathname (ex "/moose/fish/foo.txt" this will return "foo.txt")
  String getFileName() const {
    int idx = (int)path.rfind("/"); 
    return (idx < 0) ? path : path.substr(idx + 1);
  }

  // Returns the last part of the filename, without its file extension (e.g. for "/moose/fish/foo.txt" this will return "foo")
  String getFileNameWithoutExtension() const{
    String filename = getFileName(); 
    const int idx = (int)filename.rfind("."); 
    return (idx < 0) ? filename : filename.substr(0, idx);
  }

  // Returns the file's extension (ex "/moose/fish/foo.txt" would return ".txt")
  String getExtension() const {
    String filename = getFileName(); 
    int idx = (int)filename.rfind("."); 
    return (idx < 0) ? "" : filename.substr(idx);
  }


private:

  //internal path
  String path;

  //internal utility
  static const String normalizePath(String path);

  //isGoodNormalizedPath 
  static bool isGoodNormalizedPath(String path);


}; //end class

///////////////////////////////////////////////////////////////////////
class VISUS_KERNEL_API KnownPaths
{
public:
  static String VisusHome;
  static String BinaryDirectory;
  static String CurrentWorkingDirectory();
private:
  KnownPaths()=delete;
};


} //namespace Visus


#endif //__VISUS_PATH_H__


