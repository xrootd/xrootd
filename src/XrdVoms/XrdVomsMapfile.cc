/******************************************************************************/
/*                                                                            */
/*                        X r d V o m s M a p f i l e . c c                   */
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

#include "XrdVoms/XrdVomsMapfile.hh"

#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucString.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdSec/XrdSecEntity.hh"
#include "XrdSec/XrdSecEntityAttr.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysFD.hh"
#include "XrdSys/XrdSysPthread.hh"

#include <memory>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <fcntl.h>
#include <poll.h>

bool XrdVomsMapfile::tried_configure = false;
std::unique_ptr<XrdVomsMapfile> XrdVomsMapfile::mapper;

namespace {

std::string
PathToString(const std::vector<std::string> &path)
{
    if (path.empty()) {return "/";}
    std::stringstream ss;
    for (const auto &entry : path) {
        ss << "/" << entry;
    }

    return ss.str();
}

uint64_t monotonic_time_s() {
  struct timespec tp;
  clock_gettime(CLOCK_MONOTONIC, &tp);
  return tp.tv_sec + (tp.tv_nsec >= 500000000);
}

}


XrdVomsMapfile::XrdVomsMapfile(XrdSysError *erp,
    const std::string &mapfile)
    : m_mapfile(mapfile), m_edest(erp)
{
    struct stat statbuf;
    if (-1 == stat(m_mapfile.c_str(), &statbuf)) {
        m_edest->Emsg("XrdVomsMapfile", errno, "Error checking the mapfile", m_mapfile.c_str());
        return;
    }
    memcpy(&m_mapfile_ctime, &statbuf.st_ctim, sizeof(decltype(m_mapfile_ctime)));

    if (!ParseMapfile(m_mapfile)) {return;}

    pthread_t tid;
    auto rc = XrdSysThread::Run(&tid, XrdVomsMapfile::MaintenanceThread,
                                static_cast<void*>(this), 0, "VOMS Mapfile refresh");
    if (rc) {
        m_edest->Emsg("XrdVomsMapfile", "Failed to launch VOMS mapfile monitoring thread");
        return;
    }
    m_is_valid = true;
}


XrdVomsMapfile::~XrdVomsMapfile()
{}


bool
XrdVomsMapfile::ParseMapfile(const std::string &mapfile)
{
    std::ifstream fstr(mapfile);
    if (!fstr.is_open()) {
        m_edest->Emsg("ParseMapfile", "Failed to open file", mapfile.c_str(), strerror(errno));
        return false;
    }
    std::shared_ptr<std::vector<MapfileEntry>> entries(new std::vector<MapfileEntry>());
    for (std::string line; std::getline(fstr, line); ) {
        MapfileEntry entry;
        if (ParseLine(line, entry.m_path, entry.m_target) && !entry.m_path.empty()) {
            if (m_edest->getMsgMask() & LogMask::Debug) {
                m_edest->Log(LogMask::Debug, "ParseMapfile", PathToString(entry.m_path).c_str(), "->", entry.m_target.c_str());
            }
            entries->emplace_back(entry);
        }
    }
    m_entries = entries;
    return true;
}


bool
XrdVomsMapfile::ParseLine(const std::string &line, std::vector<std::string> &entry, std::string &target)
{
    bool began_entry = false;
    bool finish_entry = false;
    bool began_target = false;
    std::string element;
    element.reserve(16);
    for (size_t idx=0; idx<line.size(); idx++) {
        auto txt = line[idx];
        if (!began_entry && !finish_entry) {
            if (txt == '#') {return false;}
            else if (txt == '"') {began_entry = true;}
            else if (!isspace(txt)) {return false;}
            continue;
        } else if (began_entry && !finish_entry) {
            if (txt == '\\') {
                if (idx + 1 == line.size()) {return false;}
                idx++;
                auto escaped_char = line[idx];
                switch (escaped_char) {
                case '\'':
                    element += "'";
                    break;
                case '\"':
                    element += "\"";
                    break;
                case '/':
                    element += "/";
                    break;
                case 'f':
                    element += "\f";
                    break;
                case 'n':
                    element += "\n";
                    break;
                case 'r':
                    element += "\r";
                    break;
                case 't':
                    element += "\t";
                    break;
                default:
                    return false;
                };
            } else if (txt == '"') {
                if (!element.empty()) entry.push_back(element);
                finish_entry = true;
            } else if (txt == '/') {
                if (!element.empty()) entry.push_back(element);
                element.clear();
            } else if (isprint(txt)) {
                element += txt;
            } else {
                return false;
            }
        } else if (!began_target) {
            if (isspace(txt)) {continue;}
            began_target = true;
        }
        if (began_target) {
            if (isprint(txt)) {
                target += txt;
            } else if (isspace(txt)) {
                return true;
            } else {
                return false;
            }
        }
    }
    return true;
}


