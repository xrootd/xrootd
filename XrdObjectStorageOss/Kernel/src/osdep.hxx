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

#ifndef __VISUS_OS_DEP_H__
#define __VISUS_OS_DEP_H__

#include <Visus/Kernel.h>
#include <Visus/File.h>
#include <Visus/Semaphore.h>
#include <Visus/Thread.h>
#include <Visus/Time.h>

#include <iostream>
#include <fcntl.h>
#include <cstring>
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>

//////////////////////////////////////////////////
#if WIN32
	#include <Windows.h>
	#include <Psapi.h>
	#include <io.h>
	#include <direct.h>
	#include <process.h>
	#include <sddl.h>
	#include <ShlObj.h>
	#include <winsock2.h>
	#include <time.h>
	#include <sys/timeb.h>
	
	#pragma warning(disable:4996)
	
	#define getIpCat(__value__)    htonl(__value__)
	typedef int socklen_t;
	#define SHUT_RD   SD_RECEIVE 
	#define SHUT_WR   SD_SEND 
	#define SHUT_RDWR SD_BOTH 
	
	#define Stat64 ::_stat64
	#define LSeeki64 _lseeki64

//////////////////////////////////////////////////
#elif __APPLE__
	#include <unistd.h>
	#include <signal.h>
	#include <dlfcn.h>
	#include <errno.h>
	#include <pwd.h>
	#include <netdb.h> 
	#include <strings.h>
	#include <pthread.h>
	#include <climits>
	
	#include <sys/socket.h>
	#include <sys/mman.h>
	#include <sys/stat.h>
	#include <sys/sysctl.h>
	#include <sys/ioctl.h>
	#include <sys/time.h>
	#include <dirent.h>
	
	#include <mach/mach.h>
	#include <mach/task.h>
	#include <mach/mach_init.h>
	#include <mach/mach_host.h>
	#include <mach/vm_statistics.h>
	#include <mach/mach_types.h>
	#include <mach-o/dyld.h>
	
	#include <arpa/inet.h>
	#include <netinet/tcp.h>

	#define getIpCat(__value__)    __value__
	#define closesocket(socketref) ::close(socketref)
	#define Stat64                 ::stat
	#define LSeeki64               ::lseek
	
	#if __clang__
		#include <dispatch/dispatch.h>
		void mm_InitAutoReleasePool();
		void mm_DestroyAutoReleasePool();
	#else
		#include <semaphore.h>
	#endif

//////////////////////////////////////////////////
#else
	
	#ifndef _GNU_SOURCE
	#define _GNU_SOURCE
	#endif
	
	#include <semaphore.h>
	#include <errno.h>
	#include <unistd.h>
	#include <limits.h>
	#include <dlfcn.h>
	#include <pwd.h>
	#include <pthread.h>
	#include <netdb.h> 
	#include <strings.h>
	#include <signal.h>
	#include <sys/sendfile.h>
	#include <sys/socket.h>
	#include <sys/sysinfo.h>
	
	#include <sys/mman.h>
	#include <sys/stat.h>
	#include <sys/types.h>
	#include <sys/ioctl.h>
	#include <sys/time.h>
	
	#include <arpa/inet.h>
	#include <netinet/tcp.h>

	#include <dirent.h>

	#define getIpCat(__value__)    __value__
	#define closesocket(socketref) ::close(socketref)
	#define Stat64                 ::stat
	#define LSeeki64               ::lseek

#endif


#ifndef O_BINARY
#define O_BINARY 0
#endif 

#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

#ifndef S_ISREG
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif

namespace Visus {

VISUS_SHARED_EXPORT void __do_not_remove_my_function__();

/// /////////////////////////////////////////////////////////////////////////////////////////
class osdep {

public:

  //getPlatformName
  static String getPlatformName()
	{
	#if WIN32
	  return "win";
	#elif __APPLE__
	  return "osx";
	#else
	  return "unix";
	#endif
	}  

  //BreakInDebugger
  static void BreakInDebugger()
	{
	#if WIN32 
	  #if __MSVC_VER
	    _CrtDbgBreak();
	  #else
	    DebugBreak();
	  #endif
	#elif __APPLE__
	  asm("int $3");
	#else
	  ::kill(0, SIGTRAP);
	  assert(0);
	#endif
	}  

