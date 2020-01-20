
#include <regex>
#include <cstring>
#include <string>
#include <iostream>

#include <uuid/uuid.h>
#include "json.h"
#include "macaroons.h"

#include "XrdAcc/XrdAccPrivs.hh"
#include "XrdAcc/XrdAccAuthorize.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSec/XrdSecEntity.hh"

#include "XrdMacaroonsHandler.hh"

using namespace Macaroons;


char *unquote(const char *str) {
  int l = strlen(str);
  char *r = (char *) malloc(l + 1);
  r[0] = '\0';
  int i, j = 0;

  for (i = 0; i < l; i++) {

    if (str[i] == '%') {
      char savec[3];
      if (l <= i + 3) {
        free(r);
        return NULL;
      }
      savec[0] = str[i + 1];
      savec[1] = str[i + 2];
      savec[2] = '\0';

      r[j] = strtol(savec, 0, 16);

      i += 2;
    } else if (str[i] == '+') r[j] = ' ';
    else r[j] = str[i];

    j++;
  }

  r[j] = '\0';

  return r;

}


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


Handler::~Handler()
{
    delete m_chain;
}


std::string
Handler::GenerateID(const std::string &resource,
                    const XrdSecEntity &entity,
                    const std::string &activities,
                    const std::vector<std::string> &other_caveats,
                    const std::string &before)
{
    uuid_t uu;
    uuid_generate_random(uu);
    char uuid_buf[37];
    uuid_unparse(uu, uuid_buf);
    std::string result(uuid_buf);

    std::stringstream ss;
    ss << "ID=" << result << ", ";
    ss << "resource=" << resource << ", ";
    if (entity.prot[0] != '\0') {ss << "protocol=" << entity.prot << ", ";}
    if (entity.name) {ss << "name=" << entity.name << ", ";}
    if (entity.host) {ss << "host=" << entity.host << ", ";}
    if (entity.vorg) {ss << "vorg=" << entity.vorg << ", ";}
    if (entity.role) {ss << "role=" << entity.role << ", ";}
    if (entity.grps) {ss << "groups=" << entity.grps << ", ";}
    if (entity.endorsements) {ss << "endorsements=" << entity.endorsements << ", ";}
    if (activities.size()) {ss << "base_activities=" << activities << ", ";}

    for (std::vector<std::string>::const_iterator iter = other_caveats.begin();
         iter != other_caveats.end();
         iter++)
    {
        ss << "user_caveat=" << *iter << ", ";
    }

    ss << "expires=" << before;

    m_log->Emsg("MacaroonGen", ss.str().c_str());
    return result;
}


std::string
Handler::GenerateActivities(const XrdHttpExtReq & req, const std::string &resource) const
{
    std::string result = "activity:READ_METADATA";
    // TODO - generate environment object that includes the Authorization header.
    XrdAccPrivs privs = m_chain ? m_chain->Access(&req.GetSecEntity(), resource.c_str(), AOP_Any, NULL) : XrdAccPriv_None;
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
    return !strcmp(verb, "POST") || !strncmp(path, "/.well-known/", 13) ||
           !strncmp(path, "/.oauth2/", 9);
}


int Handler::ProcessOAuthConfig(XrdHttpExtReq &req) {
    if (req.verb != "GET")
    {
        return req.SendSimpleResp(405, NULL, NULL, "Only GET is valid for oauth config.", 0);
    }
    auto header = req.headers.find("Host");
    if (header == req.headers.end())
    {
        return req.SendSimpleResp(400, NULL, NULL, "Host header is required.", 0);
    }

    json_object *response_obj = json_object_new_object();
    if (!response_obj)
    {
        return req.SendSimpleResp(500, NULL, NULL, "Unable to create new JSON response object.", 0);
    }
    std::string token_endpoint = "https://" + header->second + "/.oauth2/token";
    json_object *endpoint_obj =
        json_object_new_string_len(token_endpoint.c_str(), token_endpoint.size());
    if (!endpoint_obj)
    {
        return req.SendSimpleResp(500, NULL, NULL, "Unable to create a new JSON macaroon string.", 0);
    }
    json_object_object_add(response_obj, "token_endpoint", endpoint_obj);

    const char *response_result = json_object_to_json_string_ext(response_obj, JSON_C_TO_STRING_PRETTY);
    int retval = req.SendSimpleResp(200, NULL, NULL, response_result, 0);
    json_object_put(response_obj);
    return retval;
}


