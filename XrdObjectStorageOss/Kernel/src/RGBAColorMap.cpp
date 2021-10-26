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

#include <Visus/RGBAColorMap.h>

namespace Visus {


/////////////////////////////////////////////////////////////////
RGBAColorMap::RGBAColorMap(const double* values, size_t num) 
{
  for (size_t I = 0; I < num; I += 4, values += 4)
  {
    double x = values[0];
    double r = values[2];
    double g = values[3];
    double b = values[4];
    double a = 1;
    setColorAt(x, Color((float)r, (float)g, (float)b, (float)a));
  }
}

/////////////////////////////////////////////////////////////////
Color RGBAColorMap::colorAt(double x) const
{
  x = Utils::clamp(x, min_x, max_x);

  for (int I = 0; I < (int)(colors.size() - 1); I++)
  {
    const auto& p0 = colors[I + 0]; auto x0 = p0.first; auto c0 = p0.second.toCieLab();
    const auto& p1 = colors[I + 1]; auto x1 = p1.first; auto c1 = p1.second.toCieLab();

    if (!(x0 <= x && x <= x1))
      continue;

    if (interpolation == InterpolationMode::Flat)
      return p0.second;

    auto alpha = (x - x0) / (x1 - x0);
    if (interpolation == InterpolationMode::Inverted)
      alpha = 1 - alpha;

    return Color::interpolate((Float32)(1 - alpha), c0, (Float32)alpha, c1).toRGB();
  }

  VisusAssert(false);
  return Colors::Black;
}

/////////////////////////////////////////////////////////////////
Array RGBAColorMap::toArray(int nsamples) const
{
  Array ret(nsamples, DTypes::UINT8_RGBA);

  Uint8* DST = ret.c_ptr();
  for (int I = 0; I < nsamples; I++)
  {
    double alpha = I / (double)(nsamples - 1);
    double x = min_x + alpha * (max_x - min_x);
    Color color = colorAt(x);
    *DST++ = (Uint8)(255 * color.getRed());
    *DST++ = (Uint8)(255 * color.getGreen());
    *DST++ = (Uint8)(255 * color.getBlue());
    *DST++ = (Uint8)(255 * color.getAlpha());
  }

  return ret;
}


} //namespace Visus

