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

#ifndef __VISUS_HISTOGRAM_H
#define __VISUS_HISTOGRAM_H

#include <Visus/Kernel.h>
#include <Visus/Array.h>

namespace Visus {

////////////////////////////////////////////////////////////////////
class VISUS_KERNEL_API Histogram 
{
public:

  VISUS_CLASS(Histogram)

  Range                range=Range(0,0,0);
  std::vector<Uint64>  bins;

  //constructor
  Histogram() {
  }

  //constructor
  Histogram(Range range,int nbins) 
  {
    this->bins.resize(nbins);
    this->range = Range(range.from,range.to, (range.to - range.from) / nbins);
  }

  //compute
  bool compute(Array src,int C,Aborted aborted=Aborted());

  //empty
  bool empty() const {
    return this->bins.empty();
  }

  //getNumBins
  int getNumBins() const {
    return (int)((this->bins).size());
  }

  //getRange
  Range getRange() const {
    return this->range;
  }

  //getBinRange
  Range getBinRange(int x) const {
    int W=getNumBins();
    Range ret;
    if (!(x>=0 && x<W)) {VisusAssert(false);return ret;}
    ret.from =this->range.from+((x+0)/((double)W))*(this->range.to-this->range.from);
    ret.to   =this->range.from+((x+1)/((double)W))*(this->range.to-this->range.from);
    return ret;
  }

  //readBin
  Uint64 readBin(int x) const {
    if (x < 0            ) x = 0;
    if (x >= getNumBins()) x = getNumBins() - 1;
    return  this->bins[x];
  }

  //findBin
  int findBin(double value) const
  {
    VisusAssert(!this->bins.empty());
    value=Utils::clamp(value,this->range.from,this->range.to);
    int W = (int)getNumBins();
    int ret = (int)(W*((value - this->range.from) / (this->range.to - this->range.from)));
    if (ret < 0) ret = 0;
    if (ret >= W) ret = W - 1;
    return ret;
  }

  //incrementBin
  void incrementBin(int x)
  {
    VisusAssert(x < this->bins.size());
    this->bins[x] = this->bins[x] + 1;
  }



};


} //namespace Visus

#endif //__VISUS_HISTOGRAM_H

