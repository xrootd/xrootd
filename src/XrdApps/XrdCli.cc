/******************************************************************************//*                                                                            *//*                         X r d C l i . c c                                  *//*                                                                            *//* (c) 2026 by the XRootD Collaboration                                       *//*                                                                            *//* This file is part of the XRootD software suite.                            *//*                                                                            *//* XRootD is free software: you can redistribute it and/or modify it under    *//* the terms of the GNU Lesser General Public License as published by the     *//* Free Software Foundation, either version 3 of the License, or (at your     *//* option) any later version.                                                 *//*                                                                            *//* XRootD is distributed in the hope that it will be useful, but WITHOUT      *//* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or      *//* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public       *//* License for more details.                                                  *//*                                                                            *//* You should have received a copy of the GNU Lesser General Public License   *//* along with XRootD in a file called COPYING.LESSER (LGPL license) and file  *//* COPYING (GPL license).  If not, see <http://www.gnu.org/licenses/>.        *//*                                                                            *//******************************************************************************/

#include "XrdVersion.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClFileSystem.hh"
#include "XrdCl/XrdClURL.hh"
#include "XrdCl/XrdClXRootDResponses.hh"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <limits.h>
#include <memory>
#include <sstream>
#include <string>
#include <sys/stat.h>

#include <CLI/CLI.hpp>

#ifndef _WIN32
#include <unistd.h>
#endif

