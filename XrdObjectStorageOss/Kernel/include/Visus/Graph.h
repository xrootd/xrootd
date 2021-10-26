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

#ifndef VISUS_GRAPH_H_
#define VISUS_GRAPH_H_

#include <Visus/Kernel.h>
#include <Visus/Sphere.h>
#include <Visus/Position.h>
#include <Visus/CriticalSection.h>
#include <Visus/StringMap.h>

#include <iostream>

namespace Visus {

//#define VISUS_DEBUG_GRAPH 1

/////////////////////////////////////////////
template<class vT=int>
class GraphNode
{
public:

  bool deleted;
  vT   data;

#if !SWIG
  std::vector<int> in;
  std::vector<int> out;
#endif

  //constructor
  GraphNode()
    #ifdef VISUS_DEBUG_GRAPH
    : deleted(true)
    #endif
  {}

  //constructor
  inline GraphNode(vT d) : deleted(false),data(d) 
  {}

  //constructor
  inline GraphNode(const GraphNode<vT> &g) : deleted(g.deleted),data(g.data)
  {
    in .resize(g.in .size());for (int i=0;i<(int)g.in .size();i++) in [i]=g.in [i]; 
    out.resize(g.out.size());for (int i=0;i<(int)g.out.size();i++) out[i]=g.out[i]; 
  }

  //destructor
  inline ~GraphNode()
  {
    #ifdef VISUS_DEBUG_GRAPH
    this->deleted=true;
    in.clear();
    out.clear();
    #endif
  }

  //shallow copy, so better to use concrete types and not pointers for vT and eT
  inline GraphNode<vT>& operator=(const GraphNode<vT> &n) 
  {
    if (&n!=this)
    {
      this->deleted=n.deleted;
      this->data=n.data;
      this->in .resize(n.in.size());for (int i=0;i<(int)n.in .size();i++) in [i]=n.in [i];
      this->out.resize(n.in.size());for (int i=0;i<(int)n.out.size();i++) out[i]=n.out[i];
    }
    return *this;
  }

  //in_degree
  inline int in_degree() const
  {
    #ifdef VISUS_DEBUG_GRAPH
    {
      int cnt=0;
      for (int i=0;i<(int)in.size();i++) 
        cnt+=(int)(in[i]>=0); 
      VisusAssert(cnt==(int)in.size());
    }
    #endif
    return (int)in.size();
  }

  //out_degree
  inline int out_degree() const
  {
    #ifdef VISUS_DEBUG_GRAPH
    {
      int cnt=0;
      for (int i=0;i<(int)out.size();i++) 
        cnt+=(int)(out[i]>=0); 
      VisusAssert(cnt==(int)out.size());
    }
    #endif
    return (int)out.size();
  }

  //get_in_edge
  inline int get_in_edge(int e) const
  {
    for (int i=0;i<(int)in.size();i++) 
      if (this->in[i]==e) return i; 
    return -1; 
  }

  //get_out_edge
  inline int get_out_edge(int e) const
  {
    for (int i=0;i<(int)out.size();i++) 
      if (this->out[i]==e) return i; 
    return -1; 
  }

  //add_in_edge
  inline void add_in_edge(int e)
    {this->in.push_back(e);}
  
  //add_out_edge
  inline void add_out_edge(int e)
    {this->out.push_back(e);}
  
  //del_in_edge
  inline void del_in_edge(int e)       
  {
    //find edge
    int eidx=this->get_in_edge(e);
    VisusAssert(eidx>=0);
    int i=eidx;

    //shift elements
    for (;i<(int)in.size()-1&&this->in[i]>=0;i++)
      this->in[i]=this->in[i+1];

    //reduce size of in vector
    this->in.pop_back();
  }

  //del_out_edge
  inline void del_out_edge(int e)       
  {
    //find edge
    int eidx=this->get_out_edge(e);
    VisusAssert(eidx>=0);
    int i=eidx;

    //shift elements
    for (;i<(int)out.size()-1&&this->out[i]>=0;i++)
      this->out[i]=this->out[i+1];

    //reduce size of in vector
    this->out.pop_back();
  }

};



/////////////////////////////////////////////
template<class eT=int>
class GraphEdge 
{
public:

  bool deleted; 
  int  src, dst; 
  eT   data;

public:

  //constructor
  inline GraphEdge() 
    #ifdef VISUS_DEBUG_GRAPH
    : deleted(true),src(-1),dst(-1),data(0)
    #endif
  {}

