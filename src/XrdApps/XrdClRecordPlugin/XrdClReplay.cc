//------------------------------------------------------------------------------
// Copyright (c) 2011-2021 by European Organization for Nuclear Research (CERN)
// Author: Michal Simon <michal.simon@cern.ch> 
// Co-Author: Andreas-Joachim Peters <andreas.joachim.peters@cern.ch>
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
//------------------------------------------------------------------------------

#include "XrdCl/XrdClOperations.hh"
#include "XrdCl/XrdClUtils.hh"
#include "XrdCl/XrdClFileOperations.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdClAction.hh"
#include "XrdClActionMetrics.hh"
#include "XrdClReplayArgs.hh"
#include <fstream>
#include <vector>
#include <tuple>
#include <unordered_map>
#include <chrono>
#include <iostream>
#include <thread>
#include <iomanip>
#include <atomic>
#include <stdarg.h>
#include <getopt.h>
#include <map>
#include <vector>
#include <numeric>
#include <mutex>
#include <condition_variable>

namespace XrdCl
{

//------------------------------------------------------------------------------
//! Buffer pool - to limit memory consumption
//------------------------------------------------------------------------------
class BufferPool
{
  public:

    //--------------------------------------------------------------------------
    //! Single instance access
    //--------------------------------------------------------------------------
    static BufferPool& Instance()
    {
      static BufferPool instance;
      return instance;
    }

    //--------------------------------------------------------------------------
    //! Allocate a buffer if has available memory, otherwise wait until enough
    //! memory has been reclaimed
    //--------------------------------------------------------------------------
    std::shared_ptr<std::vector<char>> Allocate( size_t length )
    {
      std::unique_lock<std::mutex> lck( mtx );
      cv.wait( lck, [this, length]{ return available >= length; } );
      available -= length;
      BufferDeleter del;
      std::shared_ptr<std::vector<char>> buffer( new std::vector<char>( length, 'A' ), del );
      return buffer;
    }

  private:

    //--------------------------------------------------------------------------
    //! Reclaim memory
    //--------------------------------------------------------------------------
    void Reclaim( size_t length )
    {
      std::unique_lock<std::mutex> lck(mtx);
      available += length;
      cv.notify_all();
    }

    //--------------------------------------------------------------------------
    //! Custom deleter for shared_ptr (notifies the buffer-pool when memory
    //! has been reclaimed)
    //--------------------------------------------------------------------------
    struct BufferDeleter
    {
      void operator()( std::vector<char> *buff )
      {
        BufferPool::Instance().Reclaim( buff->size() );
        delete buff;
      }
    };

    static const size_t KB = 1024;
    static const size_t MB = 1024 * KB;
    static const size_t GB = 1024 * MB;

    //--------------------------------------------------------------------------
    //! Constructor - read out the memory limit from XRD_MAXBUFFERSIZE, if not
    //! set don't impose any limit
    //--------------------------------------------------------------------------
    BufferPool() : mtx(), cv()
    {
      const char *maxsize = getenv( "XRD_MAXBUFFERSIZE" );
      if( maxsize )
      {
        size_t len = strlen( maxsize );
        size_t pos;
        available = std::stoul( maxsize, &pos );
        std::string sufix( len != pos ? maxsize + len - 2 : "" );
        std::transform( sufix.begin(), sufix.end(), sufix.begin(), ::toupper );
        if( !sufix.empty() )
        {
          if( sufix == "KB" )
            available *= KB;
          else if( sufix == "MB" )
            available *= MB;
          else if( sufix == "GB" )
            available *= GB;
        }
        return;
      }
      available = std::numeric_limits<size_t>::max();
    }

    BufferPool( const BufferPool& ) = delete;
    BufferPool( BufferPool&& ) = delete;

    BufferPool& operator=( const BufferPool& ) = delete;
    BufferPool& operator=( BufferPool& ) = delete;


    size_t                  available;
    std::mutex              mtx;
    std::condition_variable cv;
};

//------------------------------------------------------------------------------
//! Timer helper class
//------------------------------------------------------------------------------
class mytimer_t
{
  public:
  //--------------------------------------------------------------------------
  //! Constructor (record start time)
  //--------------------------------------------------------------------------
  mytimer_t()
  : start(clock_t::now())
  {
  }

  //--------------------------------------------------------------------------
  //! Reset the start time
  //--------------------------------------------------------------------------
  void reset() { start = clock_t::now(); }

  //--------------------------------------------------------------------------
  //! @return : get time elapsed from start
  //--------------------------------------------------------------------------
  double elapsed() const
  {
    return (1.0
            * (std::chrono::duration_cast<std::chrono::nanoseconds>(clock_t::now() - start).count())
            / 1000000000.0);
  }

  private:
  using clock_t = std::chrono::high_resolution_clock;
  std::chrono::time_point<clock_t> start;  //< registered start time
};

//------------------------------------------------------------------------------
//! Barrier for synchronizing the asynchronous execution of actions
//! It is actually a wrapper around semaphore.
//------------------------------------------------------------------------------
class barrier_t
{
  public:
  //------------------------------------------------------------------------
  //! Constructor
  //! @param sem : the semaphore
  //------------------------------------------------------------------------
  barrier_t(XrdSysSemaphore& sem)
  : sem(sem)
  {
  }

  //------------------------------------------------------------------------
  //! Destructor
  //------------------------------------------------------------------------
  ~barrier_t() { sem.Post(); }

  inline XrdSysSemaphore& get() { return sem; }

