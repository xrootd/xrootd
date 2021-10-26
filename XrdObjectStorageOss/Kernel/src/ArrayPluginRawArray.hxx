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


#ifndef VISUS_ARRAY_PLUGIN_RAWARRAY_H
#define VISUS_ARRAY_PLUGIN_RAWARRAY_H

#include <Visus/Array.h>
#include <Visus/File.h>

namespace Visus {


///////////////////////////////////////////////////////////////////////////////
class VISUS_KERNEL_API RawArrayPlugin : public ArrayPlugin
{
public:

  VISUS_NON_COPYABLE_CLASS(RawArrayPlugin)

    std::set<String> extensions = std::set<String>{ ".raw",".bin",".brick",".dat" };

  //constructor
  RawArrayPlugin() {
  }

  //destructor
  virtual ~RawArrayPlugin() {
  }

  //handleStatImage
  virtual StringTree handleStatImage(String url_) override
  {
    Url url(url_);

    if (!url.isFile())
      return StringTree();

    String filename = url.getPath();

    String ext = Path(filename).getExtension();

    if (extensions.find(ext) == extensions.end())
      return StringTree();

    Int64 filesize = FileUtils::getFileSize(Path(filename));
    if (filesize <= 0)
      return StringTree();

    StringTree ret("info");
    ret.write("format", "RAW");
    ret.write("url", url_);
    ret.write("filesize", filesize);

    return ret;
  }

  //handleLoadImage
  virtual Array handleLoadImage(String url_, std::vector<String> args) override
  {
    Url url(url_);

    if (!url.isFile())
      return Array();

    String filename = url.getPath();

    String ext = Path(filename).getExtension();

    if (extensions.find(ext) == extensions.end())
      return Array();

    DType      dtype;
    Int64      offset = 0;
    PointNi    dims;

    for (int I = 0; I<(int)args.size(); I++)
    {
      if (args[I] == "--dtype")
      {
        String sdtype = args[++I];
        dtype = DType::fromString(sdtype);
        if (!dtype.valid())
        {
          PrintWarning("invalid --dtype",sdtype);
          return Array();
        }
      }
      else if (args[I] == "--dims")
      {
        dims = PointNi::fromString(args[++I]);
      }
      else if (args[I] == "--offset")
      {
        offset = cint64(args[++I]);
      }
    }

    if (!dtype.valid())
    {
      PrintWarning("please use --dtype for RawArrayPlugin");
      return Array();
    }

    if (dims.innerProduct() <= 0)
    {
      PrintWarning("please use --dims for RawArrayPlugin");
      return Array();
    }

    //try to open the binary file
    File file;
    if (!file.open(filename, "r"))
    {
      PrintWarning("file.open(",filename,",\"rb\") failed",filename);
      return Array();
    }

    Array dst;
    if (!dst.resize(dims, dtype, __FILE__, __LINE__))
      return Array();

    if (!file.read(offset, dst.c_size(), dst.c_ptr()))
    {
      PrintWarning("file.read failed for file",filename);
      return Array();
    }

    return dst;
  }

  //handleSaveImage
  virtual bool handleSaveImage(String url_, Array src, std::vector<String> args) override
  {
    Url url(url_);

    if (!url.isFile())
      return false;

    String filename = url.getPath();

    String ext = Path(filename).getExtension();

    if (extensions.find(ext) == extensions.end())
      return false;

    FileUtils::removeFile(filename);

    File file;
    if (!file.createAndOpen(filename, "w"))
    {
      PrintWarning("RawArrayPlugin::handleSaveImage ERROR, failed to file.open(",filename,",\"wb\")");
      return false;
    }

    Int64 offset = 0;
    for (int I = 0; I<(int)args.size(); I++)
    {
      if (args[I] == "--offset")
      {
        offset = cint64(args[++I]);
      }
    }

    Int64 tot = src.getTotalNumberOfSamples();
    if (tot <= 0)
      return false;

    if (!file.write(offset, src.c_size(), src.c_ptr()))
    {
      PrintWarning("write error on file",filename);
      return false;
    }

    PrintInfo("saved image",filename);
    return true;
  }
};


} //namespace Visus

#endif //VISUS_ARRAY_PLUGIN_RAWARRAY_H

