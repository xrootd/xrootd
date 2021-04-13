/******************************************************************************/
/*                                                                            */
/*               X r d T l s T e m p C A . h h                                */
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

#include <string>
#include <memory>

// Forward dec'ls.
class XrdSysError;

/**
 * This class provides manages a "CA file" that is a concatenation of all the
 * CAs in a given CA directory.  This is useful in TLS contexts where, instead
 * of loading all CAs for each connection, we only want to load a single file.
 *
 * This will hand out the CA file directly, allowing external libraries (such
 * as libcurl) do the loading of CAs directly.
 */
class XrdTlsTempCA {
public:
    class TempCAGuard;

    XrdTlsTempCA(XrdSysError *log, std::string ca_dir);
    ~XrdTlsTempCA();

    /**
     * Returns true if object is valid.
     */
    bool IsValid() const {return m_ca_file.get() && m_crl_file.get();}

    /**
     * Returns the current location of the CA temp file.
     */
    std::string CAFilename() const {auto file_ref = m_ca_file; return file_ref ? *file_ref : "";}

    /**
     * Returns the current location of the CA temp file.
     */
    std::string CRLFilename() const {auto file_ref = m_crl_file; return file_ref ? *file_ref : "";}

    /**
     * Manages the temporary file associated with the curl handle
     */
    class TempCAGuard {
    public:
        static std::unique_ptr<TempCAGuard> create(XrdSysError &, const std::string &ca_tmp_dir);

    int getCAFD() const {return m_ca_fd;}
    std::string getCAFilename() const {return m_ca_fname;}

    int getCRLFD() const {return m_crl_fd;}
    std::string getCRLFilename() const {return m_crl_fname;}

    /**
     * Move temporary file to the permanent location.
     */
    bool commit();

    TempCAGuard(const TempCAGuard &) = delete;

    ~TempCAGuard();

    private:
        TempCAGuard(int ca_fd, int crl_fd, const std::string &ca_tmp_dir, const std::string &ca_fname, const std::string &crl_fname);

        int m_ca_fd{-1};
        int m_crl_fd{-1};
        std::string m_ca_tmp_dir;
        std::string m_ca_fname;
        std::string m_crl_fname;
    };


private:
    /** 
     * Run the CA maintenance routines.
     * This will go through the CA directory, concatenate the
     * CA contents into a single PEM file, and delete the prior
     * copy of the concatenated CA certs.
     */
    bool Maintenance();

    /**
     * Thread managing the invocation of the CA maintenance routines
     */
    static void *MaintenanceThread(void *myself_raw);

    /**
     * Read and write ends of a pipe to communicate between the parent
     * object and the maintenance thread.
     */
    int m_maintenance_pipe_r{-1};
    int m_maintenance_pipe_w{-1};
    int m_maintenance_thread_pipe_r{-1};
    int m_maintenance_thread_pipe_w{-1};
    XrdSysError &m_log;
    const std::string m_ca_dir;
    std::shared_ptr<std::string> m_ca_file;
    std::shared_ptr<std::string> m_crl_file;

        // After success, how long to wait until the next CA reload.
    static constexpr unsigned m_update_interval = 900;
        // After failure, how long to wait until the next CA reload.
    static constexpr unsigned m_update_interval_failure = 10;
};