std::string
XrdVomsMapfile::Map(const std::vector<std::string> &fqan)
{
    decltype(m_entries) entries = m_entries;
    if (!entries) {return "";}

    if (m_edest && (m_edest->getMsgMask() & LogMask::Debug)) {
        m_edest->Log(LogMask::Debug, "VOMSMapfile", "Mapping VOMS FQAN", PathToString(fqan).c_str());
    }

    for (const auto &entry : *entries) {
        if (Compare(entry, fqan)) {
            if (m_edest && (m_edest->getMsgMask() & LogMask::Debug)) {
                m_edest->Log(LogMask::Debug, "VOMSMapfile", "Mapped FQAN to target", entry.m_target.c_str());
            }
            return entry.m_target;
        }
    }
    return "";
}


bool
XrdVomsMapfile::Compare(const MapfileEntry &entry, const std::vector<std::string> &fqan)
{
    if (entry.m_path.empty()) {return false;}

    // A more specific mapfile entry cannot match a generic FQAN
    if (fqan.size() < entry.m_path.size()) {return false;}

    XrdOucString fqan_element;
    for (size_t idx=0; idx<entry.m_path.size(); idx++) {
        fqan_element.assign(fqan[idx].c_str(), 0);
        const auto &path_element = entry.m_path[idx];
        if (!fqan_element.matches(path_element.c_str())) {return false;}
    }
    if (fqan.size() == entry.m_path.size()) {return true;}
    if (entry.m_path.back() == "*") {return true;}
    return false;
}


std::vector<std::string>
XrdVomsMapfile::MakePath(const XrdOucString &group)
{
    int from = 0;
    XrdOucString entry;
    std::vector<std::string> path;
    path.reserve(4);
        // The const'ness of the tokenize method as declared is incorrect; we use
        // const_cast here to avoid fixing the XrdOucString header (which would break
        // the ABI).
    while ((from = const_cast<XrdOucString&>(group).tokenize(entry, from, '/')) != -1) {
        if (entry.length() == 0) continue;
        path.emplace_back(entry.c_str());
    }
    return path;
}


int
XrdVomsMapfile::Apply(XrdSecEntity &entity)
{
    // In current use cases, the gridmap results take precedence over the voms-mapfile
    // results.  However, the grid mapfile plugins often will populate the name attribute
    // with a reasonable default (DN or DN hash) if the mapping fails, meaning we can't
    // simply look at entity.name; instead, we look at an extended attribute that is only
    // set when the mapfile is used to generate the name.
    std::string gridmap_name;
    auto gridmap_success = entity.eaAPI->Get("gridmap.name", gridmap_name);
    if (gridmap_success && gridmap_name == "1") {
        return 0;
    }

    int from_vorg = 0, from_role = 0, from_grps = 0;
    XrdOucString vorg = entity.vorg, entry_vorg;
    XrdOucString role = entity.role ? entity.role : "", entry_role = "NULL";
    XrdOucString grps = entity.grps, entry_grps;
    if (m_edest) m_edest->Log(LogMask::Debug, "VOMSMapfile", "Applying VOMS mapfile to incoming credential");
    while (((from_vorg = vorg.tokenize(entry_vorg, from_vorg, ' ')) != -1) &&
           ((role == "") || (from_role = role.tokenize(entry_role, from_role, ' ')) != -1) &&
           ((from_grps = grps.tokenize(entry_grps, from_grps, ' ')) != -1))
    {
        auto fqan = MakePath(entry_grps);
        if (fqan.empty()) {continue;}

        // By convention, the root group should be the same as the VO name; however,
        // the VOMS mapfile makes this assumption.  To be secure, enforce it.
        if (strcmp(fqan[0].c_str(), entry_vorg.c_str())) {continue;}

        fqan.emplace_back(std::string("Role=") + entry_role.c_str());
        fqan.emplace_back("Capability=NULL");
        std::string username;
        if (!(username = Map(fqan)).empty()) {
            if (entity.name) {free(entity.name);}
            entity.name = strdup(username.c_str());
            break;
        }
    }

    return 0;
}


XrdVomsMapfile *
XrdVomsMapfile::Get()
{
    return mapper.get();
}


