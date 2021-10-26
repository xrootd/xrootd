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

#ifndef VISUS_CLOUD_STORAGE_
#define VISUS_CLOUD_STORAGE_

#include <Visus/Kernel.h>
#include <Visus/NetService.h>

#include <vector>

namespace Visus {


  ////////////////////////////////////////////////////////////////////////////////
class VISUS_KERNEL_API CloudStorageItem
{
public:

  String                 fullname;
  StringMap              metadata;
  bool                   is_directory = false;

  //is_directory==false
  SharedPtr<HeapMemory>  body;

  //is_directory==true
  std::vector< SharedPtr<CloudStorageItem> > childs;

  //costructor
  CloudStorageItem() {
  }

  //valid
  bool valid() const {
    return !fullname.empty();
  }

  //constructor
  static SharedPtr<CloudStorageItem> createBlob(String fullname, SharedPtr<HeapMemory> body = SharedPtr<HeapMemory>(), StringMap metadata = StringMap()) {
    auto ret = std::make_shared<CloudStorageItem>();
    ret->fullname = fullname;
    ret->metadata = metadata;
    ret->is_directory = false;
    ret->body = body;
    return ret;
  }

  //constructor
  static SharedPtr<CloudStorageItem> createDir(String fullname, StringMap metadata = StringMap()) {
    auto ret = std::make_shared<CloudStorageItem>();
    ret->fullname = fullname;
    ret->metadata = metadata;
    ret->is_directory = true;
    return ret;
  }

  //getContentLength
  Int64 getContentLength() const {
    return is_directory? 0 : (body? body->c_size() : cint64(metadata.getValue("Content-Length")));
  }

  //setContentLength
  void setContentLength(Int64 value) {
    metadata.setValue("Content-Length", cstring(value));
  }

  //getContentType
  String getContentType() const {
    return is_directory? "" : metadata.getValue("Content-Type", "application/octet-stream");
  }

  //setContentType
  void setContentType(String value) {
    metadata.setValue("Content-Type", value);
  }

  //findChild
  SharedPtr<CloudStorageItem> findChild(String fullname) {
    VisusAssert(is_directory);
    for (auto child : childs)
      if (child->fullname == fullname)
        return child;
    return SharedPtr<CloudStorageItem>();
  }

  //hasChild
  bool hasChild(String fullname) {
    return findChild(fullname) ? true : false;
  }


};

////////////////////////////////////////////////////////////////////////////////
class VISUS_KERNEL_API CloudStorage 
{
public:

  VISUS_CLASS(CloudStorage)

  //constructor
  CloudStorage(){
  }

  //destructor
  virtual ~CloudStorage() {
  }

  //possible return types: empty(i.e. invalid) azure gcs s3
  static String guessType(Url url);

  //createInstance
  static SharedPtr<CloudStorage> createInstance(Url url);

public:

  //addBucket
  virtual Future<bool> addBucket(SharedPtr<NetService> net, String bucket, Aborted aborted = Aborted()) = 0;

  //deleteBucket
  virtual Future<bool> deleteBucket(SharedPtr<NetService> net, String bucket, Aborted aborted = Aborted())=0;

public:

  //getDir
  virtual Future< SharedPtr<CloudStorageItem> > getDir(SharedPtr<NetService> net, String fullname, Aborted aborted = Aborted()) {
    ThrowException("not implemented");
    return Promise< SharedPtr<CloudStorageItem> >().get_future();
  }


public:

  //addBlob
  virtual Future<bool> addBlob(SharedPtr<NetService> net, SharedPtr<CloudStorageItem> blob, Aborted aborted = Aborted())=0;

  //getBlob
  virtual Future< SharedPtr<CloudStorageItem> > getBlob(SharedPtr<NetService> net, String name, bool head=false, Aborted aborted = Aborted()) = 0;

  // deleteBlob
  virtual Future<bool> deleteBlob(SharedPtr<NetService> net, String name, Aborted aborted = Aborted()) = 0;

};

} //namespace Visus


#endif //VISUS_CLOUD_STORAGE_




