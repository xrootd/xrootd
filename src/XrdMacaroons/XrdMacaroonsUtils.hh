#pragma once

#include <string>

class XrdSysError;

namespace Macaroons {

bool GetSecretKey(const std::string &filename, XrdSysError *log, std::string &secret);

std::string NormalizeSlashes(const std::string &input);

}
