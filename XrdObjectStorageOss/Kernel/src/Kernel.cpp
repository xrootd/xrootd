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

#include <Visus/Kernel.h>

#include <Visus/Thread.h>
#include <Visus/NetService.h>
#include <Visus/RamResource.h>
#include <Visus/Path.h>
#include <Visus/File.h>
#include <Visus/Encoder.h>
#include <Visus/StringTree.h>
#include <Visus/NetService.h>
#include "osdep.hxx"

#include <assert.h>
#include <type_traits>
#include <iomanip>
#include <fstream>
#include <iostream>
#include <atomic>
#include <clocale>
#include <cctype>


#include <Visus/Frustum.h>
#include <Visus/Graph.h>

#include <Visus/KdArray.h>
#include <Visus/TransferFunction.h>
#include <Visus/Statistics.h>
#include <Visus/Array.h>

#include <clocale>

#include "EncoderId.hxx"
#include "EncoderLz4.hxx"
#include "EncoderZip.hxx"
#include "EncoderZfp.hxx"

#include "ArrayPluginDevnull.hxx"
#include "ArrayPluginRawArray.hxx"

#if VISUS_IMAGE
#  include <FreeImage.h>
#  include "ArrayPluginFreeimage.hxx"
#  include "EncoderFreeImage.hxx"
#endif