int Handler::ProcessTokenRequest(XrdHttpExtReq &req)
{
    if (req.verb != "POST")
    {
        return req.SendSimpleResp(405, NULL, NULL, "Only POST is valid for token request.", 0);
    }
    auto header = req.headers.find("Content-Type");
    if (header == req.headers.end())
    {
        return req.SendSimpleResp(400, NULL, NULL, "Content-Type missing; not a valid macaroon request?", 0);
    }
    if (header->second != "application/x-www-form-urlencoded")
    {
        return req.SendSimpleResp(400, NULL, NULL, "Content-Type must be set to `application/macaroon-request' to request a macaroon", 0);
    }
    char *request_data_raw;
    // Note: this does not null-terminate the buffer contents.
    if (req.BuffgetData(req.length, &request_data_raw, true) != req.length)
    {
        return req.SendSimpleResp(400, NULL, NULL, "Missing or invalid body of request.", 0);
    }
    std::string request_data(request_data_raw, req.length);
    bool found_grant_type = false;
    ssize_t validity = -1;
    std::string scope;
    std::string token;
    std::istringstream token_stream(request_data);
    while (std::getline(token_stream, token, '&'))
    {
        std::string::size_type eq = token.find("=");
        if (eq == std::string::npos)
        {
            return req.SendSimpleResp(400, NULL, NULL, "Invalid format for form-encoding", 0);
        }
        std::string key = token.substr(0, eq);
        std::string value = token.substr(eq + 1);
        //std::cout << "Found key " << key << ", value " << value << std::endl;
        if (key == "grant_type")
        {
            found_grant_type = true;
            if (value != "client_credentials")
            {
                return req.SendSimpleResp(400, NULL, NULL, "Invalid grant type specified.", 0);
            }
        }
        else if (key == "expire_in")
        {
            try
            {
                validity = std::stoll(value);
            }
            catch (...)
            {
                return req.SendSimpleResp(400, NULL, NULL, "Expiration request not parseable.", 0);
            }
            if (validity <= 0)
            {
                return req.SendSimpleResp(400, NULL, NULL, "Expiration request has invalid value.", 0);
            }
        }
        else if (key == "scope")
        {
            char *value_raw = unquote(value.c_str());
            if (value_raw == NULL)
            {
                return req.SendSimpleResp(400, NULL, NULL, "Unable to unquote scope.", 0);
            }
            scope = value_raw;
            free(value_raw);
        }
    }
    if (!found_grant_type)
    {
        return req.SendSimpleResp(400, NULL, NULL, "Grant type not specified.", 0);
    }
    if (scope.empty())
    {
        return req.SendSimpleResp(400, NULL, NULL, "Scope was not specified.", 0);
    }
    std::istringstream token_stream_scope(scope);
    std::string path;
    std::vector<std::string> other_caveats;
    while (std::getline(token_stream_scope, token, ' '))
    {
        std::string::size_type col = token.find(":");
        if (col == std::string::npos)
        {
            return req.SendSimpleResp(400, NULL, NULL, "Invalid format for requested scope", 0);
        }
        std::string key = token.substr(0, col);
        std::string value = token.substr(col + 1);
        //std::cout << "Found activity " << key << ", path " << value << std::endl;
        if (path.empty())
        {
            path = value;
        }
        else if (value != path)
        {
            std::stringstream ss;
            ss << "Encountered requested scope request for authorization " << key
               << " with resource path " << value << "; however, prior request had path "
               << path;
            m_log->Emsg("MacaroonRequest", ss.str().c_str());
            return req.SendSimpleResp(500, NULL, NULL, "Server only supports all scopes having the same path", 0);
        }
        other_caveats.push_back(key);
    }
    if (path.empty())
    {
        path = "/";
    }
    std::vector<std::string> other_caveats_final;
    if (!other_caveats.empty()) {
        std::stringstream ss;
        ss << "activity:";
        for (std::vector<std::string>::const_iterator iter = other_caveats.begin();
             iter != other_caveats.end();
             iter++)
        {
            ss << *iter << ",";
        }
        const std::string &final_str = ss.str();
        other_caveats_final.push_back(final_str.substr(0, final_str.size() - 1));
    }
    return GenerateMacaroonResponse(req, path, other_caveats_final, validity, true);
}


// Process a macaroon request.
int Handler::ProcessReq(XrdHttpExtReq &req)
{
    if (req.resource == "/.well-known/oauth-authorization-server") {
        return ProcessOAuthConfig(req);
    } else if (req.resource == "/.oauth2/token") {
        return ProcessTokenRequest(req);
    }

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

    // request_data is not necessarily null-terminated; hence, we use the more advanced _ex variant
    // of the tokener to avoid making a copy of the character buffer.
    char *request_data;
    if (req.BuffgetData(blen, &request_data, true) != blen)
    {
        return req.SendSimpleResp(400, NULL, NULL, "Missing or invalid body of request.", 0);
    }
    json_tokener *tokener = json_tokener_new();
    if (!tokener)
    {
        return req.SendSimpleResp(500, NULL, NULL, "Internal error when allocating token parser.", 0);
    }
    json_object *macaroon_req = json_tokener_parse_ex(tokener, request_data, blen);
    enum json_tokener_error err = json_tokener_get_error(tokener);
    json_tokener_free(tokener);
    if (err != json_tokener_success)
    {
        if (macaroon_req) json_object_put(macaroon_req);
        return req.SendSimpleResp(400, NULL, NULL, "Invalid JSON serialization of macaroon request.", 0);
    }
    json_object *validity_obj;
    if (!json_object_object_get_ex(macaroon_req, "validity", &validity_obj))
    {
        json_object_put(macaroon_req);
        return req.SendSimpleResp(400, NULL, NULL, "JSON request does not include a `validity`", 0);
    }
    const char *validity_cstr = json_object_get_string(validity_obj);
    if (!validity_cstr)
    {
        json_object_put(macaroon_req);
        return req.SendSimpleResp(400, NULL, NULL, "validity key cannot be cast to a string", 0);
    }
    std::string validity_str(validity_cstr);
    ssize_t validity = determine_validity(validity_str);
    if (validity <= 0)
    {
        json_object_put(macaroon_req);
        return req.SendSimpleResp(400, NULL, NULL, "Invalid ISO 8601 duration for validity key", 0);
    }
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
    json_object_put(macaroon_req);

    return GenerateMacaroonResponse(req, req.resource, other_caveats, validity, false);
}


