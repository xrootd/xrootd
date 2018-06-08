
#include <regex>
#include <cstring>
#include <string>
#include <iostream>

#include "json.h"
#include "macaroons.h"

#include "handler.hh"

using namespace Macaroons;

static
ssize_t determine_validity(const std::string& input)
{
    ssize_t duration = 0;
    if (input.find("PT") != 0)
    {
        return -1;
    }
    size_t pos = 2;
    std::string remaining = input;
    do
    {
        remaining = remaining.substr(pos);
        if (remaining.size() == 0) break;
        long cur_duration;
        try
        {
            cur_duration = stol(remaining, &pos);
        } catch (...)
        {
            return -1;
        }
        if (pos >= remaining.size())
        {
            return -1;
        }
        char unit = remaining[pos];
        switch (unit) {
        case 'S':
            break;
        case 'M':
            cur_duration *= 60;
            break;
        case 'H':
            cur_duration *= 3600;
            break;
        default:
            return -1;
        };
        pos ++;
        duration += cur_duration;
    } while (1);
    return duration;
}

// See if the macaroon handler is interested in this request.
// We intercept all POST requests as we will be looking for a particular
// header.
bool
Handler::MatchesPath(const char *verb, const char *path)
{
    return !strcmp(verb, "POST");
}


// Process a macaroon request.
int Handler::ProcessReq(XrdHttpExtReq &req)
{
    auto header = req.headers.find("Content-Type");
    if (header == req.headers.end())
    {
        return req.SendSimpleResp(400, NULL, NULL, "Content-Type missing; not a valid macaroon request?", 0);
    }
    if (header->second != "application/macaroon-request")
    {
        return req.SendSimpleResp(400, NULL, NULL, "Content-Type must be set to `application/macaroon-request' to request a macaroon", 0);
    }
    header = req.headers.find("Content-Length");
    if (header == req.headers.end())
    {
        return req.SendSimpleResp(400, NULL, NULL, "Content-Length missing; not a valid POST", 0);
    }
    ssize_t blen;
    try
    {
        blen = std::stoll(header->second);
    }
    catch (...)
    {
        return req.SendSimpleResp(400, NULL, NULL, "Content-Length not parseable.", 0);
    }
    if (blen <= 0)
    {
        return req.SendSimpleResp(400, NULL, NULL, "Content-Length has invalid value.", 0);
    }
    //for (const auto &header : req.headers) { printf("** Request header: %s=%s\n", header.first.c_str(), header.second.c_str()); }
    char *request_data;
    if (req.BuffgetData(blen, &request_data, true) != blen)
    {
        return req.SendSimpleResp(400, NULL, NULL, "Missing or invalid body of request.", 0);
    }
    json_object *macaroon_req = json_tokener_parse(request_data);
    if (!macaroon_req)
    {
        return req.SendSimpleResp(400, NULL, NULL, "Invalid JSON serialization of macaroon request.", 0);
    }
    json_object *validity_obj;
    if (!json_object_object_get_ex(macaroon_req, "validity", &validity_obj))
    {
        return req.SendSimpleResp(400, NULL, NULL, "JSON request does not include a `validity`", 0);
    }
    const char *validity_cstr = json_object_get_string(validity_obj);
    if (!validity_cstr)
    {
        return req.SendSimpleResp(400, NULL, NULL, "validity key cannot be cast to a string", 0);
    }
    std::string validity_str(validity_cstr);
    ssize_t validity = determine_validity(validity_str);
    if (validity <= 0)
    {
        return req.SendSimpleResp(400, NULL, NULL, "Invalid ISO 8601 duration for validity key", 0);
    }
    time_t now;
    time(&now);
    now += validity;
    char utc_time_buf[21];
    if (!strftime(utc_time_buf, 28, "before:%FT%TZ", gmtime(&now)))
    {
        return req.SendSimpleResp(500, NULL, NULL, "Internal error constructing UTC time", 0);
    }

    std::string macaroon_id = "id1";
    enum macaroon_returncode mac_err;

    struct macaroon *mac = macaroon_create(reinterpret_cast<const unsigned char*>(m_location.c_str()),
                                           m_location.size(),
                                           reinterpret_cast<const unsigned char*>(m_secret.c_str()),
                                           m_secret.size(),
                                           reinterpret_cast<const unsigned char*>(macaroon_id.c_str()),
                                           macaroon_id.size(), &mac_err);
    if (!mac) {
        return req.SendSimpleResp(500, NULL, NULL, "Internal error constructing the macaroon", 0);
    }
    struct macaroon *mac_with_date = macaroon_add_first_party_caveat(mac,
                                                     reinterpret_cast<const unsigned char*>(utc_time_buf),
                                                     strlen(utc_time_buf),
                                                     &mac_err);
    macaroon_destroy(mac);

    size_t size_hint = macaroon_serialize_size_hint(mac_with_date, MACAROON_V1);

    std::vector<char> macaroon_resp; macaroon_resp.reserve(size_hint);
    if (!(size_hint = macaroon_serialize(mac_with_date, MACAROON_V1, reinterpret_cast<unsigned char*>(&macaroon_resp[0]), size_hint, &mac_err)))
    {
        return req.SendSimpleResp(500, NULL, NULL, "Internal error serializing macaroon", 0);
    }
    
    return req.SendSimpleResp(500, NULL, NULL, &macaroon_resp[0], size_hint);
}

