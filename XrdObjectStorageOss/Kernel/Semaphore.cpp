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

#include "Semaphore.h"

#if WIN32
#include <Windows.h>
#else
#include <semaphore.h>
#endif

namespace Visus {
	
/////////////////////////////////////////////////////////////////////////////////////////
class Semaphore::Pimpl
{
public:
#if WIN32

	HANDLE handle;
	
	//constructor
	Pimpl() {
		handle = CreateSemaphore(nullptr, /*initial_value*/0, 0x7FFFFFFF, nullptr); 
		VisusAssert(handle != NULL);
	}
	
	//destructor
	~Pimpl() {
		CloseHandle(handle);
	}
	
	//down
	void down() {
		VisusReleaseAssert(WaitForSingleObject(handle, INFINITE) == WAIT_OBJECT_0);
	}
	
	//up
	void up() {
		VisusReleaseAssert(ReleaseSemaphore(handle, 1, nullptr));
	}
	
	//tryDown
	bool tryDown(){
		return WaitForSingleObject(handle, 0) == WAIT_OBJECT_0;
	}

#elif __clang__

  dispatch_semaphore_t sem;
  
  //constructor
  Pimpl() {
  	sem = dispatch_semaphore_create(/*initial_value*/0);VisusReleaseAssert(sem);
  }
  
  //destructor
  ~Pimpl()  {
  	dispatch_release(sem);
  }
  
  //down	
  void down() {
  	VisusReleaseAssert(dispatch_semaphore_wait(this->sem, DISPATCH_TIME_FOREVER) == 0);
  }
  
  //tryDown
  bool tryDown() {
  	return dispatch_semaphore_wait(this->sem, DISPATCH_TIME_NOW) == 0;
  }
  
  //up
  void up(){
  	dispatch_semaphore_signal(this->sem);
  }
  
#elif __APPLE__
 
 	//apple does not support unnamed semaphore
	//see https://heldercorreia.com/semaphores-in-mac-os-x-fd7a7418e13b

  sem_t* sem=nullptr;

	//constructor
  Pimpl() 
  {
  	static std::atomic<Int64> __id__(0);
  	
  	while (true)
  	{
	  	std::string name=concatenate("visus",(Int64)(++__id__));
	   	sem=sem_open(name.c_str(), O_CREAT | O_EXCL, 0644, /*initial_value*/0); 
	   	
	   	if (sem!=SEM_FAILED)
	   	{
	   		// marks the semaphore to be destroyed when all processes stop using it.
	   		sem_unlink(name.c_str());
	   		return;
	   	}
	   	
	   	if (errno==ENAMETOOLONG)
   		{
   			__id__.exchange(0);
   			continue;
   		}   	
	   	
	 		PrintInfo("sem_open() failed", name, strerror(errno));
	 		VisusReleaseAssert(false);
	  }
   	
  }
  
  //destructor
  ~Pimpl() {
  	sem_close(sem);
  }
  
	//down
  void down() {
  	while (sem_wait(sem)== -1) 
  		VisusReleaseAssert(errno == EINTR);
  }
  
  //tryDown
  bool tryDown() {
  	return sem_trywait(sem) == 0;
  }
  
  //up
  void up() {
  	VisusReleaseAssert(sem_post(sem) == 0);
  }
  
#else

	sem_t sem;
	
	//constructor
	Pimpl() {
		sem_init(&sem, 0, /*initial_value*/0);
	}
	
	//destructor
	~Pimpl() {
		sem_destroy(&sem);
	}
	
	//down
	void down() {
		while (sem_wait(&sem)== -1) VisusReleaseAssert(errno == EINTR);
	}
	
	//tryDown
	bool tryDown() {
		return sem_trywait(&sem) == 0;
	}
	
	//up
	void up()                 {
		VisusReleaseAssert(sem_post(&sem) == 0);
	}
  
#endif

}; 	
	

 ///////////////////////////////////////////////////
Semaphore::Semaphore(){
  pimpl = new Pimpl();
}

Semaphore::~Semaphore(){
  delete pimpl;
}

void Semaphore::down(){
  pimpl->down();
}

void Semaphore::up(){
  pimpl->up();
}

bool Semaphore::tryDown() {
  return pimpl->tryDown();
}

} //namespace Visus

