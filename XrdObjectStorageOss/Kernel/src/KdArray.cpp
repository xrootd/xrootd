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

#include <Visus/KdArray.h>

#include <list>

namespace Visus {

//////////////////////////////////////////////
class KdArray::SingleCache
{
public:

  class Cached
  {
  public:
    BigInt           id =0;
    Int64            c_size=0;
    Array            blockdata;
    Array            fullres;
    Array            displaydata;
  };

  Int64                                          max_memory=0;
  Int64                                          used_memory=0;
  std::list<Cached>                              list;
  std::map< BigInt, std::list<Cached>::iterator> map;

  //constructor
  SingleCache() {
  }

  //contains
  bool contains(KdArrayNode* node) const {
    return map.find(node->id)!=map.end();
  }

  //push
  void push(KdArrayNode* node)
  {
    //PrintInfo("push",max_memory?"fine":"coarse",node->id);
    VisusAssert(!contains(node));

    Cached cached;
    cached.c_size=node->c_size();
    if (!cached.c_size) return; //nothing to cache

    cached.id             = node->id;
    cached.blockdata      = node->blockdata;
    cached.fullres        = node->fullres;
    cached.displaydata    = node->displaydata;

    //there is a limit
    if (max_memory>0)
    {
      while ((cached.c_size+used_memory)>=(max_memory))
      {
        auto it=list.end();--it;
        used_memory-=it->c_size;
        map.erase(it->id);
        list.erase(it);
      }
    }

    list.push_front(cached);
    map[cached.id]=list.begin();
    used_memory+=cached.c_size;
  }

  //pop
  void pop(KdArrayNode* node)
  {
    //PrintInfo("pop",max_memory?"fine":"coarse",node->id);

    auto it=map.find(node->id);

    //not found
    if (it==map.end()) 
      return; 

    const Cached& cached=*(it->second);

    VisusAssert(cached.id ==node->id);
    node->blockdata   = cached.blockdata;
    node->fullres     = cached.fullres;
    node->displaydata = cached.displaydata;

    used_memory-=cached.c_size;
    list.erase(it->second);
    map.erase(it);
    VisusAssert(!contains(node));
  }
};

//////////////////////////////////////////////
class KdArray::MultiCache
{
public:

  int         cutoff;
  SingleCache coarse_cache;
  SingleCache fine_cache;

  //constructor
  MultiCache() : cutoff(0) {
  }

  //push
  void push(KdArrayNode* node) {
    (node->level < cutoff ? coarse_cache : fine_cache).push(node);
  }

  //pop
  void pop(KdArrayNode* node) {
    (node->level < cutoff ? coarse_cache : fine_cache).pop(node);
  }
};


////////////////////////////////////////////////////////////////
KdArray::KdArray(int pdim_) : pdim(pdim_) 
{}

////////////////////////////////////////////////////////////////
KdArray::~KdArray()
{}

////////////////////////////////////////////////////////////////
void KdArray::onNodeEnter(KdArrayNode* node)
{
  if (cache)  cache->pop(node);
  if (node->left ) onNodeEnter (node->left.get ());
  if (node->right) onNodeEnter (node->right.get()); //IMPORTANT THE ORDER... node and then childs
}


////////////////////////////////////////////////////////////////
void KdArray::onNodeExit(KdArrayNode* node)
{
  if (node->left ) onNodeExit(node->left.get ()); //IMPORTANT THE ORDER... childs and then node
  if (node->right) onNodeExit(node->right.get());
  if (cache)  cache->push(node);
}

////////////////////////////////////////////////////////////////
void KdArray::clearChilds(KdArrayNode* node)
{
  if (node->left ) onNodeExit(node->left.get ());
  if (node->right) onNodeExit(node->right.get());
  node->left .reset();
  node->right.reset();
}

////////////////////////////////////////////////////////////////
void KdArray::split(KdArrayNode* node,int split_bit)
{
  VisusAssert(split_bit>=0);
  VisusAssert(node->isLeaf());
  VisusAssert(root->id ==1);

  node->split_bit = split_bit;

  node->left=std::make_shared<KdArrayNode>(node->id * 2 + 0, node);
  node->left->logic_box = node->logic_box;
  node->left->logic_box.p2[node->split_bit]=node->getLogicMiddle();

  node->right=std::make_shared<KdArrayNode>(node->id * 2 + 1, node);
  node->right->logic_box = node->logic_box;
  node->right->logic_box.p1[node->split_bit]=node->getLogicMiddle();

  onNodeEnter(node->left .get());
  onNodeEnter(node->right.get());
}


////////////////////////////////////////////////////////////////
void KdArray::enableCaching(int cutoff,Int64 fine_maxmemory)
{
  if (cache) return;
  cache=std::make_shared<MultiCache>();
  cache->cutoff=cutoff;
  cache->coarse_cache.max_memory=0;
  cache->fine_cache.max_memory=fine_maxmemory;
}


} //namespace Visus


