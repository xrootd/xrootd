//------------------------------------------------------------------------------
// Copyright (c) 2011-2017 by European Organization for Nuclear Research (CERN)
// Author: Michal Simon <michal.simon@cern.ch>
//------------------------------------------------------------------------------
// This file is part of the XRootD software suite.
//
// XRootD is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// XRootD is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with XRootD.  If not, see <http://www.gnu.org/licenses/>.
//
// In applying this licence, CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#ifndef SRC_XRDAPPS_RECORDPLUGIN_XRDCLACTION_HH_
#define SRC_XRDAPPS_RECORDPLUGIN_XRDCLACTION_HH_

#include <sstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include "XrdCl/XrdClXRootDResponses.hh"

namespace XrdCl
{
//----------------------------------------------------------------------------
//! Action
//----------------------------------------------------------------------------
struct Action
{
  //--------------------------------------------------------------------------
  // Constructor
  // @param file    : pointer to the file plug-in that recorded the action
  //                  (to be used as an ID)
  // @param timeout : operation timeout (common for every operation)
  //--------------------------------------------------------------------------
  Action(void* file, uint16_t timeout)
  : id(reinterpret_cast<uint64_t>(file))
  , timeout(timeout)
  , start(std::chrono::system_clock::now())  // register the action start time
  {
  }

  //--------------------------------------------------------------------------
  //! Record the server response / error / timeout
  //--------------------------------------------------------------------------
  inline void RecordResult(XRootDStatus* st, AnyObject* rsp)
  {
    stop   = std::chrono::system_clock::now();  // register the response time
    status = *st;
    Serialize(rsp);
  }

  //--------------------------------------------------------------------------
  //! Convert timpoint to unix timestamp with ns
  //--------------------------------------------------------------------------
  static inline double time(
    std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> tp)
  {
    auto secs = std::chrono::time_point_cast<std::chrono::seconds>(tp);
    auto ns   = std::chrono::time_point_cast<std::chrono::nanoseconds>(tp)
              - std::chrono::time_point_cast<std::chrono::nanoseconds>(secs);
    return secs.time_since_epoch().count() + ns.count() / 1000000000.0;
  }

  //--------------------------------------------------------------------------
  //! Get curretn unix time in ns precision as a double
  //--------------------------------------------------------------------------
  static inline double timeNow()
  {
    auto now = std::chrono::system_clock::now();
    return time(now);
  }


  //--------------------------------------------------------------------------
  //! Convert the action / response data into csv row
  //--------------------------------------------------------------------------
  inline std::string ToString()
  {
    std::stringstream ss;
    ss << "\"" << id     << "\"" << ',';
    ss << "\"" << Name() << "\"" << ',';

    double tstart = time(start);
    double tstop  = time(stop);
    ss << "\"" << std::fixed << std::setprecision(9) << tstart << "\"" << ",";
    std::string argstr = ArgStr();
    if (!argstr.empty())
      argstr += ';';
    ss << "\"" << argstr << timeout << "\"" << ',';
    ss << "\"" << std::fixed << std::setprecision(9) << tstop  << "\"" << ",";
    auto ststr = status.ToString();
    while (ststr.back() == ' ')
      ststr.pop_back();
    ss << "\"" << ststr << "\"" << ',';
    ss << "\"" << serialrsp << "\"" << '\n';
    return ss.str();
  }

  //--------------------------------------------------------------------------
  //! Destructor
  //--------------------------------------------------------------------------
  virtual ~Action() {}

  //--------------------------------------------------------------------------
  //! Action name
  //--------------------------------------------------------------------------
  virtual std::string Name() = 0;

  //--------------------------------------------------------------------------
  //! Convert operation arguments into a string
  //--------------------------------------------------------------------------
  virtual std::string ArgStr() = 0;

  //--------------------------------------------------------------------------
  //! Serialize server response
  //--------------------------------------------------------------------------
  virtual void Serialize(AnyObject* response) {}