  //InitAutoReleasePool
  static void InitAutoReleasePool()
	{
	#if __clang__
	  mm_InitAutoReleasePool();
	#endif
	}  

  //DestroyAutoReleasePool
  static void DestroyAutoReleasePool()
	{
	#if __clang__
	  mm_DestroyAutoReleasePool();
	#endif
	}  

  //GetTotalMemory
  static Int64 GetTotalMemory()
	{
	#if WIN32 
	  MEMORYSTATUSEX status;
	  status.dwLength = sizeof(status);
	  GlobalMemoryStatusEx(&status);
	  return status.ullTotalPhys;
	#elif __APPLE__
	  int mib[2] = { CTL_HW,HW_MEMSIZE };
	  Int64 ret = 0;
	  size_t length = sizeof(ret);
	  sysctl(mib, 2, &ret, &length, NULL, 0);
	  return ret;
	#else
	  struct sysinfo memInfo;
	  sysinfo(&memInfo);
	  return ((Int64)memInfo.totalram) * memInfo.mem_unit;
	#endif
	};  

  ///GetProcessUsedMemory
  static Int64 GetProcessUsedMemory()
	{
	#if WIN32 
	  PROCESS_MEMORY_COUNTERS pmc;
	  GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
	  return pmc.PagefileUsage;
	#elif __APPLE__
	  struct task_basic_info t_info;
	  mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;
	  task_info(current_task(), TASK_BASIC_INFO, (task_info_t)&t_info, &t_info_count);
	  size_t size = t_info.resident_size;
	  return (Int64)size;
	#else
	  long rss = 0L;
	  FILE* fp = NULL;
	  if ((fp = fopen("/proc/self/statm", "r")) == NULL) return 0;
	  if (fscanf(fp, "%*s%ld", &rss) != 1)
	  {
	    fclose(fp); return 0;
	  }
	  fclose(fp);
	  return (Int64)rss * (Int64)sysconf(_SC_PAGESIZE);
	#endif
	}  

  //GetOsUsedMemory
  static Int64 GetOsUsedMemory()
	{
	#if WIN32 
	  MEMORYSTATUSEX status;
	  status.dwLength = sizeof(status);
	  GlobalMemoryStatusEx(&status);
	  return status.ullTotalPhys - status.ullAvailPhys;
	#elif __APPLE__
	  vm_statistics_data_t vm_stats;
	  mach_port_t mach_port = mach_host_self();
	  mach_msg_type_number_t count = sizeof(vm_stats) / sizeof(natural_t);
	  vm_size_t page_size;
	  host_page_size(mach_port, &page_size);
	  host_statistics(mach_port, HOST_VM_INFO, (host_info_t)&vm_stats, &count);
	  return ((int64_t)vm_stats.active_count +
	    (int64_t)vm_stats.inactive_count +
	    (int64_t)vm_stats.wire_count) * (int64_t)page_size;
	#else
	  struct sysinfo memInfo;
	  sysinfo(&memInfo);
	
	  Int64 ret = memInfo.totalram - memInfo.freeram;
	  //Read /proc/meminfo to get cached ram (freed upon request)
	  FILE* fp;
	  int MAXLEN = 1000;
	  char buf[MAXLEN];
	  fp = fopen("/proc/meminfo", "r");
	
	  if (fp) {
	    for (int i = 0; i <= 3; i++) {
	      if (fgets(buf, MAXLEN, fp) == nullptr)
	        buf[0] = 0;
	    }
	    char* p1 = strchr(buf, (int)':');
	    unsigned long cacheram = strtoull(p1 + 1, NULL, 10) * 1000;
	    ret -= cacheram;
	    fclose(fp);
	  }
	
	  ret *= memInfo.mem_unit;
	  return ret;
	#endif
	}  


  //Utils::
  static double GetRandDouble(double a, double b) 
	{
	#if WIN32 
	  {return a + (((double)rand()) / (double)RAND_MAX) * (b - a); }
	#else
	  {return a + drand48() * (b - a); }
	#endif
	}  

