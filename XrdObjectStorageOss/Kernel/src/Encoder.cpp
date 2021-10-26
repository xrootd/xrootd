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

#include <Visus/Encoder.h>
#include <Visus/StringUtils.h>



namespace Visus {

VISUS_IMPLEMENT_SINGLETON_CLASS(Encoders)


////////////////////////////////////////////////////////////////
void Encoders::registerEncoder(String key, Creator creator)
{
  key = StringUtils::trim(key);

  creators.push_back(std::make_pair(key, creator));

  //find the creator with more "in-common" characters
  // example, if we had two creators "jpeg" and "jpeg-experimental" and I do createEncoder("jpeg-experimental-100,200,300")"
  // then I will call std::make_shared<JPegExperimental>("jpeg-experimental-100,200,300");
  //see https://github.com/sci-visus/OpenVisus/issues/82
  std::sort(creators.begin(), creators.end(), [](const std::pair<String, Creator>& a, const std::pair<String, Creator>& b) {
    return a.first.size() > b.first.size();
  });

}

////////////////////////////////////////////////////////////////
SharedPtr<Encoder> Encoders::createEncoder(String specs) const
{
  specs = StringUtils::trim(specs);

  for (auto it : creators)
  {
    auto key = it.first;
    if (StringUtils::startsWith(specs, key))
    {
      auto creator = it.second;
      return creator(specs);
    }
  }

  //not found
  return SharedPtr<Encoder>();
}


} //namespace Visus

