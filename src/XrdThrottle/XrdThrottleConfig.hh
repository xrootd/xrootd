/******************************************************************************/
/*                                                                            */
/* (c) 2025 by the Morgridge Institute for Research                           */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
/*                                                                            */
/* XRootD is free software: you can redistribute it and/or modify it under    */
/* the terms of the GNU Lesser General Public License as published by the     */
/* Free Software Foundation, either version 3 of the License, or (at your     */
/* option) any later version.                                                 */
/*                                                                            */
/* XRootD is distributed in the hope that it will be useful, but WITHOUT      */
/* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or      */
/* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public       */
/* License for more details.                                                  */
/*                                                                            */
/* You should have received a copy of the GNU Lesser General Public License   */
/* along with XRootD in a file called COPYING.LESSER (LGPL license) and file  */
/* COPYING (GPL license).  If not, see <http://www.gnu.org/licenses/>.        */
/*                                                                            */
/* The copyright holder's institutional names and contributor's names may not */
/* be used to endorse or promote products derived from this software without  */
/* specific prior written permission of the institution or contributor.       */
/******************************************************************************/

#ifndef XrdThrottle_Config_hh
#define XrdThrottle_Config_hh

#include <string>

class XrdOucEnv;
class XrdOucStream;
class XrdSysError;

namespace XrdThrottle {

class Configuration {
public:
    Configuration(XrdSysError & log, XrdOucEnv *env)
    : m_env(env), m_log(log)
    {}

    // Generate the XrdThrottle configuration from the given file name.
    //
    // Returns 0 on success, or a non-zero error code on failure.
    int Configure(const std::string &config_file);

    // Get the configuration for the fslib to use.
    // The default is "libXrdOfs.so".
    const std::string &GetFileSystemLibrary() const { return m_fslib; }

    // Get the configuration for the loadshed hostname.
    // If not set, the default is empty.
    const std::string &GetLoadshedHost() const { return m_loadshed_hostname; }

    // Get the configuration for the loadshed port.
    // Valid values are 1 to 65535.
    // If not set, the default is 0.
    long long GetLoadshedPort() const { return m_loadshed_port; }

    // Get the configuration for the loadshed frequency.
    // Valid values are 1 to 100.
    // If not set, the default is 0.
    long long GetLoadshedFreq() const { return m_loadshed_freq; }

    // Get the configuration for th maximum number of open files.
    // If -1, no limit is set.
    long long GetMaxOpen() const { return m_max_open; }

    // Get the configuration for the maximum number of active connections.
    // If -1, no limit is set.
    long long GetMaxConn() const { return m_max_conn; }

    // Get the configuration for the maximum wait time before a request is
    // failed with EMFILE.
    // If not set, the default is 30 seconds.
    long long GetMaxWait() const { return m_max_wait; }

    // Get the configuration for the throttle concurrency limit.
    // If -1, no limit is set.
    long long GetThrottleConcurrency() const { return m_throttle_concurrency_limit; }

    // Get the configuration for the maximum number of bytes per second.
    // If -1, no limit is set.
    long long GetThrottleDataRate() const { return m_throttle_data_rate; }

    // Get the configuration for the maximum number of IOPS per second.
    // If -1, no limit is set.
    long long GetThrottleIOPSRate() const { return m_throttle_iops_rate; }

    // Get the configuration for the recompute interval, in milliseconds.
    // If not set, the default is 1000 ms.
    long long GetThrottleRecomputeIntervalMS() const { return m_throttle_recompute_interval_ms; }

    // Get the configuration for the trace levels.
    // If not set, the default is 0.
    int GetTraceLevels() const { return m_trace_levels; }

private:
    int xloadshed(XrdOucStream &Config);
    int xmaxopen(XrdOucStream &Config);
    int xmaxconn(XrdOucStream &Config);
    int xmaxwait(XrdOucStream &Config);
    int xthrottle(XrdOucStream &Config);
    int xtrace(XrdOucStream &Config);

    XrdOucEnv *m_env{nullptr};
    std::string m_fslib{"libXrdOfs.so"};
    XrdSysError &m_log;
    std::string m_loadshed_hostname;
    long long m_loadshed_freq{0};
    long long m_loadshed_port{0};
    long long m_max_conn{-1};
    long long m_max_open{-1};
    long long m_max_wait{30};
    long long m_throttle_concurrency_limit{-1};
    long long m_throttle_data_rate{-1};
    long long m_throttle_iops_rate{-1};
    long long m_throttle_recompute_interval_ms{1000};
    int m_trace_levels{0};
};

} // namespace XrdThrottle

#endif // XrdThrottle_Config_hh