  uint64_t                              id;         //> File object ID
  uint16_t                              timeout;    //> operation timeout
  std::chrono::system_clock::time_point start;      //> start time
  XRootDStatus                          status;     //> operation status
  std::string                           serialrsp;  //> serialized response
  std::chrono::system_clock::time_point stop;       //> response time
};

//----------------------------------------------------------------------------
//! Open action
//----------------------------------------------------------------------------
struct OpenAction : public Action
{
  OpenAction(
    void* file, const std::string& url, OpenFlags::Flags flags, Access::Mode mode, uint16_t timeout)
  : Action(file, timeout)
  , url(url)
  , flags(flags)
  , mode(mode)
  {
  }

  std::string Name() { return "Open"; }

  std::string ArgStr()
  {
    std::stringstream ss;
    ss << url << ';';
    ss << flags << ';';
    ss << mode;
    return ss.str();
  }

  const std::string url;
  OpenFlags::Flags  flags;
  Access::Mode      mode;
};

//----------------------------------------------------------------------------
//! Close action
//----------------------------------------------------------------------------
struct CloseAction : public Action
{
  CloseAction(void* file, uint16_t timeout)
  : Action(file, timeout)
  {
  }

  std::string Name() { return "Close"; }

  std::string ArgStr() { return {}; }
};

//----------------------------------------------------------------------------
//! Stat action
//----------------------------------------------------------------------------
struct StatAction : public Action
{
  StatAction(void* file, bool force, uint16_t timeout)
  : Action(file, timeout)
  , force(force)
  {
  }

  std::string Name() { return "Stat"; }

  std::string ArgStr() { return force ? "true" : "false"; }

  void Serialize(AnyObject* response)
  {
    if (!response)
      return;
    StatInfo* info = nullptr;
    response->Get(info);
    std::stringstream ss;
    ss << std::to_string(info->GetSize()) << ';';
    ss << std::to_string(info->GetFlags()) << ';';
    ss << info->GetModTime() << ';';
    ss << info->GetChangeTime() << ';';
    ss << info->GetAccessTime() << ';';
    ss << info->GetModeAsOctString() << ';';
    ss << info->GetOwner() << ';';
    ss << info->GetGroup() << ';';
    ss << info->GetChecksum();
    serialrsp = ss.str();
  }

  bool force;
};

//----------------------------------------------------------------------------
//! Read action
//----------------------------------------------------------------------------
struct ReadAction : public Action
{
  ReadAction(void* file, uint64_t offset, uint32_t size, uint16_t timeout)
  : Action(file, timeout)
  , offset(offset)
  , size(size)
  {
  }

  std::string Name() { return "Read"; }

  std::string ArgStr() { return std::to_string(offset) + ';' + std::to_string(size); }

  void Serialize(AnyObject* response)
  {
    if (!response)
      return;
    ChunkInfo* ptr = nullptr;
    response->Get(ptr);
    serialrsp = std::to_string(ptr->length);
  }

  uint64_t offset;
  uint32_t size;
};

struct PgReadAction : public Action
{
  PgReadAction(void* file, uint64_t offset, uint32_t size, uint16_t timeout)
  : Action(file, timeout)
  , offset(offset)
  , size(size)
  {
  }

  std::string Name() { return "PgRead"; }

  std::string ArgStr() { return std::to_string(offset) + ';' + std::to_string(size); }

  void Serialize(AnyObject* response)
  {
    if (!response)
      return;
    PageInfo* ptr = nullptr;
    response->Get(ptr);
    serialrsp = std::to_string(ptr->GetLength()) + ';' + std::to_string(ptr->GetNbRepair());
  }

  uint64_t offset;
  uint32_t size;
};

//----------------------------------------------------------------------------
//! Write action
//----------------------------------------------------------------------------
struct WriteAction : public Action
{
  WriteAction(void* file, uint64_t offset, uint32_t size, uint16_t timeout)
  : Action(file, timeout)
  , offset(offset)
  , size(size)
  {
  }

