
#ifndef __XRDMACAROONS_GENERATOR_H
#define __XRDMACAROONS_GENERATOR_H

#include "XrdAcc/XrdAccAuthorize.hh"
#include "XrdSys/XrdSysError.hh"

#include <bitset>
#include <exception>
#include <string>

namespace Macaroons {

class XrdMacaroonsGenerator {
public:
    XrdMacaroonsGenerator(XrdSysError &err) :
      m_log(err)
    {
        if (!Config()) {
            throw std::runtime_error("Failed to configure the xrootd macaroons generator");
        }
    }

    // Generate a macaroon, returning it as an encoded string.
    // @param id     - unique ID for the macaroon
    // @param path   - path to limit the macaroon to
    // @param opers  - the permitted operations bitmask for the macaroon
    // @param expiry - the Unix timestamp when the macaroon should expir
    std::string Generate(const std::string &id, const std::string &user, const std::string &path, const std::bitset<AOP_LastOp> &opers, time_t expiry, const std::vector<std::string> &other_caveats);

private:
    // Configure the macaroon generator, based on the global process configuration file
    bool Config();

    XrdSysError &m_log;
    std::string m_secret;
    std::string m_sitename;
};

}

#endif