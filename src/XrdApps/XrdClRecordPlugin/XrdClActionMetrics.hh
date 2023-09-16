//------------------------------------------------------------------------------
// Copyright (c) 2011-2021 by European Organization for Nuclear Research (CERN)
// Author: Andreas-Joachim Peters <andreas.joachim.peters@cern.ch>
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

#include "XrdClAction.hh"
#include <fstream>
#include <vector>
#include <tuple>
#include <unordered_map>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <atomic>
#include <stdarg.h>
#include <getopt.h>
#include <map>
#include <vector>
#include <numeric>
#include <regex>

namespace XrdCl
{
//------------------------------------------------------------------------------
//! Metrics struct storing all timing and IO information of an action
//------------------------------------------------------------------------------
struct ActionMetrics
{
  ActionMetrics()
  {
    std::string op[] = { "OpenR",  "OpenW",   "Open",     "Read", "Write",      "Stat",       "Close",
                         "PgRead", "PgWrite", "Truncate", "Sync", "VectorRead", "VectorWrite" };
    for (auto& i : op)
    {
      std::string gain = i + "::tgain";  // time early for IO submission
      std::string loss = i + "::tloss";  // time late for IO submission
      std::string nomi = i + "::tnomi";  // nominal IO time
      std::string exec = i + "::texec";  // time to submit IO
      std::string meas = i + "::tmeas";  // time measured for IO to complete

      delays[gain] = 0;
      delays[loss] = 0;
      delays[nomi] = 0;
      delays[exec] = 0;
      delays[meas] = 0;

      std::string cnt = i + "::n";  // IOPS
      std::string vol = i + "::b";  // number of bytes
      std::string err = i + "::e";  // number of unsuccessful IOs
      std::string off = i + "::o";  // maximum offset seen

      ios[cnt] = 0;
      ios[vol] = 0;
      ios[err] = 0;
      ios[off] = 0;
    }

    ios["All::e"] = 0;  // Error counter for summing over files
    synchronicity = 0.0;
    errors        = 0;
  }

  std::string Dump(bool json) const
  {
    std::stringstream ss;
    if (!json)
    {
      ss << "# -----------------------------------------------------------------" << std::endl;
      if (fname != "")
      {
        ss << "# File: " << fname << std::endl;
        ss << "# Sync: " << std::fixed << std::setprecision(2) << synchronicity << "%" << std::endl;
        ss << "# Errs: " << std::fixed << errors << std::endl;
      }
      else
      {
        ss << "# Summary" << std::endl;
      }
      ss << "# -----------------------------------------------------------------" << std::endl;
      for (auto& i : delays)
      {
        std::string key = i.first;
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
        if (i.second)
        {
          ss << "# " << std::setw(16) << key << " : " << std::setw(16) << std::fixed << i.second
             << " s" << std::endl;
        }
      }
      for (auto& i : ios)
      {
        std::string key = i.first;
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
        if (i.second)
        {
          ss << "# " << std::setw(16) << key << " : " << std::setw(16) << i.second << std::endl;
        }
      }
    }
    else
    {
      std::string name = fname;
      if (fname.empty())
        name = "_files_summary_";

      ss << "    {" << std::endl;
      ss << "      \"name\":"
         << "\"" << name << "\"," << std::endl;
      ss << "      \"synchronicity\": " << synchronicity << "," << std::endl;
      ss << "      \"errors\": " << errors << "," << std::endl;
      for (auto& i : delays)
      {
        std::string key = i.first;
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
        if (i.second)
        {
          ss << "      \"" << key << "\": " << i.second << "," << std::endl;
        }
      }
      for (auto& i : ios)
      {
        std::string key = i.first;
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
        if (i.second)
        {
          ss << "      \"" << key << "\": " << i.second << "," << std::endl;
        }
      }
      ss.seekp(-2, std::ios_base::end);
      ss << "\n";
      if (fname.empty())
        ss << "    }" << std::endl;
      else
        ss << "    }," << std::endl;
    }
    return ss.str();
  }

  size_t getIopsRead() const
  {
    auto v1 = ios.find("Read::n");
    auto v2 = ios.find("PgRead::n");
    auto v3 = ios.find("VectorRead::n");
    return (v1->second + v2->second + v3->second);
  }