namespace
{
struct StatOptions
{
  std::string path;
  int timeout = -1;
};

void SetEnvironment(const char *name, const std::string &value)
{
#ifdef _WIN32
  _putenv_s(name, value.c_str());
#else
  setenv(name, value.c_str(), 1);
#endif
}

bool HasScheme(const std::string &path)
{
  return path.find("://") != std::string::npos;
}

std::string LocalDisplayPath(const std::string &localPath)
{
  char resolved[PATH_MAX];
  if(::realpath(localPath.c_str(), resolved) != nullptr)
  {
    return std::string("file://") + resolved;
  }
  return std::string("file://") + localPath;
}

std::string FileType(const XrdCl::StatInfo &info)
{
  if(info.TestFlags(XrdCl::StatInfo::IsDir)) return "directory";
  if(info.TestFlags(XrdCl::StatInfo::Other)) return "unknown";
  return "regular file";
}

char FileTypeChar(const XrdCl::StatInfo &info)
{
  return info.TestFlags(XrdCl::StatInfo::IsDir) ? 'd' : '-';
}

std::string FileType(mode_t mode)
{
  if(S_ISBLK(mode)) return "block device";
  if(S_ISCHR(mode)) return "character device";
  if(S_ISDIR(mode)) return "directory";
  if(S_ISFIFO(mode)) return "fifo";
  if(S_ISLNK(mode)) return "symbolic link";
  if(S_ISREG(mode)) return "regular file";
  if(S_ISSOCK(mode)) return "socket";
  return "unknown";
}

char FileTypeChar(mode_t mode)
{
  if(S_ISBLK(mode)) return 'b';
  if(S_ISCHR(mode)) return 'c';
  if(S_ISDIR(mode)) return 'd';
  if(S_ISFIFO(mode)) return 'p';
  if(S_ISLNK(mode)) return 'l';
  if(S_ISSOCK(mode)) return 's';
  return '-';
}

std::string ModeTriplet(mode_t mode, mode_t read, mode_t write, mode_t execute)
{
  std::string result;
  result += (mode & read) ? "r" : "-";
  result += (mode & write) ? "w" : "-";
  result += (mode & execute) ? "x" : "-";
  return result;
}

std::string ModeString(mode_t mode)
{
  std::string result;
  result += FileTypeChar(mode);
  result += ModeTriplet(mode, S_IRUSR, S_IWUSR, S_IXUSR);
  result += ModeTriplet(mode, S_IRGRP, S_IWGRP, S_IXGRP);
  result += ModeTriplet(mode, S_IROTH, S_IWOTH, S_IXOTH);
  return result;
}

std::string ModeOctal(mode_t mode)
{
  std::ostringstream out;
  out << std::setfill('0') << std::setw(4) << std::oct
      << (mode & 07777);
  return out.str();
}

std::string BasicModeString(const XrdCl::StatInfo &info)
{
  std::string mode;
  mode += FileTypeChar(info);
  mode += info.TestFlags(XrdCl::StatInfo::IsReadable) ? "r" : "-";
  mode += info.TestFlags(XrdCl::StatInfo::IsWritable) ? "w" : "-";
  mode += info.TestFlags(XrdCl::StatInfo::XBitSet) ? "x" : "-";
  mode += "------";
  return mode;
}

std::string ModeString(const XrdCl::StatInfo &info)
{
  if(info.ExtendedFormat())
  {
    return std::string(1, FileTypeChar(info)) + info.GetModeAsOctString();
  }
  return BasicModeString(info);
}

std::string ModeOctal(const XrdCl::StatInfo &info)
{
  if(info.ExtendedFormat()) return info.GetModeAsString();

  unsigned int mode = 0;
  if(info.TestFlags(XrdCl::StatInfo::IsReadable)) mode |= 0400;
  if(info.TestFlags(XrdCl::StatInfo::IsWritable)) mode |= 0200;
  if(info.TestFlags(XrdCl::StatInfo::XBitSet)) mode |= 0100;

  std::ostringstream out;
  out << std::setfill('0') << std::setw(4) << std::oct << mode;
  return out.str();
}

std::string FormatTimestamp(uint64_t timestamp)
{
  std::time_t seconds = static_cast<std::time_t>(timestamp);
  std::tm tm;

#ifdef _WIN32
  localtime_s(&tm, &seconds);
#else
  localtime_r(&seconds, &tm);
#endif

  char buffer[32];
  std::strftime(buffer, sizeof(buffer), "%F %T", &tm);
  return std::string(buffer) + ".000000";
}

XrdCl::URL FileSystemURL(XrdCl::URL url)
{
  url.SetPath("");
  url.SetParams(XrdCl::URL::ParamsMap{});
  return url;
}

int RunLocalStat(const std::string &originalPath, const std::string &localPath)
{
  struct stat statBuffer;
  if(::stat(localPath.c_str(), &statBuffer) != 0)
  {
    const int err = errno;
    std::cerr << "xrd stat error: " << err << " (" << std::strerror(err)
              << ") - errno reported by local system call "
              << std::strerror(err) << '\n';
    return err;
  }

  std::cout << "  File: '" << LocalDisplayPath(localPath) << "'\n";
  std::cout << "  Size: " << statBuffer.st_size << '\t'
            << FileType(statBuffer.st_mode) << '\n';
  std::cout << "Access: (" << ModeOctal(statBuffer.st_mode) << "/"
            << ModeString(statBuffer.st_mode) << ")\tUid: "
            << statBuffer.st_uid << "\tGid: " << statBuffer.st_gid << "\t\n";
  std::cout << "Access: " << FormatTimestamp(statBuffer.st_atime) << '\n';
  std::cout << "Modify: " << FormatTimestamp(statBuffer.st_mtime) << '\n';
  std::cout << "Change: " << FormatTimestamp(statBuffer.st_ctime) << '\n';
  return 0;
}

int RunRemoteStat(const std::string &path)
{
  XrdCl::URL url(path);
  if(!url.IsValid())
  {
    std::cerr << "xrd stat: invalid URL '" << path << "'\n";
    return 64;
  }

  if(url.IsLocalFile())
  {
    return RunLocalStat(path, url.GetPath());
  }

  XrdCl::FileSystem fs(FileSystemURL(url));
  XrdCl::StatInfo *rawInfo = nullptr;
  XrdCl::XRootDStatus status = fs.Stat(url.GetPathWithParams(), rawInfo);
  std::unique_ptr<XrdCl::StatInfo> info(rawInfo);

  if(!status.IsOK())
  {
    std::cerr << "xrd stat: unable to stat '" << path
              << "': " << status.ToStr() << '\n';
    return status.GetShellCode();
  }

  std::cout << "  File: '" << path << "'\n";
  std::cout << "  Size: " << info->GetSize() << '\t' << FileType(*info) << '\n';
  std::cout << "Access: (" << ModeOctal(*info) << "/" << ModeString(*info)
            << ")\tUid: " << info->GetOwner()
            << "\tGid: " << info->GetGroup() << "\t\n";

  const auto accessTime = info->ExtendedFormat()
    ? info->GetAccessTime()
    : info->GetModTime();
  const auto changeTime = info->ExtendedFormat()
    ? info->GetChangeTime()
    : info->GetModTime();

  std::cout << "Access: " << FormatTimestamp(accessTime) << '\n';
  std::cout << "Modify: " << FormatTimestamp(info->GetModTime()) << '\n';
  std::cout << "Change: " << FormatTimestamp(changeTime) << '\n';
  return 0;
}

void SetVerbose(unsigned int verbosity)
{
  if(verbosity == 0) return;
  if(verbosity == 1) XrdCl::DefaultEnv::SetLogLevel("Warning");
  else if(verbosity == 2) XrdCl::DefaultEnv::SetLogLevel("Info");
  else XrdCl::DefaultEnv::SetLogLevel("Debug");
}

void ApplyClientOptions(int timeout, const std::string &cert,
                        const std::string &key, bool ipv4, bool ipv6)
{
  if(timeout >= 0)
  {
    XrdCl::DefaultEnv::GetEnv()->PutInt("RequestTimeout", timeout);
  }

  if(!cert.empty())
  {
    if(key.empty())
    {
      SetEnvironment("X509_USER_PROXY", cert);
      XrdCl::DefaultEnv::GetEnv()->PutString("HttpClientCertFile", cert);
      XrdCl::DefaultEnv::GetEnv()->PutString("HttpClientKeyFile", cert);
    }
    else
    {
      SetEnvironment("X509_USER_CERT", cert);
      XrdCl::DefaultEnv::GetEnv()->PutString("HttpClientCertFile", cert);
    }
  }

  if(!key.empty())
  {
    SetEnvironment("X509_USER_KEY", key);
    XrdCl::DefaultEnv::GetEnv()->PutString("HttpClientKeyFile", key);
  }

  if(ipv4 && !ipv6)
  {
    XrdCl::DefaultEnv::GetEnv()->PutString("NetworkStack", "IPv4");
    XrdCl::DefaultEnv::GetEnv()->PutInt("PreferIPv4", 1);
  }
  else if(ipv6 && !ipv4)
  {
    XrdCl::DefaultEnv::GetEnv()->PutString("NetworkStack", "IPv6");
    XrdCl::DefaultEnv::GetEnv()->PutInt("PreferIPv4", 0);
  }
}

int RunStat(const StatOptions &options, unsigned int verbosity = 0,
            const std::string &logFile = "", const std::string &cert = "",
            const std::string &key = "", bool ipv4 = false,
            bool ipv6 = false)
{
  if(!logFile.empty() && !XrdCl::DefaultEnv::SetLogFile(logFile))
  {
    std::cerr << "xrd stat: unable to open log file '" << logFile << "'\n";
    return 1;
  }

  SetVerbose(verbosity);
  ApplyClientOptions(options.timeout, cert, key, ipv4, ipv6);

  if(HasScheme(options.path)) return RunRemoteStat(options.path);
  return RunLocalStat(options.path, options.path);
}


}

