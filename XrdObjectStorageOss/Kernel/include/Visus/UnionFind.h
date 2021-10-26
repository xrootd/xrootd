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

#ifndef VISUS_UNIONFIND_H_
#define VISUS_UNIONFIND_H_

#include <Visus/Kernel.h>

namespace Visus {

//-------------------------------------------------------------------------------
// UNION-FIND
//
// make(x)     O(1)        - create a component with x as its only member
// union(x,y)  O(alpha(n)) - connect the components containins x and y
// find(x)     O(alpha(n)) - find the component containing x
//
// NOTE: Runtime complexity is amortized over all calls.
//       alpha(n) is the inverse Ackerman function which grows very slowly... 
//       probably its value will always be less than 5, so "nearly constant".
//
//-------------------------------------------------------------------------------
template<class Type>
class UnionFind
{
protected:

  // maps an element to its set's representative
  std::map<Type,Type>   reps; 

  // rank (depth) of a set
  std::map<Type,int>    rank; 

public:

  //constructor
  UnionFind()  
    {};
  
  //destructor
  ~UnionFind() 
    {};

  //create a set with x as its sole member
  inline void make_set(Type x) 
  {
    reps[x]=x;
    rank[x]=0;
  }

  //return the representative element of the set containing x
  //assumes make_set(x) has already been called
  inline Type find_set(Type x)
  {
    if (reps[x]!=x)
      reps[x]=this->find_set(reps[x]);
    return reps[x];
  }

  //return the representative element of the set containing x
  //doesn't assume that x is already added, but does assume Type(0) is possible
  inline Type find_set_safe(Type x) 
  {
    if (reps.find(x)==reps.end()) return Type(0);
    if (reps[x]!=x)
      reps[x]=this->find_set(reps[x]);
    return reps[x];
  }

  //join the sets containing x and y
  inline Type union_set(Type x, Type y)
  {
    Type xp=find_set(x);
    Type yp=find_set(y);
    return this->link(xp,yp);
  }

  //shortcut for union_set when find_set(x)==x and find_set(y)==y
  inline Type link(Type x, Type y)
  {
    if (rank[x]<rank[y])
      reps[x]=y; // x's component is now represented by y
    else if (x != y)
    {
      reps[y]=x; // y's component is now represented by x
      if (rank[x]==rank[y])
        ++rank[x];
      return x;
    }
    return y;
  }

  //delete all components
  inline void clear()
  {
    reps.clear();
    rank.clear();
  }

private:

  VISUS_NON_COPYABLE_CLASS(UnionFind)

};

} //namespace Visus

#endif //VISUS_UNIONFIND_H_