  //createDirectory
  static bool createDirectory(String dirname)
	{
	#if WIN32 
	  return CreateDirectory(TEXT(dirname.c_str()), NULL) != 0;
	#else
	  return ::mkdir(dirname.c_str(), 0775) == 0; //user(rwx) group(rwx) others(r-x)
	#endif
	}  
	
	#if WIN32 
	static String Win32FormatErrorMessage(DWORD ErrorCode)
	{
	  TCHAR* buff = nullptr;
	  const int flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
	  auto language_id = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT);
	  DWORD   len = FormatMessage(flags, nullptr, ErrorCode, language_id, reinterpret_cast<LPTSTR>(&buff), 0, nullptr);
	  String ret(buff, len);
	  LocalFree(buff);
	  return ret;
	}
	#endif	
	
  //removeDirectory
  static bool removeDirectory(String value)
	{
#if WIN32
		String cmd = cstring("rmdir /Q /S ", StringUtils::replaceAll(value, "/", "\\"));
#else
		String cmd = cstring("rm -Rf ", StringUtils::replaceAll(value, "\\", "/"));
#endif
		return ::system(cmd.c_str()) == 0 || ::system(cmd.c_str()) == 0; //try 2 times
	}  

  //createLink
  static bool createLink(String existing_file, String new_file)
	{
	#if WIN32 
	  if (CreateHardLink(new_file.c_str(), existing_file.c_str(), nullptr) == 0)
	  {
	    PrintWarning("Error creating link", Win32FormatErrorMessage(GetLastError()));
	    return false;
	  }
	  return true;
	#else
	  return symlink(existing_file.c_str(), new_file.c_str()) == 0;
	#endif
	}
  

  //getTimeStamp
  static Int64 getTimeStamp()
	{
	#if WIN32 
	  struct _timeb t;
	#ifdef _INC_TIME_INL
	  _ftime_s(&t);
	#else
	  _ftime(&t);
	#endif
	  return ((Int64)t.time) * 1000 + t.millitm;
	#else
	  struct timeval tv;
	  gettimeofday(&tv, nullptr);
	  return ((Int64)tv.tv_sec) * 1000 + tv.tv_usec / 1000;
	#endif
	}  
	

  //millisToLocal
  static struct tm millisToLocal(const Int64 millis)
	{
	  struct tm result;
	  const Int64 seconds = millis / 1000;
	
	  if (seconds < 86400LL || seconds >= 2145916800LL)
	  {
	    // use extended maths for dates beyond 1970 to 2037..
	    const int timeZoneAdjustment = 31536000 - (int)(Time(1971, 0, 1, 0, 0).getUTCMilliseconds() / 1000);
	    const Int64 jdm = seconds + timeZoneAdjustment + 210866803200LL;
	
	    const int days = (int)(jdm / 86400LL);
	    const int a = 32044 + days;
	    const int b = (4 * a + 3) / 146097;
	    const int c = a - (b * 146097) / 4;
	    const int d = (4 * c + 3) / 1461;
	    const int e = c - (d * 1461) / 4;
	    const int m = (5 * e + 2) / 153;
	
	    result.tm_mday = e - (153 * m + 2) / 5 + 1;
	    result.tm_mon = m + 2 - 12 * (m / 10);
	    result.tm_year = b * 100 + d - 6700 + (m / 10);
	    result.tm_wday = (days + 1) % 7;
	    result.tm_yday = -1;
	
	    int t = (int)(jdm % 86400LL);
	    result.tm_hour = t / 3600;
	    t %= 3600;
	    result.tm_min = t / 60;
	    result.tm_sec = t % 60;
	    result.tm_isdst = -1;
	  }
	  else
	  {
	    time_t now = static_cast <time_t> (seconds);
	
	#if WIN32 
	#ifdef _INC_TIME_INL
	    if (now >= 0 && now <= 0x793406fff)
	      localtime_s(&result, &now);
	    else
	      memset(&result, 0, sizeof(result));
	#else
	    result = *localtime(&now);
	#endif
	#else
	    localtime_r(&now, &result); // more thread-safe
	#endif
	  }
	
	  return result;
	}  	

