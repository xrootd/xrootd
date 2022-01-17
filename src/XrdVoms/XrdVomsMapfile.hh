/******************************************************************************/
/*                                                                            */
/*                        X r d V o m s M a p f i l e . h h                   */
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

#include "XrdHttp/XrdHttpSecXtractor.hh"

#include "XrdOuc/XrdOucString.hh"

#include <atomic>
#include <memory>
#include <string>
#include <vector>

class XrdVomsMapfile : public XrdHttpSecXtractor {

public:
    virtual ~XrdVomsMapfile();

    static XrdVomsMapfile *Configure(XrdSysError *, XrdHttpSecXtractor *);
    static XrdVomsMapfile *Get();

    virtual int GetSecData(XrdLink *, XrdSecEntity &, SSL *);
    int Apply(XrdSecEntity &);

    bool IsValid() const {return m_is_valid;}

    /* Base class returns an error if these aren't overridden */
    virtual int Init(SSL_CTX *, int) {return 0;}
    virtual int InitSSL(SSL *ssl, char *cadir) {return 0;}
    virtual int FreeSSL(SSL *) {return 0;}

private:
    bool Reconfigure();
    void SetErrorStream(XrdSysError *erp) {if (erp) {m_edest = erp;}}
    void SetExtractor(XrdHttpSecXtractor *xtractor) {if (xtractor) {m_xrdvoms = xtractor;}}

    XrdVomsMapfile(XrdSysError *erp, XrdHttpSecXtractor *xrdvoms, const std::string &mapfile);

    enum LogMask {
        Debug = 0x01,
        Info = 0x02,
        Warning = 0x04,
        Error = 0x08,
        All = 0xff
    };

    struct MapfileEntry {
        std::vector<std::string> m_path;
        std::string m_target;
    };

    bool ParseMapfile(const std::string &mapfile);
    bool ParseLine(const std::string &line, std::vector<std::string> &entry, std::string &target);

    std::string Map(const std::vector<std::string> &fqan);
    bool Compare(const MapfileEntry &entry, const std::vector<std::string> &fqan);
    std::vector<std::string> MakePath(const XrdOucString &group);

    // A continuously-running thread for maintenance tasks (reloading the mapfile)
    static void *MaintenanceThread(void *myself_raw);

    // Set to true if the last maintenance attempt succeeded.
    bool m_is_valid = false;
    // Time of the last observed status change of file.
    struct timespec m_mapfile_ctime{0, 0};

    std::string m_mapfile;
    std::shared_ptr<const std::vector<MapfileEntry>> m_entries;
    XrdHttpSecXtractor *m_xrdvoms{nullptr};
    XrdSysError *m_edest{nullptr};

    // Pipes to allow the main thread to communicate shutdown events to the maintenance
    // thread, allowing for a clean shutdown.
    int m_maintenance_pipe_r{-1};
    int m_maintenance_pipe_w{-1};
    int m_maintenance_thread_pipe_r{-1};
    int m_maintenance_thread_pipe_w{-1};

        // After success, how long to wait until the next mapfile check.
    static constexpr unsigned m_update_interval = 30;
        // After failure, how long to wait until the next mapfile check.
    static constexpr unsigned m_update_interval_failure = 3;

    // Singleton
    static std::unique_ptr<XrdVomsMapfile> mapper;
    // There are multiple protocol objects that may need the mapfile object;
    // if we already tried-and-failed configuration once, this singleton will
    // help us avoid failing again.
    static bool tried_configure;
};
