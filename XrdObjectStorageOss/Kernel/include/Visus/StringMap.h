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

#ifndef VISUS_STRINGMAP_H__
#define VISUS_STRINGMAP_H__

#include <Visus/Kernel.h>

#include <map>

namespace Visus {

////////////////////////////////////////////////////////////////////////
class VISUS_KERNEL_API StringMap
{
public:

  VISUS_CLASS(StringMap)

  typedef std::map<String,String> Map;
  typedef Map::iterator       iterator;
  typedef Map::const_iterator const_iterator;

  //constructor
  StringMap(){
  }

  //destructor
  ~StringMap(){
  }

  //clear
  inline void clear() {
    values.clear();
  }

  //size
  inline int size() const {
    return (int)values.size();
  }

  //empty
  inline bool empty() const {
    return values.empty();
  }

  //iterators
  inline iterator       begin()       {return values.begin();}
  inline const_iterator begin() const {return values.begin();}
  inline iterator       end  ()       {return values.end  ();}
  inline const_iterator end  () const {return values.end  ();}

  //find
  inline iterator find(const String& key)  {
    return values.find(key);
  }

  //find
  inline const_iterator find(const String& key) const {
    return values.find(key);
  }

  //hasValue
  inline bool hasValue(const String& key) const {
    return values.find(key)!=values.end();
  }

  //getValue
  inline String getValue(const String& key,String default_value="") const {
    auto it=values.find(key);return it!=values.end()? it->second : default_value;
  }

  //setValue
  inline void setValue(const String& key,const String& value) {
    values[key]=value;
  }

  //eraseValue
  inline void eraseValue(const String& key) {
    values.erase(key);
  }

  //operator==
  bool operator==(const StringMap& other) const {
    return values == other.values;
  }

  //operator!=
  bool operator!=(const StringMap& other) const {
    return values != other.values;
  }

private:

  Map values;

};

} //namespace Visus

#endif //VISUS_STRINGMAP_H__

