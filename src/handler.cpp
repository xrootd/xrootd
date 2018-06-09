
#include <regex>
#include <cstring>
#include <string>
#include <iostream>

#include "uuid.h"
#include "json.h"
#include "macaroons.h"

#include "XrdAcc/XrdAccPrivs.hh"
#include "XrdAcc/XrdAccAuthorize.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSec/XrdSecEntity.hh"

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


std::string
Handler::GenerateID(const XrdSecEntity &entity, const std::string &activities,
                    const std::string &before)
{
    uuid_t uu;
    uuid_generate_random(uu);
    char uuid_buf[37];
    uuid_unparse(uu, uuid_buf);
    std::string result(uuid_buf);

    std::stringstream ss;
    ss << "ID=" << result << ", ";
    if (entity.prot[0] != '\0') {ss << "protocol=" << entity.prot << ", ";}
    if (entity.name) {ss << "name=" << entity.name << ", ";}
    if (entity.host) {ss << "host=" << entity.host << ", ";}
    if (entity.vorg) {ss << "vorg=" << entity.vorg << ", ";}
    if (entity.role) {ss << "vorg=" << entity.role << ", ";}
    if (entity.grps) {ss << "vorg=" << entity.grps << ", ";}
    if (entity.endorsements) {ss << "vorg=" << entity.endorsements << ", ";}
    if (activities.size()) {ss << "activities=" << activities << ", ";}
    ss << "expires=" << before;

    m_log->Emsg("MacaroonGen", ss.str().c_str());
    return result;
}

std::string
Handler::GenerateActivities(const XrdHttpExtReq & req) const
{
    std::string result = "activities:READ_METADATA,";
    // TODO - generate environment object that includes the Authorization header.
    XrdAccPrivs privs = m_chain ? m_chain->Access(&req.GetSecEntity(), req.resource.c_str(), AOP_Any, NULL) : XrdAccPriv_None;
    if ((privs & XrdAccPriv_Create) == XrdAccPriv_Create) {result += ",UPLOAD";}
    if (privs & XrdAccPriv_Read) {result += ",DOWNLOAD";}
    if (privs & XrdAccPriv_Delete) {result += ",DELETE";}
    if ((privs & XrdAccPriv_Chown) == XrdAccPriv_Chown) {result += ",MANAGE,UPDATE_METADATA";}
    if (privs & XrdAccPriv_Readdir) {result += ",LIST";}
    return result;
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
    if (!strftime(utc_time_buf, 21, "%FT%TZ", gmtime(&now)))
    {
        return req.SendSimpleResp(500, NULL, NULL, "Internal error constructing UTC time", 0);
    }
    std::string utc_time_str(utc_time_buf);
    std::string utc_time_caveat = "before:" + std::string(utc_time_buf);

    json_object *caveats_obj;
    std::vector<std::string> other_caveats;
    if (json_object_object_get_ex(macaroon_req, "caveats", &caveats_obj))
    {
        if (json_object_is_type(caveats_obj, json_type_array))
        { // Caveats were provided.  Let's record them.
          // TODO - could just add these in-situ.  No need for the other_caveats vector.
            int array_length = json_object_array_length(caveats_obj);
            other_caveats.reserve(array_length);
            for (int idx=0; idx<array_length; idx++)
            {
                json_object *caveat_item = json_object_array_get_idx(caveats_obj, idx);
                if (caveat_item)
                {
                    const char *caveat_item_str = json_object_get_string(caveat_item);
                    other_caveats.emplace_back(caveat_item_str);
                }
            }
        }
    }

    std::string activities = GenerateActivities(req);
    std::string macaroon_id = GenerateID(req.GetSecEntity(), activities, utc_time_str);
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
    struct macaroon *mac_with_activities = macaroon_add_first_party_caveat(mac,
                                             reinterpret_cast<const unsigned char*>(activities.c_str()),
                                             activities.size(),
                                             &mac_err);
    macaroon_destroy(mac);
    if (!mac_with_activities)
    {
        return req.SendSimpleResp(500, NULL, NULL, "Internal error adding default activities to macaroon", 0);
    }

    for (const auto &caveat : other_caveats)
    {
        struct macaroon *mac_tmp = mac_with_activities;
        mac_with_activities = macaroon_add_first_party_caveat(mac_tmp,
            reinterpret_cast<const unsigned char*>(caveat.c_str()),
            caveat.size(),
            &mac_err);
        macaroon_destroy(mac_tmp);
        if (!mac_with_activities)
        {
            return req.SendSimpleResp(500, NULL, NULL, "Internal error adding user caveat to macaroon", 0);
        }
    }

    std::string path_caveat = "path:" + req.resource;
    struct macaroon *mac_with_path = macaroon_add_first_party_caveat(mac_with_activities,
                                                 reinterpret_cast<const unsigned char*>(path_caveat.c_str()),
                                                 path_caveat.size(),
                                                 &mac_err);
    macaroon_destroy(mac_with_activities);
    if (!mac_with_path) {
        return req.SendSimpleResp(500, NULL, NULL, "Internal error adding path to macaroon", 0);
    }

    struct macaroon *mac_with_date = macaroon_add_first_party_caveat(mac_with_path,
                                        reinterpret_cast<const unsigned char*>(utc_time_caveat.c_str()),
                                        strlen(utc_time_buf),
                                        &mac_err);
    macaroon_destroy(mac_with_path);
    if (!mac_with_date) {
        return req.SendSimpleResp(500, NULL, NULL, "Internal error adding date to macaroon", 0);
    }

    size_t size_hint = macaroon_serialize_size_hint(mac_with_date, MACAROON_V1);

    std::vector<char> macaroon_resp; macaroon_resp.reserve(size_hint);
    if (!(size_hint = macaroon_serialize(mac_with_date, MACAROON_V1, reinterpret_cast<unsigned char*>(&macaroon_resp[0]), size_hint, &mac_err)))
    {
        return req.SendSimpleResp(500, NULL, NULL, "Internal error serializing macaroon", 0);
    }

    json_object *response_obj = json_object_new_object();
    if (!response_obj)
    {
        return req.SendSimpleResp(500, NULL, NULL, "Unable to create new JSON response object.", 0);
    }
    json_object *macaroon_obj = json_object_new_string_len(&macaroon_resp[0], size_hint);
    if (!macaroon_obj)
    {
        return req.SendSimpleResp(500, NULL, NULL, "Unable to create a new JSON macaroon string.", 0);
    }
    json_object_object_add(response_obj, "macaroon", macaroon_obj);

    const char *macaroon_result = json_object_to_json_string_ext(response_obj, JSON_C_TO_STRING_PRETTY);
    int retval = req.SendSimpleResp(200, NULL, NULL, macaroon_result, 0);
    json_object_put(response_obj);
    return retval;
}

