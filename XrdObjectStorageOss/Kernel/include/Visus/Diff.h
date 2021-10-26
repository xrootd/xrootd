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

#ifndef VISUS_DIFF_H__
#define VISUS_DIFF_H__

#include <Visus/Kernel.h>
#include <Visus/Model.h>

#include <string>
#include <vector>

namespace Visus {

//////////////////////////////////////////////////////////
class VISUS_KERNEL_API Diff 
{
public:

  VISUS_CLASS(Diff)

  //constructor
  Diff() {
  }

  //constructor
  Diff(const std::vector<String>& patch);

  //constructor
  Diff(const std::vector<String>& before,const std::vector<String>& after);

  //destructor
  ~Diff() {
  }

  //clear
  void clear() {
    elements.clear();
  }

  //size
  int size() const{
    return (int)elements.size();
  }

  //empty
  bool empty() const{
    return elements.empty();
  }

  //inverted
  Diff inverted() const;

  //applyDirect
  std::vector<String> applyDirect(const std::vector<String>& before) const;

  //applyInverse
  std::vector<String> applyInverse(const std::vector<String>& after) const {
    auto before=inverted().applyDirect(after);
    return before;
  }


  //toString
  String toString() const;

private:


  //_________________________________________
  class TypedString
  {
  public:
    String s;
    char   type; //'+' '-' ' '
    inline TypedString(String s_="",char type_=' '):s(s_),type(type_) {}
  };

  //_________________________________________
  class Element 
  {
  public:
    int a=0, b=0, c=0, d=0;  
    std::vector<TypedString> v;

    Element inverted() const
    {
      Element ret=*this;
      ret.a=this->c;
      ret.b=this->d;
      ret.c=this->a;
      ret.d=this->b;
      for (int I=0;I<(int)this->v.size();I++)
      {
        switch (this->v[I].type) 
        {
          case '+': ret.v[I]=TypedString(this->v[I].s,'-');break;
          case '-': ret.v[I]=TypedString(this->v[I].s,'+');break;
          case ' ': ret.v[I]=TypedString(this->v[I].s,' ');break;
          default: VisusAssert(false);return *this;
        }
      } 
      return ret;
    }
  };

  std::vector<Element> elements;

};


} //namespace Visus

#endif //VISUS_MVC_H__


