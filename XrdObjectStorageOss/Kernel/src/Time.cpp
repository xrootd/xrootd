/*-----------------------------------------------------------------------------
Copyright(c) 2010 - 2018 ViSUS L.L.C.,
Scientific Computing and Imaging Institute of the University of Utah

ViSUS L.L.C., 50 W.Broadway, Ste. 300, 84101 - 2044 Salt Lake City, UT
University of Utah, 72 S Central Campus Dr, Room 3750, 84112 Salt Lake City, UT

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met :

* Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

* Neither the name of the copyright holder nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

For additional information about this project contact : pascucci@acm.org
For support : support@visus.net
-----------------------------------------------------------------------------*/

#include <Visus/Time.h>
#include "osdep.hxx"

#include <iomanip>

namespace Visus {


//////////////////////////////////////////////////////////////////////
static int extendedModulo (const Int64& value, const int& modulo) 
{return (int) (value >= 0 ? (value % modulo) : (value - ((value / modulo) + 1) * modulo));}


//==============================================================================
Time::Time (const int year,
            const int month,
            const int day,
            const int hours,
            const int minutes,
            const int seconds,
            const int milliseconds,
            const bool useLocalTime) 
{
  VisusAssert (year > 100); // year must be a 4-digit version

  if (year < 1971 || year >= 2038 || !useLocalTime)
  {
    // use extended maths for dates beyond 1970 to 2037..
    const int timeZoneAdjustment = useLocalTime ? (31536000 - (int) (Time (1971, 0, 1, 0, 0).getUTCMilliseconds() / 1000))
                                                : 0;
    const int a = (13 - month) / 12;
    const int y = year + 4800 - a;
    const int jd = day + (153 * (month + 12 * a - 2) + 2) / 5
                        + (y * 365) + (y /  4) - (y / 100) + (y / 400)
                        - 32045;

    const Int64 s = ((Int64) jd) * 86400LL - 210866803200LL;

    utc_msec = 1000 * (s + (hours * 3600 + minutes * 60 + seconds - timeZoneAdjustment))
                          + milliseconds;
  }
  else
  {
    struct tm t;
    t.tm_year   = year - 1900;
    t.tm_mon    = month;
    t.tm_mday   = day;
    t.tm_hour   = hours;
    t.tm_min    = minutes;
    t.tm_sec    = seconds;
    t.tm_isdst  = -1;

    utc_msec = 1000 * (Int64) mktime (&t);

    if (utc_msec < 0)
        utc_msec = 0;
    else
        utc_msec += milliseconds;
  }
}



//==============================================================================
Int64 Time::getTimeStamp() 
{
  return osdep::getTimeStamp();
}

//==============================================================================
  String Time::getFormattedLocalTime() const
  {
    std::ostringstream out;
    out<<std::setfill('0')
    <<std::setw(4)<<getYear()
    <<std::setw(2)<<(1+getMonth())
    <<std::setw(2)<<(getDayOfMonth())
    <<std::setw(2)<<(getHours())
    <<std::setw(2)<<(getMinutes())
    <<std::setw(2)<<(getSeconds())
    <<std::setw(3)<<(getMilliseconds());
    return out.str();
  }
  
  //==============================================================================
  String Time::getPrettyFormattedShortLocalTime() const
  {
    std::ostringstream out;
    out<<std::setfill('0')
    <<std::setw(4)<<getYear() <<"_"
    <<std::setw(2)<<(1+getMonth())<<"_"
    <<std::setw(2)<<(getDayOfMonth())<<"_"
    <<std::setw(2)<<(getHours())
    <<std::setw(2)<<(getMinutes());
    return out.str();
  }


//==============================================================================
static const char* const shortDayNames[] = { "Sun"   , "Mon"   , "Tue"    , "Wed"      , "Thu"     , "Fri"   , "Sat" };
static const char* const longDayNames[]  = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };

String Time::getWeekdayName(int dayNumber, bool threeLetterVersion)
{
  return threeLetterVersion ? shortDayNames [dayNumber % 7] : longDayNames [dayNumber % 7];
}

//==============================================================================
static const char* const shortMonthNames[] = { "Jan"    , "Feb"     , "Mar"  , "Apr"  , "May", "Jun" , "Jul" , "Aug"   , "Sep"      , "Oct"    , "Nov"     , "Dec" };
static const char* const longMonthNames[]  = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };

String Time::getMonthName(int monthNumber, bool threeLetterVersion)
{
  return threeLetterVersion ? shortMonthNames [monthNumber % 12] : longMonthNames [monthNumber % 12];
}

//==============================================================================
int Time::getYear() const           { return osdep::millisToLocal (utc_msec).tm_year + 1900; }
int Time::getMonth() const          { return osdep::millisToLocal (utc_msec).tm_mon; }
int Time::getDayOfYear() const      { return osdep::millisToLocal (utc_msec).tm_yday; }
int Time::getDayOfMonth() const     { return osdep::millisToLocal (utc_msec).tm_mday; }
int Time::getDayOfWeek() const      { return osdep::millisToLocal (utc_msec).tm_wday; }
int Time::getHours() const          { return osdep::millisToLocal (utc_msec).tm_hour; }
int Time::getMinutes() const        { return osdep::millisToLocal (utc_msec).tm_min; }
int Time::getSeconds() const        { return extendedModulo(utc_msec / 1000, 60); }
int Time::getMilliseconds() const   { return extendedModulo(utc_msec, 1000); }

} //namespace Visus