  private:
  XrdSysSemaphore& sem;  //< the semaphore to be posted
};

//------------------------------------------------------------------------------
//! AssureFile creates input data files on the fly if required
//------------------------------------------------------------------------------
bool AssureFile(const std::string& url, uint64_t size, bool viatruncate, bool verify)
{
  OpenFlags::Flags flags   = OpenFlags::Read;
  Access::Mode     mode    = Access::None;
  uint16_t         timeout = 60;

  {
    // deal with existing files
    auto         file   = std::make_unique<XrdCl::File>(false);
    XRootDStatus status = file->Open(url, flags, mode, timeout);
    if (status.IsOK())
    {
      StatInfo* statinfo;
      // file exists already, verify the size
      status = file->Stat(false, statinfo, timeout);
      if (status.IsOK())
      {
        if (statinfo->GetSize() < size)
        {
          std::cerr
            << "Error: file size is not sufficient, but I won't touch the file - aborting ...";
          return false;
        }
        else
        {
          std::cout << "# ---> info: file exists and has sufficient size" << std::endl;
          return true;
        }
      }
    }
  }

  if (verify)
  {
    std::cerr << "Verify: file is missing or inaccessible: " << url << std::endl;
    return false;
  }

  {
    // deal with non-existing file
    OpenFlags::Flags wflags = OpenFlags::New | OpenFlags::Write | OpenFlags::MakePath;
    Access::Mode     wmode  = Access::UR | Access::UW | Access::UX;
    auto             file   = std::make_unique<XrdCl::File>(false);
    XRootDStatus     status = file->Open(url, wflags, wmode, timeout);
    if (status.IsOK())
    {
      if (viatruncate)
      {
        // create a file via truncation
        status = file->Truncate(size, timeout);
        if (!status.IsOK())
        {
          std::cerr << "Error: " << status.ToString() << " - empty file might be left behind!"
                    << std::endl;
          return false;
        }
        return true;
      }
      else
      {
        // create a file via writes
        using buffer_t = std::vector<uint64_t>;  //< data buffer
        buffer_t buffer(32768);
        size_t   nbytes = 0;

        while (nbytes < size)
        {
          size_t towrite = size - nbytes;
          if (towrite > (buffer.size() * sizeof(uint64_t)))
            towrite = buffer.size() * sizeof(uint64_t);
          for (size_t i = 0; i < buffer.size(); ++i)
          {
            // we write the offset in this buffer
            buffer[i] = nbytes / sizeof(uint64_t) + i;
          }
          status = file->Write(nbytes, towrite, buffer.data(), timeout);
          if (!status.IsOK())
          {
            std::cerr << "Error: " << status.ToString() << " - failed to write file at offset "
                      << nbytes << " - incomplete file might be left behind!" << std::endl;
            return false;
          }
          nbytes += towrite;
        }
      }
      return true;
    }
    else
    {
      std::cerr << "Error: " << status.ToString() << " - failed to create file!" << std::endl;
    }
  }
  return false;
}

//------------------------------------------------------------------------------
//! Executes an action registered in the csv file
//------------------------------------------------------------------------------
class ActionExecutor
{
  using buffer_t = std::shared_ptr<std::vector<char>>;  //< data buffer

  public:
  //--------------------------------------------------------------------------
  //! Constructor
  //! @param file     : the file that should be the context of the action
  //! @param action   : the action to be executed
  //! @param args     : arguments for the action
  //! @param orgststr : original status
  //! @param resp     : original response
  //! @param duration : nominal duration of this action
  //--------------------------------------------------------------------------
  ActionExecutor(File&              file,
                 const std::string& action,
                 const std::string& args,
                 const std::string& orgststr,
                 const std::string& resp,
                 const double&      duration)
  : file(file)
  , action(action)
  , args(args)
  , orgststr(orgststr)
  , nominalduration(duration)
  {
  }