  std::string Name() { return "Write"; }

  std::string ArgStr() { return std::to_string(offset) + ';' + std::to_string(size); }

  uint64_t offset;
  uint32_t size;
};

struct PgWriteAction : public Action
{
  PgWriteAction(void* file, uint64_t offset, uint32_t size, uint16_t timeout)
  : Action(file, timeout)
  , offset(offset)
  , size(size)
  {
  }

  std::string Name() { return "PgWrite"; }

  std::string ArgStr()
  {
    std::stringstream ss;
    ss << std::to_string(offset) << ';' << std::to_string(size);
    return ss.str();
  }

  uint64_t offset;
  uint32_t size;
};

//----------------------------------------------------------------------------
//! Sync action
//----------------------------------------------------------------------------
struct SyncAction : public Action
{
  SyncAction(void* file, uint16_t timeout)
  : Action(file, timeout)
  {
  }

  std::string Name() { return "Sync"; }

  std::string ArgStr() { return {}; }
};

//----------------------------------------------------------------------------
//! Truncate action
//----------------------------------------------------------------------------
struct TruncateAction : public Action
{
  TruncateAction(void* file, uint64_t size, uint16_t timeout)
  : Action(file, timeout)
  , size(size)
  {
  }

  std::string Name() { return "Truncate"; }

  std::string ArgStr() { return std::to_string(size); }

  uint32_t size;
};

//----------------------------------------------------------------------------
//! VectorRead action
//----------------------------------------------------------------------------
struct VectorReadAction : public Action
{
  VectorReadAction(void* file, const ChunkList& chunks, uint16_t timeout)
  : Action(file, timeout)
  , req(chunks)
  {
  }

  std::string Name() { return "VectorRead"; }

  std::string ArgStr()
  {
    if (req.empty())
      return {};
    std::stringstream ss;
    ss << req[0].offset << ";" << req[0].length;
    for (size_t i = 1; i < req.size(); ++i)
      ss << ";" << req[i].offset << ";" << req[i].length;
    return ss.str();
  }

  void Serialize(AnyObject* response)
  {
    if (!response)
      return;
    VectorReadInfo* ptr = nullptr;
    response->Get(ptr);
    std::stringstream ss;
    ss << ptr->GetSize();
    auto& chunks = ptr->GetChunks();
    for (auto& ch : chunks)
      ss << ';' << ch.offset << ';' << ch.length;
    serialrsp = ss.str();
  }

  ChunkList req;
};

//----------------------------------------------------------------------------
//! Vector Write action
//----------------------------------------------------------------------------
struct VectorWriteAction : public Action
{
  VectorWriteAction(void* file, const ChunkList& chunks, uint16_t timeout)
  : Action(file, timeout)
  , req(chunks)
  {
  }

  std::string Name() { return "VectorWrite"; }

  std::string ArgStr()
  {
    if (req.empty())
      return {};
    std::stringstream ss;
    ss << req[0].offset << ";" << req[0].length;
    for (size_t i = 1; i < req.size(); ++i)
      ss << ";" << req[i].offset << ";" << req[i].length;
    return ss.str();
  }

  ChunkList req;
};

//----------------------------------------------------------------------------
//! Fcntl action
//----------------------------------------------------------------------------
struct FcntlAction : Action
{
  FcntlAction(void* file, const Buffer& arg, uint16_t timeout)
  : Action(file, timeout)
  , req(arg.GetSize())
  {
  }

  std::string Name() { return "Fcntl"; }

  std::string ArgStr() { return std::to_string(req); }

  void Serialize(AnyObject* response)
  {
    if (!response)
      return;
    Buffer* ptr = nullptr;
    response->Get(ptr);
    serialrsp = std::to_string(ptr->GetSize());
  }

  uint32_t req;
};

} /* namespace XrdCl */

#endif /* SRC_XRDAPPS_RECORDPLUGIN_XRDCLACTION_HH_ */
