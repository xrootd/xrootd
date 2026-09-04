#include "XrdClHttpTokenFileParser.hh"

#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClLog.hh"

#include "XrdOuc/XrdOucJson.hh"

#include <fstream>
#include <sstream>

using namespace std::string_literals;

bool XrdClHttp::ParseTokenFile( const std::string   &token_file,
                                CurlCopyOp::Headers &src_hdrs,
                                CurlCopyOp::Headers &dst_hdrs )
{
    XrdCl::Log *const log = XrdCl::DefaultEnv::GetLog();

    std::ifstream file(token_file);

    if (!file.is_open())
    {
        log->Warning(kLogXrdClHttp, "Failed to open token file");
        return false;
    }

    constexpr std::streamsize max_token_file_size = 10 * 1024;

    std::string content(max_token_file_size, '\0');
    file.read(&content[0], max_token_file_size);
    content.resize(file.gcount());

    std::string src_token;
    std::string dst_token;

    if (const auto json = nlohmann::json::parse(content, nullptr, false); json.is_object())
    {
        const auto read_token = [&json, log] (const char *key, std::string &token)
        {
            if (const auto value = json.find(key); value != json.end() && value->is_string() && !value->get_ref<const std::string &>().empty())
                token = value->get_ref<const std::string &>();
            else
                log->Warning(kLogXrdClHttp, "Property '%s' of the token file is not a non-empty string", key);
        };

        read_token("src", src_token);
        read_token("dst", dst_token);
    }
    else
    {
        std::istringstream lines(content);
        std::getline(lines, src_token);
        std::getline(lines, dst_token);
    }

    if (src_token.empty() && dst_token.empty())
    {
        log->Warning(kLogXrdClHttp, "Token file holds neither a <src> nor a <dst> token");
        return false;
    }

    const auto add_token = [] (CurlCopyOp::Headers &headers, const std::string &token)
    {
        if (!token.empty())
            headers.emplace_back("Authorization"s, "Bearer "s + token);
    };

    add_token(src_hdrs, src_token);
    add_token(dst_hdrs, dst_token);

    return true;
}