  size_t getIopsWrite() const
  {
    auto v1 = ios.find("Write::n");
    auto v2 = ios.find("PgWrite::n");
    auto v3 = ios.find("VectorWrite::n");
    return (v1->second + v2->second + v3->second);
  }

  size_t getBytesRead() const
  {
    auto v1 = ios.find("Read::b");
    auto v2 = ios.find("PgRead::b");
    auto v3 = ios.find("VectorRead::b");
    return (v1->second + v2->second + v3->second);
  }

  size_t getBytesWritten() const
  {
    auto v1 = ios.find("Write::b");
    auto v2 = ios.find("PgWrite::b");
    auto v3 = ios.find("VectorWrite::b");
    return (v1->second + v2->second + v3->second);
  }

  void addDelays(const std::string& action, const std::string& field, double value)
  {
    // function called from callbacks requires a guard
    std::unique_lock<std::mutex> guard(mtx);
    delays[action + "::" + field] += value;
  }

  void addIos(const std::string& action, const std::string& field, double value)
  {
    // function called from callbacks requires a guard
    std::unique_lock<std::mutex> guard(mtx);
    ios[action + "::" + field] += value;
    if (field == "e")
    {
      ios["All::e"] += value;
    }
  }

  void add(const ActionMetrics& other)
  {
    for (auto& k : other.ios)
    {
      ios[k.first] += k.second;
    }
    for (auto& k : other.delays)
    {
      delays[k.first] += k.second;
    }
    errors += other.errors;

    auto w1 = other.ios.find("Write::b");
    auto w2 = other.ios.find("PgWrite::b");
    auto w3 = other.ios.find("VectorWrite::b");

    if (((w1 != other.ios.end()) && w1->second) || ((w2 != other.ios.end()) && w2->second)
        || ((w3 != other.ios.end()) && w3->second))
    {
      // count as EGRES
      aggregated_synchronicity.writes.push_back(other.synchronicity);
    }
    else
    {
      // count es INGRES
      aggregated_synchronicity.reads.push_back(other.synchronicity);
    }
  }

  static std::string humanreadable(uint64_t insize)
  {
    const uint64_t    KB = 1000ll;
    const uint64_t    MB = 1000ll * KB;
    const uint64_t    GB = 1000ll * MB;
    const uint64_t    TB = 1000ll * GB;
    const uint64_t    PB = 1000ll * TB;
    const uint64_t    EB = 1000ll * PB;
    std::stringstream ss;
    if (insize >= (10 * KB))
    {
      if (insize >= MB)
      {
        if (insize >= GB)
        {
          if (insize >= TB)
          {
            if (insize >= PB)
            {
              if (insize >= EB)
              {
                // EB
                ss << std::fixed << std::setprecision(2) << (insize * 1.0 / EB) << " EB";
              }
              else
              {
                // PB
                ss << std::fixed << std::setprecision(2) << (insize * 1.0 / PB) << " PB";
              }
            }
            else
            {
              // TB
              ss << std::fixed << std::setprecision(2) << (insize * 1.0 / TB) << " TB";
            }
          }
          else
          {
            // GB
            ss << std::fixed << std::setprecision(2) << (insize * 1.0 / GB) << " GB";
          }
        }
        else
        {
          // MB
          ss << std::fixed << std::setprecision(2) << (insize * 1.0 / MB) << " MB";
        }
      }
      else
      {
        // KB
        ss << std::fixed << std::setprecision(2) << (insize * 1.0 / KB) << " KB";
      }
    }
    else
    {
      ss << std::fixed << insize << " B";
    }
    return ss.str();
  }


  std::string fname;
  std::string url;
  double      synchronicity;  // sync->0: async sync->100.: sync IO
  size_t      errors;         //< number of responses != SUCCESS out of the CVS file

  struct synchronicity_t
  {
    std::vector<double> reads;
    std::vector<double> writes;

    double ReadSynchronicity() const
    {
      if (reads.size())
      {
        return accumulate(reads.begin(), reads.end(), 0.0) / reads.size();
      }
      else
      {
        return 0;
      }
    }

    double WriteSynchronicity() const
    {
      if (writes.size())
      {
        return accumulate(writes.begin(), writes.end(), 0.0) / writes.size();
      }
      else
      {
        return 0;
      }
    }
  };

  synchronicity_t aggregated_synchronicity;

  std::map<std::string, uint64_t> ios;
  std::map<std::string, double>   delays;
  std::mutex                      mtx;  // only required for async callbacks
};
}
