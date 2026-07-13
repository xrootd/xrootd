#ifndef __XRDOUCJSONUTILS_HH__
#define __XRDOUCJSONUTILS_HH__
/******************************************************************************/
/*                                                                            */
/*                    X r d O u c J s o n U t i l s . h h                     */
/*                                                                            */
/* (c) 2026 by the XRootD Collaboration                                       */
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
/******************************************************************************/

#include "XrdOuc/XrdOucJson.hh"

#include <cstdint>
#include <limits>
#include <type_traits>

namespace XrdOucJsonUtils
{
  template<typename Integer>
  bool GetOptionalUnsignedInteger(const nlohmann::json &json,
                                  const char *key,
                                  Integer &value,
                                  bool &present)
  {
    static_assert(std::is_integral_v<Integer>);
    static_assert(!std::is_same_v<Integer, bool>);

    present = false;
    const auto item = json.find(key);
    if(item == json.end()) return true;

    std::uint64_t parsed = 0;
    if(item->is_number_unsigned())
    {
      parsed = item->get<std::uint64_t>();
    }
    else if(item->is_number_integer())
    {
      const std::int64_t signedValue = item->get<std::int64_t>();
      if(signedValue < 0) return false;
      parsed = static_cast<std::uint64_t>(signedValue);
    }
    else
    {
      return false;
    }

    if(parsed > static_cast<std::uint64_t>(
         std::numeric_limits<Integer>::max()))
    {
      return false;
    }
    value = static_cast<Integer>(parsed);
    present = true;
    return true;
  }
}

#endif // __XRDOUCJSONUTILS_HH__
