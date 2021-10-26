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

#include <Visus/Field.h>

namespace Visus {

//////////////////////////////////////////////////////////////////////////////
static String parseRoundBracketArgument(String s, String name)
{
  String arg = StringUtils::nextToken(s, name + "(");
  if (arg.empty() || arg[0]==')')
    return "";

  auto v = StringUtils::split(arg, ")");
  if (v.empty())
    return "";

  return StringUtils::trim(v[0]);
}

  ////////////////////////////////////////////////////
Field Field::fromString(String sfield)
{
  std::istringstream ss(sfield);

  Field ret;

  //name
  {
    ss >> ret.name;
    if (ret.name.empty())
      return Field();
  }

  //dtype
  {
    String string_dtype;
    ss >> string_dtype;
    ret.dtype = DType::fromString(string_dtype);
    if (!ret.dtype.valid())
      return Field();
  }

  //description
  if (StringUtils::contains(sfield, "description("))
    ret.description = parseRoundBracketArgument(sfield, "description");

  //compression
  {
    if (StringUtils::contains(sfield, "default_compression("))
      ret.default_compression = parseRoundBracketArgument(sfield, "default_compression");

    else if (StringUtils::contains(sfield, "compression("))
      ret.default_compression = parseRoundBracketArgument(sfield, "compression");

    else if (StringUtils::contains(sfield, "compressed("))
      ret.default_compression = parseRoundBracketArgument(sfield, "compressed");

    else if (StringUtils::contains(sfield, "compressed"))
      ret.default_compression = "zip";
  }

  //layout
  {
    if (StringUtils::contains(sfield, "default_layout("))
      ret.default_layout = parseRoundBracketArgument(sfield, "default_layout");

    else if  (StringUtils::contains(sfield, "layout("))
      ret.default_layout = parseRoundBracketArgument(sfield, "layout");

    else if (StringUtils::contains(sfield, "format"))
      ret.default_layout = parseRoundBracketArgument(sfield, "format");

    else
      ret.default_layout = ""; 
  }

  //default_value
  {
    if (StringUtils::contains(sfield, "default_value("))
      ret.default_value = cint(parseRoundBracketArgument(sfield, "default_value"));
  }

  //filter(...)
  if (StringUtils::contains(sfield, "filter("))
    ret.filter = parseRoundBracketArgument(sfield, "filter");

  //min(...) max(...)
  if (StringUtils::contains(sfield, "min(") || StringUtils::contains(sfield, "max(")) 
  {
		auto vmin = StringUtils::split(parseRoundBracketArgument(sfield, "min"));
		auto vmax = StringUtils::split(parseRoundBracketArgument(sfield, "max"));
    if (!vmin.empty() && !vmax.empty())
		{
      //you can specify a range for all components or a range for each component (separated by spaces)
			vmin.resize(ret.dtype.ncomponents(), vmin.back());
      vmax.resize(ret.dtype.ncomponents(), vmax.back());

      for (int C = 0; C < ret.dtype.ncomponents(); C++)
        ret.setDTypeRange(Range(cdouble(vmin[C]), cdouble(vmax[C]), 0), C);
    }
  }

  return ret;
}

////////////////////////////////////////////////////
void Field::write(Archive& ar) const
{
  ar.write("name", this->name);
  ar.write("description", this->description);
  ar.write("index", this->index);
  ar.write("default_compression", this->default_compression);
  ar.write("default_layout", this->default_layout);
  ar.write("default_value", this->default_value);
  ar.write("filter", this->filter);
  ar.write("dtype", this->dtype);

  //write the min="..." max="..." extracted from the dtype range
#if 1
  {
    auto allZero = [](const std::vector<double>& v) {
      for (auto it : v)
        if (it != 0) return false;
      return true;;
    };

    auto allEqual = [](const std::vector< double>& v) {
      for (auto it : v)
        if (it != v[0]) return false;
      return true;
    };
    
    auto joinString = [](const std::vector< double>& v) {
      std::vector<String> tmp;
      for (auto it : v)
        tmp.push_back(cstring(it));
      return StringUtils::join(tmp, " ");
    };

    std::vector<double> vmin, vmax;
    for (int C = 0; C < this->dtype.ncomponents(); C++)
    {
      Range range = this->dtype.getDTypeRange(C);
      vmin.push_back(range.from);
      vmax.push_back(range.to);
    }

    if (!vmin.empty() && !allZero(vmin))
      ar.write("min", allEqual(vmin)? cstring(vmin[0]) : joinString(vmin));

    if (!vmax.empty() && !allZero(vmax))
      ar.write("max", allEqual(vmax)? cstring(vmax[0]) : joinString(vmax));
  }
#endif

  //params
  if (!params.empty())
  {
    auto params = ar.addChild("params");
    for (auto it : this->params)
      params->write(it.first, it.second);
  }
}

////////////////////////////////////////////////////
void Field::read(Archive& ar)
{
  ar.read("name", this->name);
  ar.read("description", this->description);
  ar.read("index", this->index);
  ar.read("default_compression", this->default_compression);
  ar.read("default_layout", this->default_layout);
  ar.read("default_value", this->default_value);
  ar.read("filter", this->filter);
  ar.read("dtype", this->dtype);

  //read the min(..) max(...) and store in the dtype range
#if 1
  {
    String s_m;  ar.read("min", s_m); std::vector<String> vmin = StringUtils::split(s_m);
    String s_M;  ar.read("max", s_M); std::vector<String> vmax = StringUtils::split(s_M);

    if (!vmin.empty() && !vmax.empty())
    {
      //you can specify a range for all components or a range for each component (separated by spaces)
      vmin.resize(this->dtype.ncomponents(), vmin.back());
      vmax.resize(this->dtype.ncomponents(), vmax.back());

      for (int C = 0; C < dtype.ncomponents(); C++)
        this->setDTypeRange(Range(cdouble(vmin[C]), cdouble(vmax[C]), 0.0), C);
    }
  }
#endif

  this->params.clear();
  if (auto params= ar.getChild("params"))
  {
    for (auto param : params->getChilds())
    {
      if (param->isHash()) continue;
      String key = param->name;
      String value;
      param->read(key,value);
      this->params.setValue(key,value);
    }
  }

}

} //namespace Visus 