  //constructor
  inline GraphEdge(int s,int d,eT w) : deleted(false),src(s),dst(d),data(w)
  {}

  //constructor
  inline GraphEdge(const GraphEdge &e) : deleted(e.deleted),src(e.src),dst(e.dst),data(e.data)
  {}

  //destructor
  inline ~GraphEdge()
  {
    #ifdef VISUS_DEBUG_GRAPH
    this->deleted=true;
    this->src=-2; this->dst=-2;
    #endif
  }

  //shallow copy, so better to use concrete types and not pointers for vT and eT
  GraphEdge& operator=(const GraphEdge &e)
  {
    if (&e!=this)
    {
      this->deleted=e.deleted;
      this->src=e.src;
      this->dst=e.dst;
      this->data=e.data;
    }
    return *this;
  }
};

/////////////////////////////////////////////
class VISUS_KERNEL_API BaseGraph
{
public:

  //constructor
  BaseGraph() {
  }

  //destructor
  virtual ~BaseGraph() {
  }
};

/////////////////////////////////////////////
template<class vT_=int, class eT_=int>
class Graph : public BaseGraph
{
public:

  typedef vT_ vT;
  typedef eT_ eT;

  CriticalSection lock;
  
  typedef GraphNode<vT>                  Vertex;
  typedef std::vector<Vertex>            Verts;
  typedef typename Verts::iterator       VertIter;
  typedef typename Verts::const_iterator CVertIter;

  typedef GraphEdge<eT>                  Edge;
  typedef std::vector<Edge>              Edges;              
  typedef typename Edges::iterator       EdgeIter;
  typedef typename Edges::const_iterator CEdgeIter;

  // NOTE: Be sure to check if Edge or Vertex.deleted before using
  // vertex. Also TODO: implement propert iterator which skips these
  // elements and freeVerts to handle memory management.

  Position bounds;

  Verts verts;
  Edges edges;

  //in case you want to store certain stuff (example: minima_tree)
  StringMap properties;

  //constructor
  Graph() 
  { 
    this->verts.reserve(16384); 
    this->edges.reserve(16384); 
  }
  
  //copy constructor
  Graph(const Graph &other) 
  {this->operator=(other);}

  //destructor
  virtual ~Graph()
  {
  #ifdef VISUS_DEBUG_GRAPH
    clear();
  #endif
  }

  //clear
  inline void clear() 
  {
    verts.clear(); 
    edges.clear(); 
    bounds=Position::invalid();
  }

  //shallow copy, so better to use concrete types and not pointers for vT and eT
  Graph& operator=(const Graph &other)
  {
    if (this!=&other)
    {
      this->verts              = other.verts;
      this->edges              = other.edges;
      this->bounds             = other.bounds;
      this->properties         = other.properties;
    }
    return *this;
  }

  // returns index of vertex
  inline int mkVert(vT data)          
  {
    this->verts.push_back(Vertex(data));
    return (int)verts.size()-1;
  }

  // returns index of edge
  inline int mkEdge(int u,int v,eT w)  
  { 
    this->edges.push_back(Edge(u,v,w));
    int edgeidx=(int)this->edges.size()-1;
    this->verts[u].add_out_edge(edgeidx);
    this->verts[v].add_in_edge (edgeidx);
    return edgeidx;
  }

  // deletes vert and all connected edges
  inline void rmVert(int x)
  {
    /*
        A?  B?
         \ /
          X   ??
         / \ //
        P?  S?

    This simple version just deletes X and all its connected edges.
    */

    Vertex &X=this->verts[x];
    VisusAssert(!X.deleted);
    X.deleted =true;

    for (int k=0;k<X.in_degree();k++)
    {
      int ix=X.in[k];
      Edge &IX=this->edges[ix];
      int i=IX.src;
      Vertex &I=this->verts[i];
      VisusAssert(I.get_out_edge(ix)>=0);
      IX.deleted=true;
      I.del_out_edge(ix);
    }

    for (int k=0;k<X.out_degree();k++)
    {
      int ix=X.out[k];
      Edge &IX=this->edges[ix];
      int i=IX.dst;
      Vertex &I=this->verts[i];
      VisusAssert(I.get_in_edge(ix)>=0);
      IX.deleted=true;
      I.del_in_edge(ix);
    }
  }

