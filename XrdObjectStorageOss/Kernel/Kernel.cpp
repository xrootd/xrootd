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

#include "Kernel.h"

#include "Thread.h"
#include "NetService.h"
#include "Path.h"
#include "File.h"
#include "NetService.h"
#include "Utils.h"

#include <assert.h>
#include <type_traits>
#include <iomanip>
#include <fstream>
#include <iostream>
#include <atomic>
#include <clocale>
#include <cctype>

#if _WIN32
#include <Windows.h>
#include <winsock2.h>
#else
#include <signal.h>
#endif


#if __GNUC__ && !__APPLE__ && !WIN32
//this solve a problem of old Linux distribution (like Centos 5)
#ifndef htole32
#include <byteswap.h>
extern "C" uint32_t htole32(uint32_t x) {
	return bswap_32(htonl(x));
}
#endif
#endif

namespace Visus {
#define __str__(s) #s
#define __xstr__(s) __str__(s)


//////////////////////////////////////////////////////////////////
static std::pair< void (*)(String msg, void*), void*> __redirect_log__;

void RedirectLogTo(void(*callback)(String msg, void*), void* user_data) {
  __redirect_log__ = std::make_pair(callback, user_data);
}

void PrintMessageToTerminal(const String& value) {
#if WIN32 
	OutputDebugStringA(value.c_str());
#endif
	std::cout << value;
}

///////////////////////////////////////////////////////////////////////
void PrintLine(String file, int line, int level, String msg)
{
  auto t1 = Time::now();

  file = file.substr(file.find_last_of("/\\") + 1);
  file = file.substr(0, file.find_last_of('.'));

  std::ostringstream out;
  out << std::setfill('0')
    << std::setw(2) << t1.getHours()
    << std::setw(2) << t1.getMinutes()
    << std::setw(2) << t1.getSeconds()
    << std::setw(3) << t1.getMilliseconds()
    << " " << file << ":" << line
    << " " << Utils::getPid() << ":" << Thread::getThreadId()
    << " " << msg << std::endl;

  msg = out.str();
  PrintMessageToTerminal(msg);

  if (__redirect_log__.first)
    __redirect_log__.first(msg, __redirect_log__.second);
}


//////////////////////////////////////////////////////
void VisusAssertFailed(const char* file,int line,const char* expr)
{
#if _DEBUG
    Utils::breakInDebugger();
#else
    ThrowExceptionEx(file,line,expr);
#endif
}

String cnamed(String name, String value) {
  return name + "(" + value + ")";
}


//////////////////////////////////////////////////////
void ThrowExceptionEx(String file,int line, String what)
{
  String msg = cstring("Visus throwing exception", cnamed("where", file + ":" + cstring(line)), cnamed("what", what));
  PrintInfo(msg);
  throw std::runtime_error(msg);
}


int KernelModule::num_attached = 0;

//////////////////////////////////////////////////////
void KernelModule::attach()
{
  ++num_attached;
  if (num_attached != 1)
	 return;

  std::setlocale(LC_ALL, "en_US.UTF-8");

  //this is for generic network code
#if WIN32 
  WSADATA data;
  WSAStartup(MAKEWORD(2, 2), &data);
#else
  struct sigaction act, oact; //The SIGPIPE signal will be received if the peer has gone away
  act.sa_handler = SIG_IGN;   //and an attempt is made to write data to the peer. Ignoring this
  sigemptyset(&act.sa_mask);  //signal causes the write operation to receive an EPIPE error.
  act.sa_flags = 0;           //Thus, the user is informed about what happened.
  sigaction(SIGPIPE, &act, &oact);
#endif


  NetService::attach();

  //self-test for semaphores (DO NOT REMOVE, it force the creation the static Semaphore__id__)
  if (true)
	{
		Semaphore sem;
		sem.up();
		sem.down();
		VisusReleaseAssert(sem.tryDown()==false);
		sem.up();
		VisusReleaseAssert(sem.tryDown()==true);
	}
}


//////////////////////////////////////////////
void KernelModule::detach()
{
  --num_attached;
  if (num_attached != 0)
	return;

  NetService::detach();
}

} //namespace Visus


