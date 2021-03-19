/******************************************************************************/
/*                                                                            */
/*               X r d T p c N S S S u p p o r t . h h                        */
/*                                                                            */
/* (c) 2021 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*   Produced by Brian Bockelman                                              */
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

#include <atomic>
#include <string>
#include <memory>

#include "XrdOuc/XrdOucPinLoader.hh"

// Forward dec'ls.
class XrdSysError;
typedef void CURL;

namespace TPC {

/**
 * libcurl with the NSS backend has significant memory leaks around the CA handling
 * code.  We have discovered that the memory leaks are *smallest* when NSS is given
 * all the CA certificates in a single file (as opposed to many files in a directory).
 * 
 * This class takes a traditional grid CA directory, parses its contents, and creates
 * a single file.
 * 
 * Each restart of the server this temporary file is created; further, every hour a
 * new copy of the CAs is made.
 */
class XrdTpcNSSSupport {
public:
    class TempCAGuard;

    XrdTpcNSSSupport(XrdSysError *log, std::string ca_dir);

    /**
     * Returns true if the current version of libcurl needs the NSS hack.
     */
    static bool NeedsNSSHack();

    /** 
     * Run the CA maintenance routines.
     * This will go through the CA directory, concatenate the
     * CA contents into a single PEM file, and delete the prior
     * copy of the concatenated CA certs.
     */
    bool Maintenance();

    /**
     * Given a `curl` handle, tweak it to utilize the new CA directory.
     */
    std::shared_ptr<TempCAGuard> ConfigureCurl(CURL *);

    /**
     * Returns true if object is valid.
     */
    bool IsValid() const {return m_ca_file.get();}

    /**
     * Manages the temporary file associated with the curl handle
     */
    class TempCAGuard {
    public:
        static std::unique_ptr<TempCAGuard> create(XrdSysError &);

    int getFD() const {return m_fd;}
    std::string getFilename() const {return m_fname;}

    TempCAGuard(const TempCAGuard &) = delete;

    ~TempCAGuard();

    private:
        TempCAGuard(int fd, const std::string &fname);

        int m_fd;
        std::string m_fname;
    };


private:
    bool NeedsMaintenance();

    std::atomic<time_t> m_next_update{0};
    XrdSysError &m_log;
    const std::string m_ca_dir;
    std::shared_ptr<TempCAGuard> m_ca_file;
    void *m_x509_to_file_func{nullptr};
    void *m_file_to_x509_func{nullptr};
    XrdOucPinLoader m_plugin;
};

}