  // for merge tree simplification, returns index of edge if
  // vert was reduced, 0 otherwise
  inline int rmVertSmart(int x)
  {
    /*
         ? ?
         / / ...
    X   P?Q?
     \ / / ...
      S  ?? 
      | //...
      R

    This rmVert is specialized for merge tree where only valid case is
    deleting a leaf (X) and automatically reduce S if it is a saddle.

    X   P?
     \ /
      S 

    MVCApplyPatch: allow deletion of verts which may connect to a root.
    Removes X and the root (S) if there is nothing else connected to it.
    */

    Vertex &X=this->verts[x];
    VisusAssert(X.in_degree()==0 && X.out_degree()==1);
    int xs=X.out[0];
    Edge &XS =this->edges[xs];
    VisusAssert(XS.src==x && XS.dst>=0);
    int s=XS.dst;
    Vertex &S=this->verts[s];
    VisusAssert(S.get_in_edge(xs)>=0);

    X.deleted =true;
    XS.deleted=true;
    #ifdef VISUS_DEBUG_GRAPH
    X.del_out_edge(xs);
    XS.src=-4;
    XS.dst=-4;
    VisusAssert(!S.deleted);
    #endif

    int Sid=S.in_degree();
    int Sod=S.out_degree();

    if (Sid==1 && Sod==0)              //S is a lonely root put to rest
      S.deleted=true;
    else if (Sid!=2 || (Sid==2 && Sod==0))//S is a leaf or still a saddle
      S.del_in_edge(xs);
    else                               //S is a interior node -> reduce it
    {
      int ps=-1;
      for (int i=0;i<(int)S.in.size();i++) 
      {
        VisusAssert(S.in[i]>=0);
        if (S.in[i]!=xs)
        {
          ps=S.in[i]; 
          break; 
        }
      }
      VisusAssert(ps!=-1);
      Edge &PS  =this->edges[ps];
      int p=PS.src;
      Vertex &P =this->verts[p];
      int sr=S.out[0];
      Edge &SR  =this->edges[sr];
      int r=SR.dst;
      Vertex &R =this->verts[r];

      S.deleted =true;
      PS.deleted=true;
      SR.deleted=true;
      P.del_out_edge(ps);
      R.del_in_edge(sr);
      return this->mkEdge(p,r,PS.data+SR.data);
    }

    return 0;
  }                                

};


/////////////////////////////////////////////
template<typename vT,typename eT>
std::ostream& operator<<(std::ostream &out,const Graph<vT,eT> &g)
{
  typedef typename Graph<vT,eT>::CVertIter citer_t;
  typedef typename Graph<vT,eT>::CEdgeIter eiter_t;

  for (citer_t it=g.verts.begin();it!=g.verts.end();it++)
    out << *it << std::endl;
  for (eiter_t it=g.edges.begin();it!=g.edges.end();it++)
    out << *it << std::endl;

  return out;
}

/////////////////////////////////////////////
template<typename vT>
std::ostream& operator<<(std::ostream &out,const GraphNode<vT> &v)
{
  out<<"out={";
  for (int i=0;i<(int)v.out.size();i++) out<<v.out[i]<<(i!=(v.out.size()-1)?",":"}");
  out<<",in={";
  for (int i=0;i<(int)v.in.size();i++) out<<v.in[i]<<(i!=(v.in.size()-1)?",":"}");
  return out;
}

/////////////////////////////////////////////
template<typename eT>
std::ostream& operator<<(std::ostream &out,const GraphEdge<eT> &e)
{
  out<<"src:"<<e.src<<",dst:"<<e.dst<<",data/weight:"<<(int)e.data;
  return out;
}


//////////////////////////////////////////////////////////////////////////////////////
class VISUS_KERNEL_API GraphUtils
{
public:

  //toIPoint
  template<class Type>
  static Point3i toIPoint(const Type *p, const Type *data, const Point3i &dims)
  {
    VisusAssert(p>=data && p<data+dims[0]*dims[1]*dims[2]);
    const Type *q  =(const Type*)(p   -  data);
    size_t   qz =(size_t)q   /    (dims[0]*dims[1]*sizeof(Type));
    size_t   qzr=(size_t)q   -  qz*dims[0]*dims[1]*sizeof(Type);
    size_t   qy =qzr         /     dims[0];
    size_t   qyr=qzr         %     dims[0];
    size_t   qx =qyr;
    // printf("p=%x,data=%x,q=p-data=%x(%u),dims=%s\n",p,data,q,q,dims.toString().c_str());
    // printf("ip=%s (qzr=%u)\n",Point3i(qx,qy,qz).toString().c_str(),qzr);
    VisusAssert(p==(data+qx+qy*dims[0]+qz*(dims[0]*dims[1])));
    return Point3i((int)qx,(int)qy,(int)qz);
  }

