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

#ifndef VISUS_KD_ARRAY_H
#define VISUS_KD_ARRAY_H

#include <Visus/Kernel.h>
#include <Visus/Array.h>
#include <Visus/CriticalSection.h>

namespace Visus {

////////////////////////////////////////////////////////////
class VISUS_KERNEL_API KdArrayNode
{
public:

  VISUS_NON_COPYABLE_CLASS(KdArrayNode)

  // id (root has id 1), this is basically the block number
  BigInt id = 0;

  //up
  KdArrayNode* up = nullptr;

  // logic_box
  BoxNi logic_box;

  //level
  int level=0;

  // resolution
  int resolution=0;             

  //if it has childs, what is the split bit
  int split_bit=-1;

  //childs
  SharedPtr<KdArrayNode> left,right;

  //buffers (internals only)
  Array                fullres;
  Array                blockdata;

  bool                 bDisplay=false;
  Array                displaydata;
  SharedPtr<DynObject> texture;

  //default constructor
  KdArrayNode(BigInt blocknum_=0) : id(blocknum_){
    VisusAssert(id >= 0);
  }

  //constructor
  KdArrayNode(BigInt blocknum_, KdArrayNode* up_ ) : id(blocknum_), up(up_),level(up_->level+1), resolution(up_->resolution+1) {
    VisusAssert(id >= 0);
  }

  //destructor
  ~KdArrayNode() {
  }

  //getLogicMiddle
  Int64 getLogicMiddle() {
    return (this->logic_box.p1[split_bit]+this->logic_box.p2[split_bit])>>1;
  }

  //if leaf or not
  bool isLeaf() const
  {
    //must have both childs or no childs
    VisusAssert((this->left && this->right) || (!this->left && !this->right));
    return !this->left && !this->right;
  }

  //c_size (estimation of buffers occupancy)
  Int64 c_size() const
  {
    return 
      blockdata.c_size()
      + ((fullres.valid() && fullres.heap!=blockdata.heap) ? fullres.c_size() : 0)
      + ((displaydata.valid() && displaydata.heap!=blockdata.heap && displaydata.heap!=fullres.heap) ? displaydata.c_size() : 0);
  }

};


//////////////////////////////////////////////
class VISUS_KERNEL_API KdArray 
{
public:

  VISUS_NON_COPYABLE_CLASS(KdArray)

  //read/write lock
#if !SWIG
  RWLock lock;  
#endif

  //only for kd queries
  SharedPtr<KdArrayNode> root;

  //logic_box (could be less that dataset pow2 logic box), typically is the user queried box
  BoxNi logic_box;

  //clipping
  Position clipping;

  //bounds
  Position bounds;

  //constructor
  KdArray(int pdim=0);

  //destructor
  virtual ~KdArray();

  //getPointDim
  int getPointDim() const {
    return pdim;
  }

  //clearChilds
  void clearChilds(KdArrayNode* node);

  //split
  void split(KdArrayNode* node,int split_bit);

  //isNodeVisible
  bool isNodeVisible(KdArrayNode* node) const {
    return node->logic_box.strictIntersect(this->logic_box);
  }

  //enableCaching
  void enableCaching(int cutoff,Int64 fine_maxmemory);

private:

  //in case you need some caching
  class SingleCache;
  class MultiCache;
  SharedPtr<MultiCache> cache;

  int pdim;

  void onNodeEnter(KdArrayNode*);
  void onNodeExit (KdArrayNode*);


};

} //namespace Visus

#endif //VISUS_KD_ARRAY_H

