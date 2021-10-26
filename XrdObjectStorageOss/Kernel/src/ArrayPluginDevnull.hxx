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

#ifndef VISUS_ARRAY_PLUGIN_DEVNULL_H
#define VISUS_ARRAY_PLUGIN_DEVNULL_H

#include <Visus/Kernel.h>
#include <Visus/Array.h>

namespace Visus {


///////////////////////////////////////////////////////////////////////////////
class VISUS_KERNEL_API DevNullArrayPlugin : public ArrayPlugin
{
public:

  VISUS_NON_COPYABLE_CLASS(DevNullArrayPlugin)

    //constructor
    DevNullArrayPlugin() {
  }

  //destructor
  virtual ~DevNullArrayPlugin() {
  }

  //handleLoadImage
  virtual Array handleLoadImage(String url_, std::vector<String> args) override
  {
    Url url(url_);

    if (!url.isFile())
      return Array();

    String filename = url.getPath();

    if (filename != "/dev/null")
      return Array();

    DType    dtype;
    PointNi  dims;
    int      value = 0;

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
        if (dims.innerProduct() <= 0)
        {
          PrintWarning("invalid --dims", args[I]);
          return Array();
        }
      }
      else if (args[I] == "--value")
      {
        value = cint(args[I]);
      }
    }

    Array dst;
    if (!dst.resize(dims, dtype, __FILE__, __LINE__))
    {
      PrintWarning("Cannot resize memory with dims", dims, "and dtype", dtype);
      return Array();
    }

    dst.fillWithValue(value);
    return dst;
  }

  //handleSaveImage
  virtual bool handleSaveImage(String url, Array src, std::vector<String> args) override {
    return Url(url).isFile() && Url(url).getPath() == "/dev/null";
  }

};

} //namespace Visus

#endif //VISUS_ARRAY_PLUGIN_DEVNULL_H
