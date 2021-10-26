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

#ifndef VISUS_SIGNAL_SLOT_H__
#define VISUS_SIGNAL_SLOT_H__

#include <Visus/Kernel.h>
#include <Visus/Utils.h>

#include <vector>
#include <atomic>
#include <functional>

namespace Visus {

//////////////////////////////////////////////////////////
template <typename Signature>
class Slot
{
public:

  //constructor
  Slot() : id(0){
  }

  //constructor
  Slot(std::function<Signature> fn_) : id(generateId()),fn(fn_){
  }

  //copy constructor
  Slot(const Slot& other) : id(other.id),fn(other.fn) {
  }

  //destructor
  ~Slot() {
  }

  //operator=
  Slot& operator=(std::function<Signature> fn) {
    this->id=generateId();
    this->fn=fn;
    return *this;
  }
     
  //operator=
  Slot& operator=(const Slot& other) {
    this->id=other.id;
    this->fn=other.fn;
    return *this;
  }

  //receiveSignal
  template<typename... Args>
  void receiveSignal(Args&&... args) {
    fn(std::forward<Args>(args)...);
  }

  //operator==
  bool operator==(const Slot& other) const {
    return id==other.id;
  }

  //operator!=
  bool operator!=(const Slot& other) const {
    return !operator==(other);
  }

private:

  int id;

  std::function<Signature> fn;
 
  //generateId
  static int generateId() {
    static std::atomic<int> ret(0);
    return ++ret;
  }

};

//////////////////////////////////////////////////////////
template <typename Signature>
class Signal
{
public:

  typedef Visus::Slot<Signature> Slot;

  //constructor
  Signal() {
  }

  //destructor
  ~Signal() {
  }

  //empty
  bool empty() const {
    return slots.empty();
  }

  //connect
  void connect(Slot value)  {
    slots.push_back(value);
  }

  //connect
  void connect(std::function<Signature> fn) {
    connect(Slot(fn));
  }

  //disconnect
  void disconnect(Slot value) {
    Utils::remove(slots,value);
  }

  //emitSignal
  template<typename... Args>
  void emitSignal(Args&&... args) {
    auto slots=this->slots;
    for (auto slot : slots)
      slot.receiveSignal(std::forward<Args>(args)...);
  }

private:

  VISUS_NON_COPYABLE_CLASS(Signal)

  std::vector<Slot> slots;

};
  



} //namespace Visus

#endif //VISUS_SIGNAL_SLOT_H__


