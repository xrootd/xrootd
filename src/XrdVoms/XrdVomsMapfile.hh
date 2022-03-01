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

#include "XrdOuc/XrdOucString.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSec/XrdSecEntity.hh"

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#define VOMS_MAP_FAILED ((XrdVomsMapfile *)-1)

class XrdVomsMapfile {

public:
    virtual ~XrdVomsMapfile();

    // Returns `nullptr` if the mapfile was not configured; returns
    // VOMS_MAP_FAILED (`(void*)-1`) if the mapfile was configured but it
    // was unable to be parsed (or other error occurred).
    static XrdVomsMapfile *Configure(XrdSysError *);
    static XrdVomsMapfile *Get();

    int Apply(XrdSecEntity &);

    bool IsValid() const {return m_is_valid;}

private:
    bool Reconfigure();
    void SetErrorStream(XrdSysError *erp) {if (erp) {m_edest = erp;}}

    XrdVomsMapfile(XrdSysError *erp, const std::string &mapfile);

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
    XrdSysError *m_edest{nullptr};

        // After success, how long to wait until the next mapfile check.
    static constexpr unsigned m_update_interval = 30;

    // Singleton
    static std::unique_ptr<XrdVomsMapfile> mapper;
    // There are multiple protocol objects that may need the mapfile object;
    // if we already tried-and-failed configuration once, this singleton will
    // help us avoid failing again.
    static bool tried_configure;
};