  //--------------------------------------------------------------------------
  //! Execute the action
  //! @param ending  : synchronization object for ending the execution
  //--------------------------------------------------------------------------
  void Execute(std::shared_ptr<barrier_t>& ending,
               std::shared_ptr<barrier_t>& closing,
               ActionMetrics&              metric,
               bool                        simulate)
  {
    if (action == "Open")  // open action
    {
      std::string      url;
      OpenFlags::Flags flags;
      Access::Mode     mode;
      uint16_t         timeout;
      std::tie(url, flags, mode, timeout) = GetOpenArgs();

      std::string lmetric;
      if ((flags & OpenFlags::Update) || (flags & OpenFlags::Write))
      {
        metric.ios["OpenW::n"]++;
      }
      else
      {
        metric.ios["OpenR::n"]++;
      }

      metric.ios["Open::n"]++;

      mytimer_t timer;

      if (!simulate)
        WaitFor(Open(file, url, flags, mode, timeout) >>
                [this, orgststr{ orgststr }, ending, closing, timer, &metric](XRootDStatus& s) mutable
                {
                  metric.addIos("Open", "e", HandleStatus(s, orgststr, "Open"));
                  metric.addDelays("Open", "tmeas", timer.elapsed());
                  ending.reset();
                  closing.reset();
                });
      else
      {
        ending.reset();
        closing.reset();
      }
    }
    else if (action == "Close")  // close action
    {
      uint16_t    timeout = GetCloseArgs();
      mytimer_t   timer;

      if (closing)
      {
        auto& sem = closing->get();
        closing.reset();
        sem.Wait();
      }

      metric.ios["Close::n"]++;

      if (!simulate)
        Async(Close(file, timeout) >>
              [this, orgststr{ orgststr }, ending, timer, &metric](XRootDStatus& s) mutable
              {
                metric.addIos("Close", "e", HandleStatus(s, orgststr, "Close"));
                metric.addDelays("Close", "tmeas", timer.elapsed());
                ending.reset();
              });
      else
      {
        ending.reset();
      }
    }
    else if (action == "Stat")  // stat action
    {
      bool     force;
      uint16_t timeout;
      std::tie(force, timeout) = GetStatArgs();
      metric.ios["Stat::n"]++;
      mytimer_t timer;

      if (!simulate)
        Async(Stat(file, force, timeout) >>
              [this, orgststr{ orgststr }, ending, closing, timer, &metric](XRootDStatus& s, StatInfo& r) mutable
              {
                metric.addIos("Stat", "e", HandleStatus(s, orgststr, "Stat"));
                metric.addDelays("Stat", "tmeas", timer.elapsed());
                ending.reset();
                closing.reset();
              });
      else
      {
        ending.reset();
        closing.reset();
      }
    }
    else if (action == "Read")  // read action
    {
      uint64_t offset;
      buffer_t buffer;
      uint16_t timeout;
      std::tie(offset, buffer, timeout) = GetReadArgs();
      metric.ios["Read::n"]++;
      metric.ios["Read::b"] += buffer->size();
      if ((offset + buffer->size()) > metric.ios["Read::o"])
        metric.ios["Read::o"] = offset + buffer->size();

      mytimer_t timer;
      if (!simulate)
        Async(Read(file, offset, buffer->size(), buffer->data(), timeout) >>
              [buffer, orgststr{ orgststr }, ending, closing, timer, &metric](XRootDStatus& s,
                                                                              ChunkInfo& r) mutable
              {
                metric.addIos("Read", "e", HandleStatus(s, orgststr, "Read"));
                metric.addDelays("Read", "tmeas", timer.elapsed());
                buffer.reset();
                ending.reset();
                closing.reset();
              });
      else
      {
        buffer.reset();
        ending.reset();
        closing.reset();
      }
    }
    else if (action == "PgRead")  // pgread action
    {
      uint64_t offset;
      buffer_t buffer;
      uint16_t timeout;
      std::tie(offset, buffer, timeout) = GetPgReadArgs();
      metric.ios["PgRead::n"]++;
      metric.ios["PgRead::b"] += buffer->size();
      if ((offset + buffer->size()) > metric.ios["Read::o"])
        metric.ios["Read::o"] = offset + buffer->size();
      mytimer_t timer;
      if (!simulate)
        Async(PgRead(file, offset, buffer->size(), buffer->data(), timeout) >>
              [buffer, orgststr{ orgststr }, ending, closing, timer, &metric](XRootDStatus& s,
                                                                              PageInfo& r) mutable
              {
                metric.addIos("PgRead", "e", HandleStatus(s, orgststr, "PgRead"));
                metric.addDelays("PgRead", "tmeas", timer.elapsed());
                buffer.reset();
                ending.reset();
                closing.reset();
              });
      else
      {
        buffer.reset();
        ending.reset();
        closing.reset();
      }
    }
    else if (action == "Write")  // write action
    {
      uint64_t offset;
      buffer_t buffer;
      uint16_t timeout;
      std::tie(offset, buffer, timeout) = GetWriteArgs();
      metric.ios["Write::n"]++;
      metric.ios["Write::b"] += buffer->size();
      if ((offset + buffer->size()) > metric.ios["Write::o"])
        metric.ios["Write::o"] = offset + buffer->size();
      mytimer_t timer;

      if (!simulate)
        Async(
          Write(file, offset, buffer->size(), buffer->data(), timeout) >>
          [buffer, orgststr{ orgststr }, ending, closing, timer, &metric](XRootDStatus& s) mutable
          {
            metric.addIos("Write", "e", HandleStatus(s, orgststr, "Write"));
            metric.addDelays("Write", "tmeas", timer.elapsed());
            buffer.reset();
            ending.reset();
            closing.reset();
          });
      else
      {
        buffer.reset();
        ending.reset();
        closing.reset();
      }
    }
    else if (action == "PgWrite")  // pgwrite action
    {
      uint64_t offset;
      buffer_t buffer;
      uint16_t timeout;
      std::tie(offset, buffer, timeout) = GetPgWriteArgs();
      metric.ios["PgWrite::n"]++;
      metric.ios["PgWrite::b"] += buffer->size();
      if ((offset + buffer->size()) > metric.ios["Write::o"])
        metric.ios["Write::o"] = offset + buffer->size();
      mytimer_t timer;
      if (!simulate)
        Async(
          PgWrite(file, offset, buffer->size(), buffer->data(), timeout) >>
          [buffer, orgststr{ orgststr }, ending, closing, timer, &metric](XRootDStatus& s) mutable
          {
            metric.addIos("PgWrite", "e", HandleStatus(s, orgststr, "PgWrite"));
            metric.addDelays("PgWrite", "tmeas", timer.elapsed());
            buffer.reset();
            ending.reset();
            closing.reset();
          });
      else
      {
        buffer.reset();
        ending.reset();
        closing.reset();
      }
    }
    else if (action == "Sync")  // sync action
    {
      uint16_t    timeout = GetSyncArgs();
      metric.ios["Sync::n"]++;
      mytimer_t timer;
      if (!simulate)
        Async(Sync(file, timeout) >>
              [this, orgststr{ orgststr }, ending, closing, timer, &metric](XRootDStatus& s) mutable
              {
                metric.addIos("Sync", "e", HandleStatus(s, orgststr, "Sync"));
                metric.addDelays("Sync", "tmeas", timer.elapsed());
                ending.reset();
                closing.reset();
              });
      else
      {
        ending.reset();
        closing.reset();
      }
    }
    else if (action == "Truncate")  // truncate action
    {
      uint64_t size;
      uint16_t timeout;
      std::tie(size, timeout) = GetTruncateArgs();
      metric.ios["Truncate::n"]++;
      if (size > metric.ios["Truncate::o"])
        metric.ios["Truncate::o"] = size;

      mytimer_t timer;
      if (!simulate)
        Async(Truncate(file, size, timeout) >>
              [this, orgststr{ orgststr }, ending, closing, timer, &metric](XRootDStatus& s) mutable
              {
                metric.addIos("Truncate", "e", HandleStatus(s, orgststr, "Truncate"));
                metric.addDelays("Truncate", "tmeas", timer.elapsed());
                ending.reset();
                closing.reset();
              });
      else
      {
        ending.reset();
        closing.reset();
      }
    }
    else if (action == "VectorRead")  // vector read action
    {
      ChunkList chunks;
      uint16_t  timeout;
      std::vector<buffer_t> buffers;
      std::tie(chunks, timeout, buffers) = GetVectorReadArgs();
      metric.ios["VectorRead::n"]++;
      for (auto& ch : chunks)
      {
        metric.ios["VectorRead::b"] += ch.GetLength();
        if ((ch.GetOffset() + ch.GetLength()) > metric.ios["Read::o"])
          metric.ios["Read::o"] = ch.GetOffset() + ch.GetLength();
      }

      mytimer_t timer;
      if (!simulate)
        Async(
          VectorRead(file, chunks, timeout) >>
          [this, orgststr{ orgststr }, buffers, ending, closing, timer, &metric](XRootDStatus& s, VectorReadInfo& r) mutable
          {
            metric.addIos("VectorRead", "e", HandleStatus(s, orgststr, "VectorRead"));
            metric.addDelays("VectorRead", "tmeas", timer.elapsed());
            buffers.clear();
            ending.reset();
            closing.reset();
          });
      else
      {
        buffers.clear();
        ending.reset();
        closing.reset();
      }
    }
    else if (action == "VectorWrite")  // vector write
    {
      ChunkList chunks;
      uint16_t  timeout;
      std::vector<buffer_t> buffers;
      std::tie(chunks, timeout, buffers) = GetVectorWriteArgs();
      metric.ios["VectorWrite::n"]++;
      for (auto& ch : chunks)
      {
        metric.ios["VectorWrite::b"] += ch.GetLength();
        if ((ch.GetOffset() + ch.GetLength()) > metric.ios["Write::o"])
          metric.ios["Write::o"] = ch.GetOffset() + ch.GetLength();
      }
      mytimer_t timer;
      if (!simulate)
        Async(VectorWrite(file, chunks, timeout) >>
              [this, orgststr{ orgststr }, buffers, ending, closing, timer, &metric](XRootDStatus& s) mutable
              {
                metric.addIos("VectorWrite", "e", HandleStatus(s, orgststr, "VectorWrite"));
                metric.addDelays("VectorWrite", "tmeas", timer.elapsed());
                buffers.clear();
                ending.reset();
                closing.reset();
              });
      else
      {
        buffers.clear();
        ending.reset();
        closing.reset();
      }
    }
    else
    {
      DefaultEnv::GetLog()->Warning(AppMsg, "Cannot replyt %s action.", action.c_str());
    }
  }

