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
    static XrdVomsMapfile *Configure(XrdSysError *, XrdHttpSecXtractor *);
    static XrdVomsMapfile *Get();

    virtual int GetSecData(XrdLink *, XrdSecEntity &, SSL *);
    int Apply(XrdSecEntity &);

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

    std::string m_mapfile;
    std::shared_ptr<const std::vector<MapfileEntry>> m_entries;
    XrdHttpSecXtractor *m_xrdvoms{nullptr};
    XrdSysError *m_edest{nullptr};

    std::atomic<time_t> m_last_update;

    // Singleton
    static std::unique_ptr<XrdVomsMapfile> mapper;
    static bool tried_configure;
};
