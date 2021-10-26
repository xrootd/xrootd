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

#include <Visus/Diff.h>
#include <Visus/StringUtils.h>

#include <dtl/variables.hpp>
#include <dtl/functors.hpp>
#include <dtl/Sequence.hpp>
#include <dtl/Lcs.hpp>
#include <dtl/Ses.hpp>
#include <dtl/Diff.hpp>
#include <dtl/Diff3.hpp>

namespace Visus {


/////////////////////////////////////////////////////////////////
Diff::Diff(const std::vector<String>& A,const std::vector<String>& B)
{
  this->clear();

  dtl::Diff<String> d(A, B);
  d.compose();
  d.composeUnifiedHunks();

  std::vector< dtl::uniHunk< std::pair<String,dtl::elemInfo> > > hunks=d.getUniHunks();

  for (int I=0;I<(int)hunks.size();I++)
  {
    dtl::uniHunk< std::pair<String,dtl::elemInfo> >& hunk=hunks[I];
    Diff::Element element;
    element.a=(int)hunk.a;
    element.b=(int)hunk.b;
    element.c=(int)hunk.c;
    element.d=(int)hunk.d;

    std::vector< std::pair<String,dtl::elemInfo> >* vectors[3]={&hunk.common[0],&hunk.change,&hunk.common[1]};
    for (int I=0;I<3;I++)
    {
      for (int N=0;N<(int)vectors[I]->size();N++)
      {
        String s=(*vectors[I])[N].first;
        int type=(*vectors[I])[N].second.type;
        switch (type)
        {
          case dtl::SES_ADD    : element.v.push_back(Diff::TypedString(s,'+')); break;
          case dtl::SES_DELETE : element.v.push_back(Diff::TypedString(s,'-')); break;
          case dtl::SES_COMMON : element.v.push_back(Diff::TypedString(s,' ')); break;
        }
      }
    }
    this->elements.push_back(element);
  }
}



////////////////////////////////////////////////////////////
Diff::Diff(const std::vector<String>& patch) 
{
  for (auto line : patch)
  {
    if (StringUtils::startsWith(line,"@@"))
    {
      Element element;
      
      char ch;
      std::istringstream in(line);
      in>>std::skipws>>ch; VisusAssert(ch=='@');
      in>>std::skipws>>ch; VisusAssert(ch=='@');
      in>>std::skipws>>ch; VisusAssert(ch=='-');
      in>>std::skipws>>element.a ; 
      in>>std::skipws>>ch;VisusAssert(ch==',');
      in>>std::skipws>>element.b ; 
      in>>std::skipws>>ch;VisusAssert(ch=='+');
      in>>std::skipws>>element.c ; 
      in>>std::skipws>>ch;VisusAssert(ch==',');
      in>>std::skipws>>element.d ; 
      in>>std::skipws>>ch; VisusAssert(ch=='@');
      in>>std::skipws>>ch; VisusAssert(ch=='@');
      elements.push_back(element);
    }
    else
    {
      VisusAssert(!elements.empty());
      Element& element=elements.back();
      TypedString typed_string;
      typed_string.type=line[0]; VisusAssert(typed_string.type==' ' || typed_string.type=='+' || typed_string.type=='-');
      typed_string.s=line.substr(1);
      element.v.push_back(typed_string);
    }
  }
}



/////////////////////////////////////////////////////////////////
std::vector<String> Diff::applyDirect(const std::vector<String>& A) const
{
  int  inc_dec_total = 0;
  int  gap           = 1;

  std::list<String>  list(A.begin(), A.end());
            
  auto lt = list.begin();
  for (int I=0;I<(int)this->size();I++) 
  {
    const Diff::Element& element=this->elements[I];

    int inc_dec_count=0;
    for (int N=0;N<(int)element.v.size();N++)
    {
      switch (element.v[N].type)
      {
        case '+': ++inc_dec_count;break;
        case '-': --inc_dec_count;break;
        case ' ': break;
      }
    }

    int a=element.a+inc_dec_total;
    int b=element.b;
    inc_dec_total += inc_dec_count;
              
    for (int i=0;i<a-gap;++i) 
      ++lt;

    gap = a + b + inc_dec_count;
              
    for (int N=0;N<(int)element.v.size();N++)
    {
      switch (element.v[N].type) 
      {
        case '+': list.insert(lt, element.v[N].s);break;
        case '-': if (lt != list.end()) lt = list.erase(lt);break;
        case ' ': if (lt != list.end()) ++lt;break;
      }
    }
  }
       
  return std::vector<String>(list.begin(), list.end());
}

////////////////////////////////////////////////////////////
Diff Diff::inverted() const 
{
  Diff ret(*this);
  for (int I=0;I<(int)this->size();I++)
    ret.elements[I]=ret.elements[I].inverted();
  return ret;
}


////////////////////////////////////////////////////////////
String Diff::toString() const 
{
  std::ostringstream out;
  for (int I=0;I<(int)this->size();I++)
  {
    const Diff::Element& element=this->elements[I];
    out << "@@"
          << " -"  << element.a << "," << element.b
          << " +"  << element.c << "," << element.d
          << " @@" <<std::endl;

    for (int N=0;N<(int)element.v.size();N++)
      out << element.v[N].type << element.v[N].s << std::endl;
  }
  return out.str();    
}



} //namespace Visus