int
Handler::GenerateMacaroonResponse(XrdHttpExtReq &req, const std::string &resource,
    const std::vector<std::string> &other_caveats, ssize_t validity, bool oauth_response)
{
    time_t now;
    time(&now);
    if (m_max_duration > 0)
    {
        validity = (validity > m_max_duration) ? m_max_duration : validity;
    }
    now += validity;

    char utc_time_buf[21];
    if (!strftime(utc_time_buf, 21, "%FT%TZ", gmtime(&now)))
    {
        return req.SendSimpleResp(500, NULL, NULL, "Internal error constructing UTC time", 0);
    }
    std::string utc_time_str(utc_time_buf);
    std::stringstream ss;
    ss << "before:" << utc_time_str;
    std::string utc_time_caveat = ss.str();

    std::string activities = GenerateActivities(req, resource);
    std::string macaroon_id = GenerateID(resource, req.GetSecEntity(), activities, other_caveats, utc_time_str);
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

    // Embed the SecEntity name, if present.
    struct macaroon *mac_with_name;
    const char * sec_name = req.GetSecEntity().name;
    if (sec_name) {
        std::stringstream name_caveat_ss;
        name_caveat_ss << "name:" << sec_name;
        std::string name_caveat = name_caveat_ss.str();
        mac_with_name = macaroon_add_first_party_caveat(mac,
                                                        reinterpret_cast<const unsigned char*>(name_caveat.c_str()),
                                                        name_caveat.size(),
                                                        &mac_err);
        macaroon_destroy(mac);
    } else {
        mac_with_name = mac;
    }
    if (!mac_with_name)
    {
        return req.SendSimpleResp(500, NULL, NULL, "Internal error adding default activities to macaroon", 0);
    }

    struct macaroon *mac_with_activities = macaroon_add_first_party_caveat(mac_with_name,
                                             reinterpret_cast<const unsigned char*>(activities.c_str()),
                                             activities.size(),
                                             &mac_err);
    macaroon_destroy(mac_with_name);
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

    std::string path_caveat = "path:" + resource;
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
                                        utc_time_caveat.size(),
                                        &mac_err);
    macaroon_destroy(mac_with_path);
    if (!mac_with_date) {
        return req.SendSimpleResp(500, NULL, NULL, "Internal error adding date to macaroon", 0);
    }

    size_t size_hint = macaroon_serialize_size_hint(mac_with_date);

    std::vector<char> macaroon_resp; macaroon_resp.reserve(size_hint);
    if (macaroon_serialize(mac_with_date, &macaroon_resp[0], size_hint, &mac_err))
    {
        printf("Returned macaroon_serialize code: %lu\n", (unsigned long)size_hint);
        return req.SendSimpleResp(500, NULL, NULL, "Internal error serializing macaroon", 0);
    }
    macaroon_destroy(mac_with_date);

    json_object *response_obj = json_object_new_object();
    if (!response_obj)
    {
        return req.SendSimpleResp(500, NULL, NULL, "Unable to create new JSON response object.", 0);
    }
    json_object *macaroon_obj = json_object_new_string_len(&macaroon_resp[0], strlen(&macaroon_resp[0]));
    if (!macaroon_obj)
    {
        return req.SendSimpleResp(500, NULL, NULL, "Unable to create a new JSON macaroon string.", 0);
    }
    json_object_object_add(response_obj, oauth_response ? "access_token" : "macaroon", macaroon_obj);

    json_object *expire_in_obj = json_object_new_int64(validity);
    if (!expire_in_obj)
    {
        return req.SendSimpleResp(500, NULL, NULL, "Unable to create a new JSON validity object.", 0);
    }
    json_object_object_add(response_obj, "expires_in", expire_in_obj);

    const char *macaroon_result = json_object_to_json_string_ext(response_obj, JSON_C_TO_STRING_PRETTY);
    int retval = req.SendSimpleResp(200, NULL, NULL, macaroon_result, 0);
    json_object_put(response_obj);
    return retval;
}
