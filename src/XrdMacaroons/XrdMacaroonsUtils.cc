#include "XrdMacaroonsUtils.hh"

#include <string>

std::string Macaroons::NormalizeSlashes(const std::string &input)
{
    std::string output;
      // In most cases, the output should be "about as large"
      // as the input
    output.reserve(input.size());
    char prior_chr = '\0';
    size_t output_idx = 0;
    for (size_t idx = 0; idx < input.size(); idx++) {
        char chr = input[idx];
        if (prior_chr == '/' && chr == '/') {
            output_idx++;
            continue;
        }
        output += input[output_idx];
        prior_chr = chr;
        output_idx++;
    }
    return output;
}

ssize_t Macaroons::determine_validity(const std::string& input)
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
