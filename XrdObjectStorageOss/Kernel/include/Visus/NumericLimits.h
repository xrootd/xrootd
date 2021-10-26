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

#ifndef _VISUS_NUMERIC_LIMITS_H__
#define _VISUS_NUMERIC_LIMITS_H__

#include <Visus/Kernel.h>

#include <limits>

//solve portability issue about lowest 
//consider that std::numeric_limits<double>::min returns a positive small value on some platforms

namespace Visus {

  namespace PrivateNumericLimits
  {
    template<typename T> static inline T    lowest()              {return  std::numeric_limits<T          >::min();}
    template<>           inline float       lowest<float      >() {return -std::numeric_limits<float      >::max();}
    template<>           inline double      lowest<double     >() {return -std::numeric_limits<double     >::max();}
    template<>           inline long double lowest<long double>() {return -std::numeric_limits<long double>::max();}
  }

  template <typename T>
  struct NumericLimits
  {
    static inline T infinity() {return std::numeric_limits<T>::infinity();}
    static inline T epsilon () {return std::numeric_limits<T>::epsilon();}
    static inline T lowest  () {return PrivateNumericLimits::lowest<T>();}
    static inline T highest () {return std::numeric_limits<T>::max();}
  };

} //namespace Visus

#endif //_VISUS_NUMERIC_LIMITS_H__

