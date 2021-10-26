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

#ifndef VISUS_TRANSFER_FUNCTION_H
#define VISUS_TRANSFER_FUNCTION_H

#include <Visus/Kernel.h>
#include <Visus/Model.h>
#include <Visus/Color.h>
#include <Visus/Array.h>

namespace Visus {


//////////////////////////////////////////////////////////////////////////
class VISUS_KERNEL_API SingleTransferFunction
{
public:

  VISUS_CLASS(SingleTransferFunction)

  std::vector<double> values;

  //constructor
  SingleTransferFunction(std::vector<double> values_=std::vector<double>(256,0.0))
    : values(values_) {
  }

  //constructor
  SingleTransferFunction(unsigned char* values, int nvalues)
  {
    for (int I=0;I< nvalues;I++)
      this->values.push_back(values[I]/255.0);
  }


  //constructor (identity function)
  SingleTransferFunction(int nsamples) : SingleTransferFunction(std::vector<double>(nsamples, 0.0)) {
  }

  //destructor
  virtual ~SingleTransferFunction() {
  }

  //getNumberOfSamples
  inline int getNumberOfSamples() const {
    return (int)values.size();
  }

  //getValue (x must be in range [0,1])
  double getValue(double x) const
  {
    int N = getNumberOfSamples();
    if (!N) {
      VisusAssert(false);
      return 0;
    }

    x = Utils::clamp(x * (N - 1), 0.0, N - 1.0);

    int i_x1 = Utils::clamp((int)std::floor(x), 0, N - 1);
    int i_x2 = Utils::clamp((int)std::ceil(x), 0, N - 1);

    if (i_x1 == i_x2)
      return values[(int)i_x1];

    double alpha = (i_x2 - x) / (double)(i_x2 - i_x1);
    double beta = 1 - alpha;
    return alpha * values[i_x1] + beta * values[i_x2];
  }

public:

  //write
  void write(Archive& ar) const
  {
    ar.write("values", values);
  }

  //read
  void read(Archive& ar)
  {
    ar.read("values", values);
  }

  //encode
  StringTree encode(String root_name) const {
    StringTree out(root_name);
    write(out);
    return out;
  }

  //decode
  void decode(StringTree in) {
    read(in);
  }

};


////////////////////////////////////////////////////////////////////////
class VISUS_KERNEL_API TransferFunction : public Model
{
public:

  //how to map input data to [0,1]
  enum NormalizationMode
  {
    FieldRange,
    ComputeRangePerComponent,
    ComputeRangeOverall,
    UserRange
  };

  SharedPtr<SingleTransferFunction> R;
  SharedPtr<SingleTransferFunction> G;
  SharedPtr<SingleTransferFunction> B;
  SharedPtr<SingleTransferFunction> A;;

  //texture
  SharedPtr<DynObject> texture;

  //constructor
  TransferFunction(int nsamples=256, String default_name_="") : default_name(default_name_) {
    R = std::make_shared<SingleTransferFunction>(nsamples);
    G = std::make_shared<SingleTransferFunction>(nsamples);
    B = std::make_shared<SingleTransferFunction>(nsamples);
    A = std::make_shared<SingleTransferFunction>(nsamples);
  }

  //destructor
  virtual ~TransferFunction() {
  }

  //fromArray
  static SharedPtr<TransferFunction> fromArray(Array src, String default_name="");

  //in case you want banded palette
  static SharedPtr<TransferFunction> fromColors(std::vector<Color> colors, String default_name = "");

  //fromString
  static SharedPtr<TransferFunction> fromString(String content);

  //getTypeName
  virtual String getTypeName() const override {
    return "TransferFunction";
  }

  //getFunctions
  std::vector<SharedPtr<SingleTransferFunction> > getFunctions() const {
    return { this->R,this->G,this->B,this->A };
  }

  //setRed
  void setRed(SharedPtr<SingleTransferFunction> value);

  //setGreen
  void setGreen(SharedPtr<SingleTransferFunction> value);

  //setBlue
  void setBlue(SharedPtr<SingleTransferFunction> value);

  //setRed
  void setAlpha(SharedPtr<SingleTransferFunction> value);

  //getNumberOfSamples
  int getNumberOfSamples() const {
    return R->getNumberOfSamples();
  }

  //valid
  bool valid() const {
    return getNumberOfSamples() > 0;
  }

  //operator=
  //TransferFunction& operator=(const TransferFunction& other);

  //isDefault
  bool isDefault() const {
    return !default_name.empty();
  }

  //getDefaultName
  String getDefaultName() const {
    return default_name;
  }

  //getAttenuation
  double getAttenuation() const {
    return attenuation;
  }

  //setAttenutation
  void setAttenutation(double value) {
    setProperty("SetAttenutation", this->attenuation, value);
  }

  //getNormalizationMode
  int getNormalizationMode() const {
    return normalization_mode;
  }

  //setInputNormalizationMode
  void setNormalizationMode(int value) {
    setProperty("SetNormalizationMode", this->normalization_mode, value);
  }

  //getUserRange
  Range getUserRange() const {
    return this->user_range;
  }

  //setUserRange
  void setUserRange(Range range) {
    setProperty("SetUserRange", this->user_range, range);
  }

  //drawValues
  void drawValues(int function, int x1, int x2, std::vector<double> values);

  //drawLine 
  void drawLine(int function, int x1, double y1, int x2, double y2);

public:

  //getDefaults
  static std::vector<String> getDefaults();

  //getDefault
  static SharedPtr<TransferFunction> getDefault(String name);

  //setDefault
  void setDefault(String name);

public:

  //getDefaultOpacities
  static std::vector<String> getDefaultOpacities();

  //getDefaultOpacity
  static SharedPtr<SingleTransferFunction> getDefaultOpacity(String name);

  //setOpacity
  void setOpacity(String name);

public:

  //ComputeRange
  static Range ComputeRange(Array data, int C, bool bNormalizeToFloat = false, int normalization = FieldRange, Range user_range = Range::invalid());

public:

  //toArray
  Array toArray() const;

public:

  //importTransferFunction
  static SharedPtr<TransferFunction> importTransferFunction(String content);

  //exportTransferFunction
  void exportTransferFunction(String filename);

public:

  //applyToArray
  Array applyToArray(Array src, Aborted aborted = Aborted());

public:

  //execute
  virtual void execute(Archive& ar) override;

  //write
  virtual void write(Archive& ar) const override;

  //read
  virtual void read(Archive& ar) override;

private:

  //default_name
  String default_name;

  //see https://github.com/sci-visus/visus-issues/issues/260
  double attenuation = 0.0;

  //input_normalization
  int normalization_mode = FieldRange;

  //user_range
  Range user_range;

  //setDefaultName
  void setDefaultName(String value) {
    setProperty("SetDefaultName", this->default_name, value);
  }

};

typedef TransferFunction Palette;

} //namespace Visus

#endif //VISUS_TRANSFER_FUNCTION_H

