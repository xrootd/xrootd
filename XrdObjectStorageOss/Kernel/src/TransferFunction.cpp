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

#include <Visus/TransferFunction.h>

namespace Visus {

/////////////////////////////////////////////////////////////////////
SharedPtr<TransferFunction> TransferFunction::fromArray(Array src, String default_name)
{
  int nfunctions = src.dtype.ncomponents();
  VisusReleaseAssert(nfunctions == 4);
  int nsamples = (int)src.dims[0];

  src = ArrayUtils::smartCast(src, DType(src.dtype.ncomponents(), DTypes::FLOAT64));

  auto values_R = src.getComponent(0); auto R = values_R.c_ptr<Float64*>();
  auto values_G = src.getComponent(1); auto G = values_G.c_ptr<Float64*>();
  auto values_B = src.getComponent(2); auto B = values_B.c_ptr<Float64*>();
  auto values_A = src.getComponent(3); auto A = values_A.c_ptr<Float64*>();

  auto ret = std::make_shared<TransferFunction>(nsamples, default_name);
  ret->R = std::make_shared<SingleTransferFunction>(std::vector<double>(R, R + nsamples));
  ret->G = std::make_shared<SingleTransferFunction>(std::vector<double>(G, G + nsamples));
  ret->B = std::make_shared<SingleTransferFunction>(std::vector<double>(B, B + nsamples));
  ret->A = std::make_shared<SingleTransferFunction>(std::vector<double>(A, A + nsamples));
  return ret;
}

/////////////////////////////////////////////////////////////////////
SharedPtr<TransferFunction> TransferFunction::fromColors(std::vector<Color> colors, String default_name)
{
  auto ncolors = (int)colors.size();

  auto nsamples = 256;
  Array array(256, DTypes::UINT8_RGBA);

  Uint8* DST = array.c_ptr();
  for (int I = 0; I < nsamples; I++)
  {
    double alpha = I / (double)nsamples;
    auto index = (int)(alpha * (colors.size()));
    auto color = colors[Utils::clamp(index, 0, ncolors)];
    *DST++ = (Uint8)(255 * color.getRed());
    *DST++ = (Uint8)(255 * color.getGreen());
    *DST++ = (Uint8)(255 * color.getBlue());
    *DST++ = (Uint8)(255 * color.getAlpha());
  }

  return TransferFunction::fromArray(array, default_name);
}

/////////////////////////////////////////////////////////////////////
SharedPtr<TransferFunction> TransferFunction::fromString(String content)
{
  StringTree in=StringTree::fromString(content);
  if (!in.valid())
  {
    VisusAssert(false);
    return SharedPtr<TransferFunction>();
  }

  auto ret = std::make_shared<TransferFunction>();
  ret->read(in);
  return ret;
}


StringTree DrawLine(int function, int x1, double y1, int x2, double y2) {
  return StringTree("DrawLine").write("function", function).write("x1", x1).write("y1", y1).write("x2", x2).write("y2", y2);
}

StringTree DrawValues(int function, int x1, int x2, std::vector<double> values) {
  return StringTree("DrawValues").write("function", function).write("x1", x1).write("x2", x2).write("values", values);
}


////////////////////////////////////////////////////////////////////
void TransferFunction::setRed(SharedPtr<SingleTransferFunction> value)
{
  if (this->R == value) return;
  beginUpdate(value->encode("SetRed"), this->R->encode("SetRed"));
  this->R = value;
  endUpdate();

}

////////////////////////////////////////////////////////////////////
void TransferFunction::setGreen(SharedPtr<SingleTransferFunction> value)
{
  if (this->G == value) return;
  beginUpdate(value->encode("SetGreen"), this->R->encode("SetGreena"));
  this->G = value;
  endUpdate();
}

////////////////////////////////////////////////////////////////////
void TransferFunction::setBlue(SharedPtr<SingleTransferFunction> value)
{
  if (this->B == value) return;
  beginUpdate(value->encode("SetBlue"), this->R->encode("SetBlue"));
  this->B = value;
  endUpdate();
}

////////////////////////////////////////////////////////////////////////////
void TransferFunction::setAlpha(SharedPtr<SingleTransferFunction> value)
{
  if (this->A == value) return;
  beginUpdate(value->encode("SetAlpha"), this->R->encode("SetAlpha"));
  this->A = value;
  endUpdate();
}  


////////////////////////////////////////////////////////////////////////////
void TransferFunction::setDefault(String default_name)
{
  //if (bFullCopy)
  //{
  //  TransferFunction::copy(*this, *getDefault(default_name));
  //}

  auto colormap = getDefault(default_name);

  StringTree redo("SetDefault", "name", default_name);

  //describe actions for undo
  StringTree undo = Transaction();
  {
    undo.addChild("SetDefaultName")->write("value", this->default_name);  //I need to keep the old default_name
    undo.addChild(R->encode("SetRed"));
    undo.addChild(G->encode("SetGreen"));
    undo.addChild(B->encode("SetBlue"));
    undo.addChild(A->encode("SetAlpha"));
  }

  beginUpdate(redo, undo);
  {
    this->default_name = default_name;
    this->R = colormap->R;
    this->G = colormap->G;
    this->B = colormap->B;
    this->A = colormap->A;
    this->texture.reset();
  }
  endUpdate();
}

////////////////////////////////////////////////////////////////////////////
void TransferFunction::setOpacity(String name)
{
  auto A = getDefaultOpacity(name);

  StringTree redo("SetOpacity", "name", name);

  //describe actions for undo
  StringTree undo = Transaction();
  {
    undo.addChild("SetDefaultName")->write("value", this->default_name);  //I need to keep the old default_name
    undo.addChild(A->encode("SetAlpha"));
  }

  beginUpdate(redo, undo);
  {
    this->default_name = ""; //
    this->A = A;
    this->texture.reset();
  }
  endUpdate();
}


/// //////////////////////////////////////////////////////////////
struct ApplyTransferFunctionOp
{
  template <typename SrcType>
  bool execute(Array& dst, TransferFunction& tf, Array src, Aborted aborted) 
  {
    int NS = (int)src.dtype.ncomponents();
    if (!NS)
      return false;

    // for one input-> R(input) G(input) B(input) A(input)
    // otherwise    -> R(input[0]) G(input[1]) B(input[2]) A(input[3]) 
    int ND = (NS == 1)? 4 : std::min(4, NS);

    if (!dst.resize(src.dims, DType(ND, DTypes::UINT8), __FILE__, __LINE__))
      return false;

    for (int I = 0; I < ND; I++)
    {
      auto D = I < ND ? I : ND - 1; auto DST = GetComponentSamples<Uint8  >(dst, D);
      auto S = I < NS ? I : NS - 1; auto SRC = GetComponentSamples<SrcType>(src, S);

      auto input_range = TransferFunction::ComputeRange(src, S,/*bNormalizeToFloat*/false, tf.getNormalizationMode(), tf.getUserRange());
      double A = input_range.from;
      double B = input_range.to;

      VisusReleaseAssert(I>=0 && I<4);
      auto f = tf.getFunctions()[I];
      for (Int64 I = 0, Tot= src.getTotalNumberOfSamples(); I < Tot; I++)
      {
        if (aborted())  return false;
        double x = (SRC[I] - A) / (B - A);
        double y = f->getValue(x);
        DST[I] = (Uint8)Utils::clamp((Uint8)(y*255.0), (Uint8)0, (Uint8)255);
      }
    }

    dst.shareProperties(src);
    return true;
  }
};

////////////////////////////////////////////////////////////////////
Array TransferFunction::applyToArray(Array src, Aborted aborted)
{
  if (!src.valid()) return src;
  Array dst;
  ApplyTransferFunctionOp op;
  return ExecuteOnCppSamples(op, src.dtype, dst, *this, src, aborted) ? dst : Array();
}

 
  ////////////////////////////////////////////////////////////////////
void TransferFunction::execute(Archive& ar)
{
  if (ar.name == "SetDefault")
  {
    String name;
    ar.read("name", name);
    setDefault(name);
    return;
  }

  if (ar.name == "SetOpacity" )
  {
    String name;
    ar.read("name", name);
    setOpacity(name);
    return;
  }


  if (ar.name == "SetDefaultName")
  {
    String value;
    ar.read("value", value);
    setDefaultName(value);
    return;
  }

  if (ar.name == "SetRed")
  {
    auto value = std::make_shared<SingleTransferFunction>(); 
    value->read(ar);
    setRed(value);
    return;
  }

  if (ar.name == "SetGreen")
  {
    auto value = std::make_shared<SingleTransferFunction>(); 
    value->read(ar);
    setGreen(value);
    return;
  }

  if (ar.name == "SetBlue")
  {
    auto value = std::make_shared<SingleTransferFunction>(); 
    value->read(ar);
    setBlue(value);
    return;
  }

  if (ar.name == "SetAlpha")
  {
    auto value = std::make_shared<SingleTransferFunction>(); 
    value->read(ar);
    setAlpha(value);
    return;
  }

  if (ar.name == "SetUserRange")
  {
    Range value;
    ar.read("value", value);
    setUserRange(user_range);
    return;
  }

  if (ar.name == "SetNormalizationMode")
  {
    int value = FieldRange;
    ar.read("value", value);
    setNormalizationMode(value);
    return;
  }

  if (ar.name == "SetAttenutation")
  {
    double value;
    ar.read("value", value);
    setAttenutation(value);
    return;
  }

  if (ar.name == "DrawLine")
  {
    int function, x1, x2; double y1, y2;
    ar.read("function", function);
    ar.read("x1", x1);
    ar.read("y1", y1);
    ar.read("x2", x2);
    ar.read("y2", y2);
    drawLine(function, x1, y1, x2, y2);
    return;
  }

  if (ar.name == "DrawValues")
  {
    int function, x1, x2; std::vector<double> values;
    ar.read("function", function);
    ar.read("x1", x1);
    ar.read("x2", x2);
    ar.read("values", values);
    drawValues(function, x1, x2, values);
    return;
  }

  Model::execute(ar);
}


////////////////////////////////////////////////////////////////////
void TransferFunction::drawValues(int function, int x1, int x2, std::vector<double> new_values)
{
  int N = (int)new_values.size();
  VisusReleaseAssert(N==(1+x2-x1));
  VisusReleaseAssert(0<=x1 && x1<=x2 && x2<getNumberOfSamples());

  auto fn = getFunctions()[function];

  std::vector<double> old_values;
  for (int x = x1; x<=x2; x++)
    old_values.push_back(fn->values[x]);

  //useless call
  if (new_values == old_values)
    return;

  beginUpdate(
    DrawValues(function, x1, x2, new_values),
    DrawValues(function, x1, x2, old_values));
  {
    this->default_name = "";

    for (int x = x1; x <=x2; x++)
      fn->values[x] = new_values[x-x1];
  }
  endUpdate();
}


////////////////////////////////////////////////////////////////////
void TransferFunction::drawLine(int function, int x1, double y1,int x2, double y2)
{
  if (x2 < x1)
  {
    std::swap(x1, x2);
    std::swap(y1, y2);
  }

  x1 = Utils::clamp(x1, 0, getNumberOfSamples() - 1);
  x2 = Utils::clamp(x2, 0, getNumberOfSamples() - 1);

  auto fn = getFunctions()[function];

  std::vector<double> new_values; 
  for (int I=0; I < 1 + x2 - x1; I++)
  {
    double alpha  = (x1 == x2) ? 1.0 : I / (double)(x2 - x1);
    new_values.push_back((1 - alpha) * y1 + alpha * y2);
  }

  std::vector<double> old_values;
  for (int I = 0; I < new_values.size(); I++)
    old_values.push_back(fn->values[x1 + I]);

  //useless call
  if (old_values == new_values)
    return;

  beginUpdate(
    DrawLine(function, x1, y1, x2, y2), 
    DrawValues(function, x1, x2, old_values));
  {
    this->default_name = "";

    for (int I = 0; I < new_values.size(); I++)
      fn->values[x1 + I] = new_values[I];
  }
  endUpdate();
}


/////////////////////////////////////////////////////////////////////
Array TransferFunction::toArray() const
{
  Array ret;

  int nsamples=getNumberOfSamples();

#if 1
  {
    if (!ret.resize(nsamples,DTypes::UINT8_RGBA,__FILE__,__LINE__))
      return Array();

    for (int F=0;F< 4;F++)
    {
      auto fn = getFunctions()[F];
      GetComponentSamples<Uint8> write(ret, F);
      for (int I = 0; I < nsamples; I++)
      {
        double value = fn->values[I] * ((F == 3) ? (1 - this->attenuation) : 1.0);
        write[I] = (Uint8)(value * 255.0);
      }
    }
  }
#else
  {
    if (!ret.resize(nsamples,DTypes::FLOAT32_RGBA,__FILE__,__LINE__))
      return Array();

    for (int F = 0; F < 4; F++)
    {
      auto fn = functions[F];
      GetComponentSamples<Float32> write(ret,F);
      for (int I = 0; I < nsamples; I++)
      {
        auto value = fn->values[I] * (F == 3) ? (1 - this->attenuation) : 1.0;
        write[I] = (Float32)(value);
      }
    }
  }
#endif

  return ret;
}


///////////////////////////////////////////////////////////
SharedPtr<TransferFunction> TransferFunction::importTransferFunction(String url)
{
  /*
  <nsamples> 
  R G B A
  R G B A
  ...
  */

  String content = Utils::loadTextDocument(url);

  std::vector<String> lines = StringUtils::getNonEmptyLines(content);
  if (lines.empty())
  {
    PrintWarning("content is empty");
    return SharedPtr<TransferFunction>();
  }

  int nsamples = cint(lines[0]);
  lines.erase(lines.begin());
  if (lines.size() != nsamples)
  {
    PrintWarning("content is of incorrect length");
    return SharedPtr<TransferFunction>();
  }

  auto ret=std::make_shared<TransferFunction>(nsamples,"");

  for (int I = 0; I < nsamples; I++)
  {
    std::istringstream istream(lines[I]);
    for (int F = 0; F < 4; F++)
    {
      double value;
      istream >> value;
      value = value / (nsamples - 1.0);
      ret->getFunctions()[F]->values[I] = value;
    }
  }

  return ret;
}


///////////////////////////////////////////////////////////
void TransferFunction::exportTransferFunction(String filename="")
{
  /*
  <nsamples>
  R G B A
  R G B A
  ...
  */

  int nsamples = getNumberOfSamples();

  if (!nsamples)
    ThrowException("invalid nsamples");

  std::ostringstream out;
  out<<nsamples<<std::endl;
  for (int I=0;I<nsamples;I++)
  {
    for (auto fn : getFunctions())
    {
      int value = (int)(fn->values[I] * (nsamples - 1));
      out << value << " ";
    }
    out<<std::endl;
  }

  Utils::saveTextDocument(filename, out.str());
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
Range TransferFunction::ComputeRange(Array data, int C, bool bNormalizeToFloat, int normalization, Range user_range)
{
  Range range;

  if (normalization == Palette::UserRange)
  {
    range = user_range;
  }
  else if (normalization == Palette::FieldRange)
  {
    range = data.dtype.getDTypeRange(C);
    if (range.delta() <= 0)
    {
      if (data.dtype.isDecimal())
        range = ArrayUtils::computeRange(data, C); //just use the computer range of the data (the c++ range would be too high)
      else
        range = GetCppRange(data.dtype); //assume the data is spread in all the possible discrete range
    }
  }
  else if (normalization == Palette::ComputeRangePerComponent)
  {
    range = ArrayUtils::computeRange(data, C);
  }
  else if (normalization == Palette::ComputeRangeOverall)
  {
    range = Range::invalid();
    for (int C = 0; C < data.dtype.ncomponents(); C++)
      range = range.getUnion(ArrayUtils::computeRange(data, C));
  }
  else
  {
    ThrowException("internal error");
  }

  //the GL textures is read from GPU memory always in the range [0,1]
    //see https://www.khronos.org/opengl/wiki/Normalized_Integer
  if (bNormalizeToFloat && !data.dtype.isDecimal() && range.delta())
  {
    auto cpp_range = GetCppRange(data.dtype);
    auto A = cpp_range.from;
    auto B = cpp_range.to;

    auto C = data.dtype.isUnsigned() ? 0.0 : -1.0;
    auto D = 1.0;

    auto NormalizeInteger = [&](double value) {
      auto alpha = (value - A) / (B - A);
      return C + alpha * (D - C);
    };

    range = Range(
      NormalizeInteger(range.from),
      NormalizeInteger(range.to),
      0.0);
  }

  return range;
}


/////////////////////////////////////////////////////////////////////
void TransferFunction::write(Archive& ar) const
{
  int nsamples = getNumberOfSamples();

  ar.write("default_name", default_name);
  ar.write("nsamples", nsamples);
  ar.write("attenuation", attenuation);
  ar.write("user_range", user_range);
  ar.write("normalization_mode", normalization_mode);

  if (!isDefault())
  {
    this->R->write(*ar.addChild("function"));
    this->G->write(*ar.addChild("function"));
    this->B->write(*ar.addChild("function"));
    this->A->write(*ar.addChild("function"));
  }
}

/////////////////////////////////////////////////////////////////////
void TransferFunction::read(Archive& ar)
{
  const int nsamples=256;
  ar.read("default_name", default_name);
  ar.read("attenuation", attenuation);
  
  ar.read(ar.hasAttribute("input_range")? "input_range"              : "user_range",user_range);  //backward compatible
  ar.read(ar.hasAttribute("input_range")? "input_normalization_mode" : "normalization_mode", normalization_mode);  //backward compatible

  if (this->default_name.empty())
  {
    auto v = ar.getChilds("function");
    this->R = std::make_shared<SingleTransferFunction>(nsamples); this->R->read(*v[0]);
    this->G = std::make_shared<SingleTransferFunction>(nsamples); this->G->read(*v[1]);
    this->B = std::make_shared<SingleTransferFunction>(nsamples); this->B->read(*v[2]);
    this->A = std::make_shared<SingleTransferFunction>(nsamples); this->A->read(*v[3]);
  }
  else
  {
    auto def = getDefault(default_name);
    this->R = def->R;
    this->G = def->G;
    this->B = def->B;
    this->A = def->A;
  }
}

} //namespace Visus