  //--------------------------------------------------------------------------
  //! Get nominal duration variable
  //--------------------------------------------------------------------------
  double NominalDuration() const { return nominalduration; }

  //--------------------------------------------------------------------------
  //! Get aciton name
  //--------------------------------------------------------------------------
  std::string Name() const { return action; }

  private:
  //--------------------------------------------------------------------------
  //! Handle response status
  //--------------------------------------------------------------------------
  static bool HandleStatus(XRootDStatus& response, const std::string& orgstr, const std::string where="unknown")
  {
    std::string rspstr = response.ToString();
    rspstr.erase(remove(rspstr.begin(), rspstr.end(), ' '), rspstr.end());

    if (rspstr != orgstr)
    {
      DefaultEnv::GetLog()->Warning(AppMsg,
                                    "We were expecting status: %s, but "
                                    "received: %s from: %s",
                                    orgstr.c_str(),
                                    rspstr.c_str(),
				    where.c_str());
      return true;
    }
    else
    {
      return false;
    }
  }

  //--------------------------------------------------------------------------
  //! Parse arguments for open
  //--------------------------------------------------------------------------
  std::tuple<std::string, OpenFlags::Flags, Access::Mode, uint16_t> GetOpenArgs()
  {
    std::vector<std::string> tokens;
    Utils::splitString(tokens, args, ";");
    if (tokens.size() != 4)
      throw std::invalid_argument("Failed to parse open arguments.");
    std::string      url     = tokens[0];
    OpenFlags::Flags flags   = static_cast<OpenFlags::Flags>(std::stoul(tokens[1]));
    Access::Mode     mode    = static_cast<Access::Mode>(std::stoul(tokens[2]));
    uint16_t         timeout = static_cast<uint16_t>(std::stoul(tokens[3]));
    return std::make_tuple(url, flags, mode, timeout);
  }

  //--------------------------------------------------------------------------
  //! Parse arguments for close
  //--------------------------------------------------------------------------
  uint16_t GetCloseArgs() { return static_cast<uint16_t>(std::stoul(args)); }

  std::tuple<bool, uint16_t> GetStatArgs()
  {
    std::vector<std::string> tokens;
    Utils::splitString(tokens, args, ";");
    if (tokens.size() != 2)
      throw std::invalid_argument("Failed to parse stat arguments.");
    bool     force   = (tokens[0] == "true");
    uint16_t timeout = static_cast<uint16_t>(std::stoul(tokens[1]));
    return std::make_tuple(force, timeout);
  }

  //--------------------------------------------------------------------------
  //! Parse arguments for read
  //--------------------------------------------------------------------------
  std::tuple<uint64_t, buffer_t, uint16_t> GetReadArgs()
  {
    std::vector<std::string> tokens;
    Utils::splitString(tokens, args, ";");
    if (tokens.size() != 3)
      throw std::invalid_argument("Failed to parse read arguments.");
    uint64_t offset  = std::stoull(tokens[0]);
    uint32_t length  = std::stoul(tokens[1]);
    auto     buffer  = BufferPool::Instance().Allocate( length );
    uint16_t timeout = static_cast<uint16_t>(std::stoul(tokens[2]));
    return std::make_tuple(offset, buffer, timeout);
  }

  //--------------------------------------------------------------------------
  //! Parse arguments for pgread
  //--------------------------------------------------------------------------
  inline std::tuple<uint64_t, buffer_t, uint16_t> GetPgReadArgs() { return GetReadArgs(); }

  //--------------------------------------------------------------------------
  //! Parse arguments for write
  //--------------------------------------------------------------------------
  inline std::tuple<uint64_t, buffer_t, uint16_t> GetWriteArgs() { return GetReadArgs(); }

  //--------------------------------------------------------------------------
  //! Parse arguments for pgwrite
  //--------------------------------------------------------------------------
  inline std::tuple<uint64_t, buffer_t, uint16_t> GetPgWriteArgs() { return GetReadArgs(); }

  //--------------------------------------------------------------------------
  //! Parse arguments for sync
  //--------------------------------------------------------------------------
  uint16_t GetSyncArgs() { return static_cast<uint16_t>(std::stoul(args)); }