  //toPoint
  template<class Type>
  static Point3d toPoint(const Type *p, const Type *data, const Point3i &dims, const Point3d &scale, const Point3d &translate)
  {
    VisusAssert(p>=data && p<data+dims[0]*dims[1]*dims[2]);
    const Type *q  =(const Type*)(p   -  data);
    size_t   qz =(size_t)q   /    (dims[0]*dims[1]*sizeof(Type));
    size_t   qzr=(size_t)q   -  qz*dims[0]*dims[1]*sizeof(Type);
    size_t   qy =qzr         /     dims[0];
    size_t   qyr=qzr         %     dims[0];
    size_t   qx =qyr;
    // printf("p=%x,data=%x,q=p-data=%x(%u),dims=%s\n",p,data,q,q,dims.toString().c_str());
    // printf("ip=%s (qzr=%u)\n",Point3i(qx,qy,qz).toString().c_str(),qzr);
    VisusAssert(p==(data+qx+qy*dims[0]+qz*(dims[0]*dims[1])));
    return Point3d(translate[0]+qx*scale[0],translate[1]+qy*scale[1],translate[2]+qz*scale[2]);
  }

  //genDot
  template<class vT,class eT>
  static void genDot(const Graph<vT,eT> &g, vT data, const Point3i &dims, std::ostream &out, typename Graph<vT,eT>::Vertex *highlight=nullptr)
  {
    typedef typename Graph<vT,eT>::CVertIter citer_t;
    typedef typename Graph<vT,eT>::CEdgeIter eiter_t;

    out << "digraph G {" << std::endl;

    // vertices
    for (citer_t it=g.verts.begin();it!=g.verts.end();it++)
    {
      out << "\"" << *it << "\"" << " [label=\"" << *(*it)->data << "\\n(" << toIPoint(*it->data,data,dims).toString() << ")\"";
      int indeg =(*it)->in_degree();
      int outdeg=(*it)->out_degree();
      if (*it==highlight)
        out << ",style=filled,fillcolor=green,color=green,style=bold,peripheries=2]";

      else if (indeg == 0) 
        out << ",style=filled,fillcolor=red]";    //LEAF (local maxima)

      else if (outdeg == 0 && indeg>0)
        out << ",style=filled,fillcolor=blue]";   //ROOT (global minima)

      else if (indeg==1&&outdeg==1)
        out << ",style=filled,fillcolor=grey]";   //INTERNAL (normal)

      else if (indeg+outdeg>2)
        out << ",style=filled,fillcolor=yellow]"; //SADDLE (join)

      else
        out << "]";

      out << std::endl;
    }

    // edges
    for (eiter_t eit=g.edges.begin();eit!=g.edges.end();eit++)
      out << "\"" << *eit.src << "\"" << "->" << "\"" << *eit.dst << "\"\n";

    out << "}" << std::endl;
  }

private:

  //not allowed
  GraphUtils()=delete;
};


struct CGraphEdge
{
  Float32 length;
  CGraphEdge(Float32 length_) : length(length_) {}
};


//////////////////////////////////////////////
//some Graph templates
//////////////////////////////////////////////

typedef Graph < Int8*   ,Int8   > GraphInt8   ;
typedef Graph < Uint8*  ,Uint8  > GraphUint8  ;
typedef Graph < Int16*  ,Int16  > GraphInt16  ;
typedef Graph < Uint16* ,Uint16 > GraphUint16 ;
typedef Graph < Int32*  ,Int32  > GraphInt32  ;
typedef Graph < Uint32* ,Uint32 > GraphUint32 ;
typedef Graph < Int64*  ,Int64  > GraphInt64  ;
typedef Graph < Uint64* ,Uint64 > GraphUint64 ;
typedef Graph < Float32*,Float32> GraphFloat32;
typedef Graph < Float64*,Float64> GraphFloat64;

typedef Graph < Point3f ,Float32>     FGraph;
typedef Graph < Sphere,  CGraphEdge>  CGraph; //CGraph==CenterLine node has position and radius, edge has centerline id and euclidian length



} //namespace Visus

#endif //VISUS_GRAPH_H_