XrdVomsMapfile *
XrdVomsMapfile::Configure(XrdSysError *erp)
{
    if (tried_configure) {
        auto result = mapper.get();
        if (result) {
            result->SetErrorStream(erp);
        }
        return result;
    }

    tried_configure = true;

    // Set default mask for logging.
    if (erp) erp->setMsgMask(LogMask::Error | LogMask::Warning);

    char *config_filename = nullptr;
    if (!XrdOucEnv::Import("XRDCONFIGFN", config_filename)) {
        return VOMS_MAP_FAILED;
    }
    XrdOucStream stream(erp, getenv("XRDINSTANCE"));

    int cfg_fd;
    if ((cfg_fd = open(config_filename, O_RDONLY, 0)) < 0) {
        if (erp) erp->Emsg("Config", errno, "open config file", config_filename);
        return VOMS_MAP_FAILED;
    }
    stream.Attach(cfg_fd);
    char *var;
    std::string map_filename;
    while ((var = stream.GetMyFirstWord())) {
        if (!strcmp(var, "voms.mapfile")) {
            auto val = stream.GetWord();
            if (!val || !val[0]) {
                if (erp) erp->Emsg("Config", "VOMS mapfile not specified");
                return VOMS_MAP_FAILED;
            }
            map_filename = val;
        } else if (!strcmp(var, "voms.trace")) {
            auto val = stream.GetWord();
            if (!val || !val[0]) {
                if (erp) erp->Emsg("Config", "VOMS logging level not specified");
                return VOMS_MAP_FAILED;
            }
            if (erp) erp->setMsgMask(0);
            if (erp) do {
                if (!strcmp(val, "all")) {erp->setMsgMask(erp->getMsgMask() | LogMask::All);}
                else if (!strcmp(val, "error")) {erp->setMsgMask(erp->getMsgMask() | LogMask::Error);}
                else if (!strcmp(val, "warning")) {erp->setMsgMask(erp->getMsgMask() | LogMask::Warning);}
                else if (!strcmp(val, "info")) {erp->setMsgMask(erp->getMsgMask() | LogMask::Info);}
                else if (!strcmp(val, "debug")) {erp->setMsgMask(erp->getMsgMask() | LogMask::Debug);}
                else if (!strcmp(val, "none")) {erp->setMsgMask(0);}
                else {erp->Emsg("Config", "voms.trace encountered an unknown directive:", val);}
                val = stream.GetWord();
            } while (val);
        }
    }

    if (!map_filename.empty()) {
        if (erp) erp->Emsg("Config", "Will initialize VOMS mapfile", map_filename.c_str());
        mapper.reset(new XrdVomsMapfile(erp, map_filename));
        if (!mapper->IsValid()) {
            mapper.reset(nullptr);
            return VOMS_MAP_FAILED;
        }
    }

    return mapper.get();
}


void *
XrdVomsMapfile::MaintenanceThread(void *myself_raw)
{
    auto myself = static_cast<XrdVomsMapfile*>(myself_raw);

   auto now = monotonic_time_s();
   auto next_update = now + m_update_interval;
   while (true) {
       now = monotonic_time_s();
       auto remaining = next_update - now;
       auto rval = sleep(remaining);
       if (rval > 0) {
           // Woke up early due to a signal; re-run prior logic.
           continue;
       }
       next_update = monotonic_time_s() + m_update_interval;
       struct stat statbuf;
       if (-1 == stat(myself->m_mapfile.c_str(), &statbuf)) {
           myself->m_edest->Emsg("XrdVomsMapfile", errno, "Error checking the mapfile",
               myself->m_mapfile.c_str());
           myself->m_mapfile_ctime.tv_sec = 0;
           myself->m_mapfile_ctime.tv_nsec = 0;
           myself->m_is_valid = false;
           continue;
       }
       // Use ctime here as it is solely controlled by the OS (unlike mtime,
       // which can be manipulated by userspace and potentially not change
       // when updated - rsync, tar, and rpm, for example, all preserve mtime).
       // ctime also will also be updated appropriately for overwrites/renames,
       // allowing us to detect those changes as well.
       //
       if ((myself->m_mapfile_ctime.tv_sec == statbuf.st_ctim.tv_sec) &&
           (myself->m_mapfile_ctime.tv_nsec == statbuf.st_ctim.tv_nsec))
       {
           myself->m_edest->Log(LogMask::Debug, "Maintenance", "Not reloading VOMS mapfile; "
               "no changes detected.");
           continue;
       }
       memcpy(&myself->m_mapfile_ctime, &statbuf.st_ctim, sizeof(decltype(statbuf.st_ctim)));

       myself->m_edest->Log(LogMask::Debug, "Maintenance", "Reloading VOMS mapfile now");
       if ( !(myself->m_is_valid = myself->ParseMapfile(myself->m_mapfile)) ) {
           myself->m_edest->Log(LogMask::Error, "Maintenance", "Failed to reload VOMS mapfile");
       }
   }
   return nullptr;
}
