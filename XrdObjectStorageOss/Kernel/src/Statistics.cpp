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

#include <Visus/Statistics.h>

namespace Visus {


///////////////////////////////////////////////////////////////////////////////////////
struct ComputeStatisticsOp
{

  struct RunningStat
  {
  public:

    //average, variance,standard_deviation
    // See Knuth TAOCP vol 2, 3rd edition, page 232
    //see http://www.johndcook.com/standard_deviation.html

    int    m_n=0;
    double m_oldM=0;
    double m_newM=0;
    double m_oldS=0;
    double m_newS=0;

    void Push(double x)
    {
      m_n++;

      // See Knuth TAOCP vol 2, 3rd edition, page 232
      if (m_n == 1)
      {
        m_oldM = m_newM = x;
        m_oldS = 0.0;
      }
      else
      {
        m_newM = m_oldM + (x - m_oldM) / m_n;
        m_newS = m_oldS + (x - m_oldM)*(x - m_newM);

        // set up for next iteration
        m_oldM = m_newM;
        m_oldS = m_newS;
      }
    }

    //Mean
    double Mean() const {
      return (m_n > 0) ? m_newM : 0.0;
    }

    //Variance
    double Variance() const {
      return ((m_n > 1) ? m_newS / (m_n - 1) : 0.0);
    }

    //StandardDeviation
    double StandardDeviation() const {
      return sqrt(Variance());
    }

  };

  //execute
  template<typename CppType>
  bool execute(Statistics& stats,Array src, int histogram_nbins,Aborted aborted)
  {
    if (aborted())
      return false;

    int ncomponents = src.dtype.ncomponents();

    stats.dims=src.dims;
    stats.dtype=src.dtype;
    stats.components.resize(ncomponents);

    Int64 nsamples=src.getTotalNumberOfSamples();

    for (int C=0;C<ncomponents;C++)
    {
      if (aborted())  
        return false;

      auto samples=GetComponentSamples<CppType>(src,C);

      auto& single=stats.components[C];
      single.dtype=src.dtype.get(C);
      single.dims =src.dims;
      single.field_range = src.dtype.getDTypeRange(C);
      single.computed_range  = ArrayUtils::computeRange(src, C, aborted);

      if (!nsamples)
        continue;

      RunningStat running_stats;
      std::vector<CppType> compute_median(nsamples);
      for (int I=0;I<nsamples;I++)
      {
        running_stats.Push((double)samples[I]);
        compute_median[I]=samples[I];
      }

      std::nth_element(&compute_median[0], &compute_median[0]+nsamples/2, &compute_median[0]+nsamples);

      single.average            = running_stats.Mean();
      single.variance           = running_stats.Variance();
      single.standard_deviation = running_stats.StandardDeviation();
      single.median             =(double)compute_median[nsamples/2];

      single.histogram=Histogram(src.dtype.isVectorOf(DTypes::UINT8) ? Range(0, 256, 1) : single.computed_range, histogram_nbins);
      if (!single.histogram.compute(src, C, aborted))
        return false;
    }

    return true;
  }
};

Statistics Statistics::compute(Array src, int histogram_nbins,Aborted aborted)
{
  ComputeStatisticsOp op;

  Statistics stats;
  if (!ExecuteOnCppSamples(op, src.dtype, stats, src, histogram_nbins, aborted))
    return Statistics();
  return stats;
}

} //namespace Visus