  //--------------------------------------------------------------------------
  //! Parse arguments for truncate
  //--------------------------------------------------------------------------
  std::tuple<uint64_t, uint16_t> GetTruncateArgs()
  {
    std::vector<std::string> tokens;
    Utils::splitString(tokens, args, ";");
    if (tokens.size() != 2)
      throw std::invalid_argument("Failed to parse truncate arguments.");
    uint64_t size    = std::stoull(tokens[0]);
    uint16_t timeout = static_cast<uint16_t>(std::stoul(tokens[1]));
    return std::make_tuple(size, timeout);
  }

  //--------------------------------------------------------------------------
  //! Parse arguments for vector read
  //--------------------------------------------------------------------------
  std::tuple<ChunkList, uint16_t, std::vector<buffer_t>> GetVectorReadArgs()
  {
    std::vector<std::string> tokens;
    Utils::splitString(tokens, args, ";");
    ChunkList chunks;
    chunks.reserve( tokens.size() - 1 );
    std::vector<buffer_t> buffers;
    buffers.reserve( tokens.size() - 1 );
    for (size_t i = 0; i < tokens.size() - 1; i += 2)
    {
      uint64_t offset = std::stoull(tokens[i]);
      uint32_t length = std::stoul(tokens[i + 1]);
      auto     buffer  = BufferPool::Instance().Allocate( length );
      chunks.emplace_back(offset, length, buffer->data());
      buffers.emplace_back( std::move( buffer ) );
    }
    uint16_t timeout = static_cast<uint16_t>(std::stoul(tokens.back()));
    return std::make_tuple(std::move(chunks), timeout, std::move(buffers));
  }

  //--------------------------------------------------------------------------
  //! Parse arguments for vector write
  //--------------------------------------------------------------------------
  inline std::tuple<ChunkList, uint16_t, std::vector<buffer_t>> GetVectorWriteArgs() { return GetVectorReadArgs(); }

  File&             file;             //< the file object
  const std::string action;           //< the action to be executed
  const std::string args;             //< arguments for the operation
  std::string       orgststr;         //< the original response status of the action
  double            nominalduration;  //< the original duration of execution
};

//------------------------------------------------------------------------------
//! Split a row into columns
//------------------------------------------------------------------------------
std::vector<std::string> ToColumns( const std::string &row )
{
  std::vector<std::string> columns;
  size_t quotecnt = 0;
  size_t pos = 0;
  //----------------------------------------------------------------------------
  //! loop over all the columns in the row
  //----------------------------------------------------------------------------
  while( pos != std::string::npos && pos < row.size() )
  {
    if( row[pos] == '"' ) // we are handling a quoted column
    {
      if( quotecnt > 0 ) // this is a closing quote
      {
        if( pos + 1 < row.size() && row[pos + 1] != ',' ) // if it is not the last character in the row it should be followed by a comma
          throw std::runtime_error( "Parsing error: missing comma" );
        --quotecnt; // strip the quote
        ++pos; // move to the comma or end of row
        continue;
      }
      else // this is a opening quote
      {
        ++quotecnt;
        auto b = std::next( row.begin(), pos + 1 ); // iterator to the beginning of our column
        size_t posend = row.find( "\",", pos + 1 ); // position of the cursor to the end of our column
        if( posend == std::string::npos && row[row.size() - 1] == '"' )
          posend = row.size() - 1;
        else if( posend == std::string::npos )
          throw std::runtime_error( "Parsing error: missing closing quote" );
        auto e = std::next( row.begin(), posend ); // iterator to the end of our column
        columns.emplace_back( b, e ); // add the column to the result
        pos = posend; // move to the next column
        continue;
      }
    }
    else if( row[pos] == ',' ) // we are handling a column separator
    {
      if( pos + 1 < row.size() && row[pos + 1] == '"' ) // check if the column is quoted
      {
        ++pos; // if yes we will handle this with the logic reserved for quoted columns
        continue;
      }
      auto b = std::next( row.begin(), pos + 1 ); // iterator to the beginning of our column
      size_t posend = row.find( ',', pos + 1 ); // position of the cursor to the end of our column
      if( posend == std::string::npos )
        posend = row.size();
      auto e = std::next( row.begin(), posend ); // iterator to the end of our column
      columns.emplace_back( b, e ); // add the column to the result
      pos = posend; // move to the next column
      continue;
    }
    else if( pos == 0 ) // we are handling the 1st column if not quoted
    {
      size_t posend = row.find( ',', pos + 1 ); // position of the cursor to the end of our column
      if( posend == std::string::npos )
        posend = row.size();
      auto end = std::next( row.begin(), posend ); // iterator to the end of our column
      columns.emplace_back( row.begin(), end ); // add the column to the result
      pos = posend;  // move to the next column
      continue;
    }
    else
    {
      throw std::runtime_error( "Parsing error: invalid input file." );
    }
  }
  return columns;
}

//------------------------------------------------------------------------------
//! List of actions: start time - action
//------------------------------------------------------------------------------
using action_list = std::multimap<double, ActionExecutor>;

//------------------------------------------------------------------------------
//! Parse input file
//! @param path : path to the input csv file
//------------------------------------------------------------------------------
std::unordered_map<File*, action_list> ParseInput(const std::string&                      path,
                                                  double&                                 t0,
                                                  double&                                 t1,
                                                  std::unordered_map<File*, std::string>& filenames,
                                                  std::unordered_map<File*, double>& synchronicity,
                                                  std::unordered_map<File*, size_t>& responseerrors,
                                                  const std::vector<std::string>&    option_regex)
{
  std::unordered_map<File*, action_list> result;
  std::unique_ptr<std::ifstream>        fin( path.empty() ? nullptr : new std::ifstream( path, std::ifstream::in ) );
  std::istream                          &input = path.empty() ? std::cin : *fin;
  std::string                            line;
  std::unordered_map<uint64_t, File*>    files;
  std::unordered_map<uint64_t, double>   last_stop;
  std::unordered_map<uint64_t, double>   overlaps;
  std::unordered_map<uint64_t, double>   overlaps_cnt;

  t0 = 10e99;
  t1 = 0;
  while (input.good())
  {
    std::getline(input, line);
    if (line.empty())
      continue;
    std::vector<std::string> tokens = ToColumns( line );
    if (tokens.size() == 6)
      tokens.emplace_back();
    if (tokens.size() != 7)
    {
      throw std::invalid_argument("Invalid input file format.");
    }

    uint64_t    id     = std::stoull(tokens[0]);  // file object ID
    std::string action = tokens[1];               // action name (e.g. Open)
    double      start  = std::stod(tokens[2]);    // start time
    std::string args   = tokens[3];               // operation arguments
    double      stop   = std::stod(tokens[4]);    // stop time
    std::string status = tokens[5];               // operation status
    std::string resp   = tokens[6];               // server response

    if (option_regex.size())
    {
      for (auto& v : option_regex)
      {
        std::vector<std::string> tokens;
        Utils::splitString(tokens, v, ":=");
        std::regex src(tokens[0]);
        if (tokens.size() != 2)
        {
          std::cerr
            << "Error: invalid regex for argument replacement - must be format like <oldstring>:=<newstring>"
            << std::endl;
          exit(EINVAL);
        }
        else
        {
          // write the results to an output iterator
          args = std::regex_replace(args, src, tokens[1]);
        }
      }
    }

    if (start < t0)
      t0 = start;
    if (stop > t1)
      t1 = stop;

    if (!files.count(id))
    {
      files[id] = new File(false);
      files[id]->SetProperty("BundledClose", "true");
      filenames[files[id]] = args;
      filenames[files[id]].erase(args.find(";"));
      overlaps[id]     = 0;
      overlaps_cnt[id] = 0;
      last_stop[id]    = stop;
    }
    else
    {
      overlaps_cnt[id]++;
      if (start > last_stop[id])
      {
        overlaps[id]++;
      }
      last_stop[id] = stop;
    }

    last_stop[id]           = stop;
    double nominal_duration = stop - start;

    if (status != "[SUCCESS]")
    {
      responseerrors[files[id]]++;
    }
    else
    {
      result[files[id]].emplace(
        start, ActionExecutor(*files[id], action, args, status, resp, nominal_duration));
    }
  }

  for (auto& it : overlaps)
  {
    // compute the synchronicity of requests
    synchronicity[files[it.first]] = 100.0 * (it.second / overlaps_cnt[it.first]);
  }
  return result;
}

//------------------------------------------------------------------------------
//! Execute list of actions against given file
//! @param file    : the file object
//! @param actions : list of actions to be executed
//! @param t0      : offset to add to each start time to determine when to ru an action
//! @return        : thread that will executed the list of actions
//------------------------------------------------------------------------------
std::thread ExecuteActions(std::unique_ptr<File> file,
                           action_list&&         actions,
                           double                t0,
                           double                speed,
                           ActionMetrics&        metric,
                           bool                  simulate)
{
  std::thread t(
    [file{ std::move(file) },
     actions{ std::move(actions) },
     t0,
     &metric,
     simulate,
     &speed]() mutable
    {
      XrdSysSemaphore endsem(0);
      XrdSysSemaphore closesem(0);
      auto            ending  = std::make_shared<barrier_t>(endsem);
      auto            closing = std::make_shared<barrier_t>(closesem);

      for (auto& p : actions)
      {
        auto& action = p.second;

        auto tdelay = t0 ? ((p.first + t0) - XrdCl::Action::timeNow()) : 0;
        if (tdelay > 0)
        {
          tdelay /= speed;
          metric.delays[action.Name() + "::tloss"] += tdelay;
	  std::this_thread::sleep_for(std::chrono::milliseconds((int) (tdelay * 1000)));
        }
        else
        {
          metric.delays[action.Name() + "::tgain"] += tdelay;
        }

        mytimer_t timer;
        action.Execute(ending, closing, metric, simulate);
        metric.addDelays(action.Name(), "tnomi", action.NominalDuration());
        metric.addDelays(action.Name(), "texec", timer.elapsed());
      }
      ending.reset();
      closing.reset();
      endsem.Wait();
      file->GetProperty("LastURL", metric.url);
      file.reset();
    });
  return t;
}

}

