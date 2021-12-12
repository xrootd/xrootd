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
#include "XrdSys/XrdSysError.hh"

#include <memory>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <fcntl.h>


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

}


XrdVomsMapfile::XrdVomsMapfile(XrdSysError *erp, XrdHttpSecXtractor *xrdvoms,
    const std::string &mapfile)
    : m_mapfile(mapfile), m_xrdvoms(xrdvoms), m_edest(erp)
{
    m_last_update.store(0, std::memory_order_relaxed);
}


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
XrdVomsMapfile::Reconfigure() {
    auto now = time(NULL);
    auto retval = true;
    std::stringstream ss;
    ss << "Last update " << m_last_update.load(std::memory_order_relaxed) << ", " << now;
    m_edest->Log(LogMask::Debug, "VOMS Mapfile", ss.str().c_str());
    if (now > m_last_update.load(std::memory_order_relaxed) + 30) {
        retval = ParseMapfile(m_mapfile);
        m_last_update.store(now, std::memory_order_relaxed);
    }
    return retval;
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
XrdVomsMapfile::Map(const std::vector<string> &fqan)
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
    while ((from = group.tokenize(entry, from, '/')) != -1) {
        if (entry.empty()) continue;
        path.emplace_back(entry.c_str());
    }
    return path;
}


int
XrdVomsMapfile::GetSecData(XrdLink * lnk, XrdSecEntity &entity, SSL *ssl)
{
    if (!m_xrdvoms) return -1;

    auto retval = m_xrdvoms->GetSecData(lnk, entity, ssl);
    if (retval) return retval;

    return Apply(entity);
}


int
XrdVomsMapfile::Apply(XrdSecEntity &entity)
{
    Reconfigure();

    int from_vorg = 0, from_role = 0, from_grps = 0;
    XrdOucString vorg = entity.vorg, entry_vorg;
    XrdOucString role = entity.role, entry_role;
    XrdOucString grps = entity.grps, entry_grps;
    if (m_edest) m_edest->Log(LogMask::Debug, "VOMSMapfile", "Applying VOMS mapfile to incoming credential");
    while (((from_vorg = vorg.tokenize(entry_vorg, from_vorg, ' ')) != -1) &&
           ((from_role = role.tokenize(entry_role, from_role, ' ')) != -1) &&
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
XrdVomsMapfile::Configure(XrdSysError *erp, XrdHttpSecXtractor *xtractor)
{
    if (tried_configure) {
        auto result = mapper.get();
        if (result) {
            result->SetExtractor(xtractor);
            result->SetErrorStream(erp);
        }
        return result;
    }

    tried_configure = true;

    // Set default mask for logging.
    if (erp) erp->setMsgMask(LogMask::Error | LogMask::Warning);

    char *config_filename = nullptr;
    if (!XrdOucEnv::Import("XRDCONFIGFN", config_filename)) {
        return nullptr;
    }
    XrdOucStream stream(erp, getenv("XRDINSTANCE"));

    int cfg_fd;
    if ((cfg_fd = open(config_filename, O_RDONLY, 0)) < 0) {
        if (erp) erp->Emsg("Config", errno, "open config file", config_filename);
        return nullptr;
    }
    stream.Attach(cfg_fd);
    char *var;
    std::string map_filename;
    while ((var = stream.GetMyFirstWord())) {
        if (!strcmp(var, "voms.mapfile")) {
            auto val = stream.GetWord();
            if (!val || !val[0]) {
                if (erp) erp->Emsg("Config", "VOMS mapfile not specified");
                return nullptr;
            }
            map_filename = val;
        } else if (!strcmp(var, "voms.trace")) {
            auto val = stream.GetWord();
            if (!val || !val[0]) {
                if (erp) erp->Emsg("Config", "VOMS logging level not specified");
                return nullptr;
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
        mapper.reset(new XrdVomsMapfile(erp, xtractor, map_filename));
        mapper->Reconfigure();
    }

    return mapper.get();
}