namespace Visus {
#define __str__(s) #s
#define __xstr__(s) __str__(s)

String OpenVisus_VERSION = "";

#ifdef GIT_REVISION
String OpenVisus_GIT_REVISION = __xstr__(GIT_REVISION);
#else
String OpenVisus_GIT_REVISION = "";
#endif

std::vector<String> CommandLine::args;

ConfigFile* VisusModule::getModuleConfig() {
  return Private::VisusConfig::getSingleton();
}


//////////////////////////////////////////////////////////////////
static std::pair< void (*)(String msg, void*), void*> __redirect_log__;

void RedirectLogTo(void(*callback)(String msg, void*), void* user_data) {
  __redirect_log__ = std::make_pair(callback, user_data);
}

void PrintMessageToTerminal(const String& value) {
  osdep::PrintMessageToTerminal(value);
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

int          CommandLine::argn=0;
const char** CommandLine::argv ;

static String visus_config_commandline_filename;

///////////////////////////////////////////////////////////////////////////////
String cstring10(double value) {
  std::ostringstream out;
  out << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
  return out.str();
}

///////////////////////////////////////////////////////////////////////////////
void SetCommandLine(int argn, const char** argv)
{
  //already called (can happen with embedded python, when I fist call the C++ SetCommandLine, and then I call the Python one)
  //C++ has precedence
  if (!CommandLine::args.empty())
    return;

  CommandLine::argn = argn;
  CommandLine::argv = argv;

  // parse command line
  for (int I = 0; I < argn; I++)
  {
    //override visus.config
    if (argv[I] == String("--visus-config") && I < (argn - 1))
    {
      visus_config_commandline_filename = argv[++I];
      continue;
    }

    //xcode debugger always passes this; just ignore it
    if (argv[I] == String("-NSDocumentRevisionsDebugMode") && I < (argn - 1))
    {
      String ignoring_enabled = argv[++I];
      continue;
    }

    CommandLine::args.push_back(argv[I]);
  }
}

/////////////////////////////////////////////////////
void SetCommandLine(std::vector<String> args)
{
  static std::vector<String> keep_in_memory = args;
  static const int argn = (int)args.size();
  static const char* argv[256];

  memset(argv, 0, sizeof(argv));
  for (int I = 0; I < args.size(); I++)
    argv[I] = args[I].c_str();

  SetCommandLine(argn, argv);
}


/////////////////////////////////////////////////////
bool VisusHasMessageLock()
{
  return Thread::isMainThread();
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




///////////////////////////////////////////////////////////
static void InitKnownPaths()
{
#ifdef VISUS_HOME
  {
    KnownPaths::VisusHome = Path(__xstr__(VISUS_HOME));
    PrintInfo("setting VISUS_HOME", KnownPaths::VisusHome, "from C++ define");
  }
#else

  if (auto VISUS_HOME = getenv("VISUS_HOME"))
  {
    KnownPaths::VisusHome = Path(VISUS_HOME);
    PrintInfo("setting VISUS_HOME", KnownPaths::VisusHome, "from getenv");
  }
  else
  {
    KnownPaths::VisusHome = osdep::getHomeDirectory() + "/visus";
    PrintInfo("setting VISUS_HOME", KnownPaths::VisusHome, "from home directory");
  }
#endif

  FileUtils::createDirectory(KnownPaths::VisusHome);
  KnownPaths::BinaryDirectory = Path(Utils::getCurrentApplicationFile()).getParent();
}


int KernelModule::attached = 0;

//////////////////////////////////////////////////////
void KernelModule::attach()
{
  if ((++attached) > 1) return;

  //check 64 bit file IO is enabled!
#if __GNUC__ && !__APPLE__
  VisusReleaseAssert(sizeof(off_t) == 8);
#endif

  //check types
  VisusReleaseAssert(sizeof(Int8) == 1 && sizeof(Uint8) == 1 && sizeof(char) == 1);
  VisusReleaseAssert(sizeof(Int16) == 2 && sizeof(Uint16) == 2 && sizeof(short) == 2);
  VisusReleaseAssert(sizeof(Int32) == 4 && sizeof(Uint32) == 4 && sizeof(int) == 4);
  VisusReleaseAssert(sizeof(Int64) == 8 && sizeof(Uint64) == 8);
  VisusReleaseAssert(sizeof(Float32) == 4 && sizeof(float) == 4);
  VisusReleaseAssert(sizeof(Float64) == 8 && sizeof(double) == 8);

#pragma pack(push)
#pragma pack(1) 
  typedef struct { Int64 a[1];           Int8 b; }S009;
  typedef struct { Int64 a[2];           Int8 b; }S017;
  typedef struct { Int64 a[2]; Int32 b[1]; Int8 c; }S021;
  typedef struct { Int64 a[4];           Int8 b; }S033;
  typedef struct { Int64 a[4]; Int32 b[1]; Int8 c; }S037;
  typedef struct { Int64 a[8]; Int8 b; }S065;
  typedef struct { Int64 a[16]; Int8 b; }S129;
  typedef struct { Int64 a[16]; Int32 b[1]; Int8 c; }S133;
#pragma pack(pop)

  VisusReleaseAssert(sizeof(S009) == 9);
  VisusReleaseAssert(sizeof(S017) == 17);
  VisusReleaseAssert(sizeof(S021) == 21);
  VisusReleaseAssert(sizeof(S033) == 33);
  VisusReleaseAssert(sizeof(S037) == 37);
  VisusReleaseAssert(sizeof(S065) == 65);
  VisusReleaseAssert(sizeof(S129) == 129);
  VisusReleaseAssert(sizeof(S133) == 133);
  


  osdep::InitAutoReleasePool();

  srand(0);
  std::setlocale(LC_ALL, "en_US.UTF-8");
  Thread::getMainThreadId() = std::this_thread::get_id();

  osdep::startup();

  Private::VisusConfig::allocSingleton();

  NetService::attach();

  InitKnownPaths();

  auto config = getModuleConfig();

  for (auto filename : {
    visus_config_commandline_filename ,
    KnownPaths::CurrentWorkingDirectory() + "/visus.config",
    KnownPaths::VisusHome + "/visus.config" 
    })
  {
    if (filename.empty())
      continue;

    bool bOk = config->load(filename);
#if _DEBUG
    PrintInfo("VisusConfig filename",filename,"ok",bOk ? "YES" : "NO");
#endif
    if (bOk) break;
  }

  PrintInfo(
    "VERSION", OpenVisus_VERSION,
    "GIT_REVISION", OpenVisus_GIT_REVISION,
    "VisusHome", KnownPaths::VisusHome, 
    "BinaryDirectory", KnownPaths::BinaryDirectory,
    "CurrentWorkingDirectory ", KnownPaths::CurrentWorkingDirectory());

  ArrayPlugins::allocSingleton();
  Encoders::allocSingleton();
  RamResource::allocSingleton();

  //in case the user whant to simulate I have a certain amount of RAM
  if (Int64 total = StringUtils::getByteSizeFromString(config->readString("Configuration/RamResource/total", "0")))
    RamResource::getSingleton()->setOsTotalMemory(total);

  NetService::Defaults::proxy = config->readString("Configuration/NetService/proxy");
  NetService::Defaults::proxy_port = cint(config->readString("Configuration/NetService/proxyport"));

  NetSocket::Defaults::send_buffer_size = config->readInt("Configuration/NetSocket/send_buffer_size");
  NetSocket::Defaults::recv_buffer_size = config->readInt("Configuration/NetSocket/recv_buffer_size");
  NetSocket::Defaults::tcp_no_delay = config->readBool("Configuration/NetSocket/tcp_no_delay", true);

  //array plugins
  {
    ArrayPlugins::getSingleton()->values.push_back(std::make_shared<DevNullArrayPlugin>());
    ArrayPlugins::getSingleton()->values.push_back(std::make_shared<RawArrayPlugin>());

#if VISUS_IMAGE
    ArrayPlugins::getSingleton()->values.push_back(std::make_shared<FreeImageArrayPlugin>());
#endif
  }

  //encoders
  {
    Encoders::getSingleton()->registerEncoder("",    [](String specs) {return std::make_shared<IdEncoder>(specs); }); 
    Encoders::getSingleton()->registerEncoder("raw", [](String specs) {return std::make_shared<IdEncoder>(specs); });
    Encoders::getSingleton()->registerEncoder("bin", [](String specs) {return std::make_shared<IdEncoder>(specs); });
    Encoders::getSingleton()->registerEncoder("lz4", [](String specs) {return std::make_shared<LZ4Encoder>(specs); });
    Encoders::getSingleton()->registerEncoder("zip", [](String specs) {return std::make_shared<ZipEncoder>(specs); });
    Encoders::getSingleton()->registerEncoder("zfp", [](String specs) {return std::make_shared<ZfpEncoder>(specs); });

#if VISUS_IMAGE
    Encoders::getSingleton()->registerEncoder("png", [](String specs) {return std::make_shared<FreeImageEncoder>(specs); });
    Encoders::getSingleton()->registerEncoder("jpg", [](String specs) {return std::make_shared<FreeImageEncoder>(specs); });
    Encoders::getSingleton()->registerEncoder("tif", [](String specs) {return std::make_shared<FreeImageEncoder>(specs); });
#endif
  }
  
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
  if ((--attached) > 0) return;

  ArrayPlugins::releaseSingleton();
  Encoders::releaseSingleton();
  RamResource::releaseSingleton();

  NetService::detach();

  Private::VisusConfig::releaseSingleton();

  osdep::DestroyAutoReleasePool();
}

} //namespace Visus