void usage()
{
  std::cerr
    << "usage: xrdreplay [-p|--print] [-c|--create-data] [t|--truncate-data] [-l|--long] [-s|--summary] [-h|--help] [-r|--replace <arg>:=<newarg>] [-f|--suppress] <recordfilename>\n"
    << std::endl;
  std::cerr << "                -h | --help             : show this help" << std::endl;
  std::cerr
    << "                -f | --suppress         : force to run all IO with all successful result status - suppress all others"
    << std::endl;
  std::cerr
    << "                                          - by default the player won't run with an unsuccessfully recorded IO"
    << std::endl;
  std::cerr << std::endl;
  std::cerr
    << "                -p | --print            : print only mode - shows all the IO for the given replay file without actually running any IO"
    << std::endl;
  std::cerr
    << "                -s | --summary          : print summary - shows all the aggregated IO counter summed for all files"
    << std::endl;
  std::cerr
    << "                -l | --long             : print long - show all file IO counter for each individual file"
    << std::endl;
  std::cerr
    << "                -r | --replace <a>:=<b> : replace in the argument list the string <a> with <b> "
    << std::endl;
  std::cerr
    << "                                          - option is usable several times e.g. to change storage prefixes or filenames"
    << std::endl;
  std::cerr << std::endl;
  std::cerr
    << "example:        ...  --replace file:://localhost:=root://xrootd.eu/        : redirect local file to remote"
    << std::endl;
  std::cerr << std::endl;
  exit(-1);
}