int main(int argc, char **argv)
{
  CLI::App app{"XRootD command-line client."};
  app.name("xrd");
  app.set_help_flag("-h,--help", "Show this message and exit");
  bool showVersion = false;
  app.add_flag("--version", showVersion, "Show version information and exit");

  int exitCode = 0;
  std::string statPath;
  int statTimeout = -1;
  unsigned int statVerbosity = 0;
  bool statVersion = false;
  bool statIPv4 = false;
  bool statIPv6 = false;
  std::string statDefinition;
  std::string statCert;
  std::string statKey;
  std::string statClientInfo;
  std::string statLogFile;

  auto *stat = app.add_subcommand("stat",
    "Display extended information about a file or directory");
  stat->add_option("file", statPath,
    "URL of the file or directory to stat");
  stat->add_flag("-V,--version", statVersion,
    "Output version information and exit");
  stat->add_flag("-v,--verbose", statVerbosity,
    "Enable verbose client logging");
  stat->add_option("-D,--definition", statDefinition,
    "Accept a GFAL parameter override");
  stat->add_option("-t,--timeout", statTimeout,
    "Maximum operation time in seconds");
  stat->add_option("-E,--cert", statCert,
    "Accept a user certificate path");
  stat->add_option("--key", statKey,
    "Accept a user private key path");
  stat->add_flag("-4", statIPv4,
    "Accept the GFAL IPv4-only flag");
  stat->add_flag("-6", statIPv6,
    "Accept the GFAL IPv6-only flag");
  stat->add_option("-C,--client-info", statClientInfo,
    "Accept custom client information");
  stat->add_option("--log-file", statLogFile,
    "Write XRootD client logs to a file");
  stat->callback([&] {
    if(statVersion)
    {
      std::cout << "xrd " << XrdVERSION << '\n';
      exitCode = 0;
      return;
    }
    if(statPath.empty())
    {
      std::cerr << "xrd stat: expected one file URL\n";
      exitCode = 64;
      return;
    }
    StatOptions options;
    options.path = statPath;
    options.timeout = statTimeout;
    exitCode = RunStat(options, statVerbosity, statLogFile, statCert,
                       statKey, statIPv4, statIPv6);
  });

  CLI11_PARSE(app, argc, argv);

  if(showVersion)
  {
    std::cout << "xrd " << XrdVERSION << '\n';
    return 0;
  }

  if(app.get_subcommands().empty())
  {
    std::cout << app.help();
  }
  return exitCode;
}
