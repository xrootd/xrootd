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

#ifndef __VISUS_FIELD_H
#define __VISUS_FIELD_H

#include <Visus/Kernel.h>
#include <Visus/DType.h>

namespace Visus {

////////////////////////////////////////////////////////
class VISUS_KERNEL_API Field 
{
public:

  VISUS_CLASS(Field)

  // field name (example TEMPERATURE) 
  String name;

  // dtype
  DType dtype; 

  //description
  String description;

  //index of the field (needed byd idxfile)
  String index;

  // name of compression (for storage)
  String default_compression;

  // default_layout
  String default_layout;

  //default_value
  int default_value=0;

  //filter
  String filter;

  //params
  StringMap params;

  //constructor
  Field(String name_ ="", DType dtype_ = DType(),String default_layout_="") :name(name_), dtype(dtype_), default_layout(default_layout_){
  }

  //constructor
  Field(String name, String dtype, String default_layout = "") : Field(name,DType::fromString(dtype),default_layout) {
  }

  //fromString
  static Field fromString(String src);

  //valid
  inline bool valid() const{
    return this->dtype.valid();
  }

  //getDescription
  inline String getDescription(bool bUseNameIfEmpty = true) const{
    return description.empty() ? (bUseNameIfEmpty ? name : "") : description;
  }

  //setDescription
  inline void setDescription(String value) {
    this->description = value; 
  }

  //hasParam
  inline bool hasParam(String key) const{
    return params.hasValue(key);
  }

  //getParam
  inline String getParam(String key, String default_value = "") const{
    return params.getValue(key, default_value);
  }

  //getDTypeRange
  Range getDTypeRange(int component=0) const 
  {
    VisusAssert(component>=0 && component<=dtype.ncomponents());
    return this->dtype.getDTypeRange(component);
  }

  //setDTypeRange
  void setDTypeRange(Range value,int component=0) 
  {
    VisusAssert(component>=0 && component<=dtype.ncomponents());
    this->dtype=this->dtype.withDTypeRange(value,component);
  }

public:

  //write
  void write(Archive& ar) const;

  //read
  void read(Archive& ar) ;

};


} //namespace Visus

#endif //__VISUS_FIELD_H