  //safe_strerror
  static String safe_strerror(int err)
	{
	  const int buffer_size = 512;
	  char buf[buffer_size];
	#if WIN32 
	  strerror_s(buf, sizeof(buf), err);
	#else
	  if (strerror_r(err, buf, sizeof(buf)) != 0)
	    buf[0] = 0;
	#endif
	
	  return String(buf);
	}  

  //CurrentWorkingDirectory
  static String CurrentWorkingDirectory()
	{
	#if WIN32 
	  {
	    char buff[2048];
	    ::GetCurrentDirectory(sizeof(buff), buff);
	    std::replace(buff, buff + sizeof(buff), '\\', '/');
	    return buff;
	  }
	#else
	  {
	    char buff[2048];
	    return getcwd(buff, sizeof(buff));
	  }
	#endif
	}  

  //PrintMessageToTerminal
  static void PrintMessageToTerminal(const String& value)
	{
	#if WIN32 
	  OutputDebugStringA(value.c_str());
	#endif
	  std::cout << value;
	}  

  //getHomeDirectory
  static String getHomeDirectory()
	{
	#if WIN32 
	  {
	    char buff[2048]; 
	    memset(buff, 0, sizeof(buff));
	    SHGetSpecialFolderPath(0, buff, CSIDL_PERSONAL, FALSE);
	    return buff;
	  }
	#else
	  {
	    if (auto homedir = getenv("HOME"))
	      return homedir;
	
	    else if (auto pw = getpwuid(getuid()))
	      return pw->pw_dir;

			ThrowException("internal error");
			return "/";
	  }
	#endif
	}  

  //getCurrentApplicationFile
  static String getCurrentApplicationFile()
	{
	#if WIN32 
	  //see https://stackoverflow.com/questions/6924195/get-dll-path-at-runtime
	  HMODULE handle;
	  GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)__do_not_remove_my_function__, &handle);
	  VisusReleaseAssert(handle);
	  char buff[2048];
	  memset(buff, 0, sizeof(buff));
	  GetModuleFileName(handle, buff, sizeof(buff));
	  return buff;
	#else
	  Dl_info dlInfo;
	  dladdr((const void*)__do_not_remove_my_function__, &dlInfo);
	  VisusReleaseAssert(dlInfo.dli_sname && dlInfo.dli_saddr);
	  return dlInfo.dli_fname;
	#endif
	} 
  
  //setBitThreadSafe
  static void setBitThreadSafe(unsigned char* buffer, Int64 bit, bool value)
	{
	  volatile char* Byte = (char*)buffer + (bit >> 3);
	  char Mask = 1 << (bit & 0x07);
	#if WIN32
	  value ? _InterlockedOr8(Byte, Mask) : _InterlockedAnd8(Byte, ~Mask);
	#else
	  value ? __sync_fetch_and_or(Byte, Mask) : __sync_fetch_and_and(Byte, ~Mask);
	#endif
	}  

  //startup
  static void startup()
	{
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
	}


	//findFilesInDirectory
	static std::vector<String> findFilesInDirectory(const String& directory)
	{
		std::vector<String> out;

#if WIN32
		WIN32_FIND_DATA file_data;
		HANDLE dir = FindFirstFile((directory + "/*").c_str(), &file_data);
		if (dir == INVALID_HANDLE_VALUE)
			return out;
		do
		{
			const String file_name = file_data.cFileName;
			const String full_file_name = directory + "/" + file_name;
			const bool is_directory = (file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

			if (is_directory)
				continue;

			out.push_back(full_file_name);
		}   
		while (FindNextFile(dir, &file_data));
		FindClose(dir);

#else

		auto dir = opendir(directory.c_str());

		struct dirent* ent = nullptr;
		while ((ent = readdir(dir)) != NULL)
		{
			const String file_name = ent->d_name;
			const String full_file_name = directory + "/" + file_name;

			class stat st;
			if (stat(full_file_name.c_str(), &st) == -1)
				continue;

			const bool is_directory = (st.st_mode & S_IFDIR) != 0;

			if (is_directory)
				continue;

			out.push_back(full_file_name);
		}
		closedir(dir);
#endif

		return out;
	}


}; //end class

}//namespace Visus

#endif //__VISUS_OS_DEP_H__