int main(int argc, char** argv)
{
  XrdCl::ReplayArgs opt(argc, argv);
  int               rc = 0;

  try
  {
    double                                                 t0 = 0;
    double                                                 t1 = 0;
    std::unordered_map<XrdCl::File*, std::string>          filenames;
    std::unordered_map<XrdCl::File*, double>               synchronicity;
    std::unordered_map<XrdCl::File*, size_t>               responseerrors;
    auto                                                   actions = XrdCl::ParseInput(opt.path(),
                                     t0,
                                     t1,
                                     filenames,
                                     synchronicity,
                                     responseerrors,
                                     opt.regex());  // parse the input file
    std::vector<std::thread>                               threads;
    std::unordered_map<XrdCl::File*, XrdCl::ActionMetrics> metrics;
    threads.reserve(actions.size());
    double               toffset = XrdCl::Action::timeNow() - t0;
    XrdCl::mytimer_t     timer;
    XrdCl::ActionMetrics summetric;
    bool                 sampling_error = false;

    for (auto& action : actions)
    {
      metrics[action.first].fname         = filenames[action.first];
      metrics[action.first].synchronicity = synchronicity[action.first];
      metrics[action.first].errors        = responseerrors[action.first];
      if (metrics[action.first].errors)
      {
        sampling_error = true;
      }
    }

    if (sampling_error)
    {
      std::cerr << "Warning: IO file contains unsuccessful samples!" << std::endl;
      if (!opt.suppress_error())
      {
        std::cerr << "... run with [-f] or [--suppress] option to suppress unsuccessful IO events!"
                  << std::endl;
        exit(-1);
      }
    }


    if (opt.print())
      toffset = 0;  // indicate not to follow timing

    for (auto& action : actions)
    {
      // execute list of actions against file object
      threads.emplace_back(ExecuteActions(std::unique_ptr<XrdCl::File>(action.first),
                                          std::move(action.second),
                                          toffset,
                                          opt.speed(),
                                          metrics[action.first],
                                          opt.print()));
    }

    for (auto& t : threads)  // wait until we are done
      t.join();

    if (opt.json())
    {
      std::cout << "{" << std::endl;
      if (opt.longformat())
        std::cout << "  \"metrics\": [" << std::endl;
    }

    for (auto& metric : metrics)
    {
      if (opt.longformat())
      {
        std::cout << metric.second.Dump(opt.json());
      }
      summetric.add(metric.second);
    }

    if (opt.summary())
      std::cout << summetric.Dump(opt.json());

    if (opt.json())
    {
      if (opt.longformat())
        std::cout << "  ]," << std::endl;
    }

    double tbench = timer.elapsed();

    if (opt.json())
    {
      {
        std::cout << "  \"iosummary\": { " << std::endl;
        if (!opt.print())
        {
          std::cout << "    \"player::runtime\": " << tbench << "," << std::endl;
        }
        std::cout << "    \"player::speed\": " << opt.speed() << "," << std::endl;
        std::cout << "    \"sampled::runtime\": " << t1 - t0 << "," << std::endl;
        std::cout << "    \"volume::totalread\": " << summetric.getBytesRead() << "," << std::endl;
        std::cout << "    \"volume::totalwrite\": " << summetric.getBytesWritten() << ","
                  << std::endl;
        std::cout << "    \"volume::read\": " << summetric.ios["Read::b"] << "," << std::endl;
        std::cout << "    \"volume::write\": " << summetric.ios["Write::b"] << "," << std::endl;
        std::cout << "    \"volume::pgread\": " << summetric.ios["PgRead::b"] << "," << std::endl;
        std::cout << "    \"volume::pgwrite\": " << summetric.ios["PgWrite::b"] << "," << std::endl;
        std::cout << "    \"volume::vectorread\": " << summetric.ios["VectorRead::b"] << ","
                  << std::endl;
        std::cout << "    \"volume::vectorwrite\": " << summetric.ios["VectorWrite::b"] << ","
                  << std::endl;
        std::cout << "    \"iops::read\": " << summetric.ios["Read::n"] << "," << std::endl;
        std::cout << "    \"iops::write\": " << summetric.ios["Write::n"] << "," << std::endl;
        std::cout << "    \"iops::pgread\": " << summetric.ios["PgRead::n"] << "," << std::endl;
        std::cout << "    \"iops::pgwrite\": " << summetric.ios["PgRead::n"] << "," << std::endl;
        std::cout << "    \"iops::vectorread\": " << summetric.ios["VectorRead::n"] << ","
                  << std::endl;
        std::cout << "    \"iops::vectorwrite\": " << summetric.ios["VectorRead::n"] << ","
                  << std::endl;
        std::cout << "    \"files::read\": " << summetric.ios["OpenR::n"] << "," << std::endl;
        std::cout << "    \"files::write\": " << summetric.ios["OpenW::n"] << "," << std::endl;
	std::cout << "    \"datasetsize::read\": " << summetric.ios["Read::o"] << "," << std::endl;
	std::cout << "    \"datasetsize::write\": " << summetric.ios["Write::o"] << "," << std::endl;
        if (!opt.print())
        {
          std::cout << "    \"bandwidth::mb::read\": "
                    << summetric.getBytesRead() / tbench / 1000000.0 << "," << std::endl;
          std::cout << "    \"bandwdith::mb::write\": "
                    << summetric.getBytesWritten() / tbench / 1000000.0 << "," << std::endl;
          std::cout << "    \"performancemark\": " << (100.0 * (t1 - t0) / tbench) << ","
                    << std::endl;
          std::cout << "    \"gain::read\":"
                    << (100.0 * summetric.delays["Read::tnomi"] / summetric.delays["Read::tmeas"])
                    << "," << std::endl;
          std::cout << "    \"gain::write\":"
                    << (100.0 * summetric.delays["Write::tnomi"] / summetric.delays["Write::tmeas"])
                    << std::endl;
        }
        std::cout << "    \"synchronicity::read\":"
                  << summetric.aggregated_synchronicity.ReadSynchronicity() << "," << std::endl;
        std::cout << "    \"synchronicity::write\":"
                  << summetric.aggregated_synchronicity.WriteSynchronicity() << "," << std::endl;
        std::cout << "    \"response::error:\":" << summetric.ios["All::e"] << std::endl;
        std::cout << "  }" << std::endl;
        std::cout << "}" << std::endl;
      }
    }
    else
    {
      std::cout << "# =============================================" << std::endl;
      if (!opt.print())
        std::cout << "# IO Summary" << std::endl;
      else
        std::cout << "# IO Summary (print mode)" << std::endl;
      std::cout << "# =============================================" << std::endl;
      if (!opt.print())
      {
        std::cout << "# Total   Runtime  : " << std::fixed << tbench << " s" << std::endl;
      }
      std::cout << "# Sampled Runtime  : " << std::fixed << t1 - t0 << " s" << std::endl;
      std::cout << "# Playback Speed   : " << std::fixed << std::setprecision(2) << opt.speed()
                << std::endl;
      std::cout << "# IO Volume (R)    : " << std::fixed
                << XrdCl::ActionMetrics::humanreadable(summetric.getBytesRead())
                << " [ std:" << XrdCl::ActionMetrics::humanreadable(summetric.ios["Read::b"])
                << " vec:" << XrdCl::ActionMetrics::humanreadable(summetric.ios["VectorRead::b"])
                << " page:" << XrdCl::ActionMetrics::humanreadable(summetric.ios["PgRead::b"])
                << " ] " << std::endl;
      std::cout << "# IO Volume (W)    : " << std::fixed
                << XrdCl::ActionMetrics::humanreadable(summetric.getBytesWritten())
                << " [ std:" << XrdCl::ActionMetrics::humanreadable(summetric.ios["Write::b"])
                << " vec:" << XrdCl::ActionMetrics::humanreadable(summetric.ios["VectorWrite::b"])
                << " page:" << XrdCl::ActionMetrics::humanreadable(summetric.ios["PgWrite::b"])
                << " ] " << std::endl;
      std::cout << "# IOPS      (R)    : " << std::fixed << summetric.getIopsRead()
                << " [ std:" << summetric.ios["Read::n"]
                << " vec:" << summetric.ios["VectorRead::n"]
                << " page:" << summetric.ios["PgRead::n"] << " ] " << std::endl;
      std::cout << "# IOPS      (W)    : " << std::fixed << summetric.getIopsWrite()
                << " [ std:" << summetric.ios["Write::n"]
                << " vec:" << summetric.ios["VectorWrite::n"]
                << " page:" << summetric.ios["PgWrite::n"] << " ] " << std::endl;
      std::cout << "# Files     (R)    : " << std::fixed << summetric.ios["OpenR::n"] << std::endl;
      std::cout << "# Files     (W)    : " << std::fixed << summetric.ios["OpenW::n"] << std::endl;
      std::cout << "# Datasize  (R)    : " << std::fixed
                << XrdCl::ActionMetrics::humanreadable(summetric.ios["Read::o"]) << std::endl;
      std::cout << "# Datasize  (W)    : " << std::fixed
                << XrdCl::ActionMetrics::humanreadable(summetric.ios["Write::o"]) << std::endl;
      if (!opt.print())
      {
        std::cout << "# IO BW     (R)    : " << std::fixed << std::setprecision(2)
                  << summetric.getBytesRead() / tbench / 1000000.0 << " MB/s" << std::endl;
        std::cout << "# IO BW     (W)    : " << std::fixed << std::setprecision(2)
                  << summetric.getBytesRead() / tbench / 1000000.0 << " MB/s" << std::endl;
      }
      std::cout << "# ---------------------------------------------" << std::endl;
      std::cout << "# Quality Estimation" << std::endl;
      std::cout << "# ---------------------------------------------" << std::endl;
      if (!opt.print())
      {
        std::cout << "# Performance Mark : " << std::fixed << std::setprecision(2)
                  << (100.0 * (t1 - t0) / tbench) << "%" << std::endl;
        std::cout << "# Gain Mark(R)     : " << std::fixed << std::setprecision(2)
                  << (100.0 * summetric.delays["Read::tnomi"] / summetric.delays["Read::tmeas"])
                  << "%" << std::endl;
        std::cout << "# Gain Mark(W)     : " << std::fixed << std::setprecision(2)
                  << (100.0 * summetric.delays["Write::tnomi"] / summetric.delays["Write::tmeas"])
                  << "%" << std::endl;
      }
      std::cout << "# Synchronicity(R) : " << std::fixed << std::setprecision(2)
                << summetric.aggregated_synchronicity.ReadSynchronicity() << "%" << std::endl;
      std::cout << "# Synchronicity(W) : " << std::fixed << std::setprecision(2)
                << summetric.aggregated_synchronicity.WriteSynchronicity() << "%" << std::endl;
      if (!opt.print())
      {
        std::cout << "# ---------------------------------------------" << std::endl;
        std::cout << "# Response Errors  : " << std::fixed << summetric.ios["All::e"] << std::endl;
        std::cout << "# =============================================" << std::endl;
        if (summetric.ios["All::e"])
        {
          std::cerr << "Error: replay job failed with IO errors!" << std::endl;
          rc = -5;
        }
      }
      if (opt.create() || opt.verify())
      {
        std::cout << "# ---------------------------------------------" << std::endl;
        if (opt.create())
        {
          std::cout << "# Creating Dataset ..." << std::endl;
        }
        else
        {
          std::cout << "# Verifying Dataset ..." << std::endl;
        }
        uint64_t created_sofar = 0;
        for (auto& metric : metrics)
        {
          if (metric.second.getBytesRead() && !metric.second.getBytesWritten())
          {
            std::cout << "# ............................................." << std::endl;
            std::cout << "# file: " << metric.second.fname << std::endl;
            std::cout << "# size: "
                      << XrdCl::ActionMetrics::humanreadable(metric.second.ios["Read::o"]) << " [ "
                      << XrdCl::ActionMetrics::humanreadable(created_sofar) << " out of "
                      << XrdCl::ActionMetrics::humanreadable(summetric.ios["Read::o"]) << " ] "
                      << std::setprecision(2) << " ( "
                      << 100.0 * created_sofar / summetric.ios["Read::o"] << "% )" << std::endl;
            if (!XrdCl::AssureFile(
                  metric.second.fname, metric.second.ios["Read::o"], opt.truncate(), opt.verify()))
            {
              if (opt.verify())
              {
                rc = -5;
              }
              else
              {
                std::cerr << "Error: failed to assure that file " << metric.second.fname
                          << " is stored with a size of "
                          << XrdCl::ActionMetrics::humanreadable(metric.second.ios["Read::o"])
                          << " !!!";
                rc = -5;
              }
            }
          }
        }
      }
    }
  }
  catch (const std::invalid_argument& ex)
  {
    std::cout << ex.what() << std::endl;  // print parsing errors
    return 1;
  }

  return rc;
}
